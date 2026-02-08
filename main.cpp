#include "daisysp.h"
#include "legacy_engine.h"
#include "omni_resonator.h"
#include "time_machine_hardware.h"
#include "uber_fdn.h"

using namespace daisy;
using namespace daisysp;
using namespace oam::time_machine;

// --- Hardware ---
TimeMachineHardware hw;

// --- Memory ---
// 60MB Buffer for Legacy Mode (or Shared use)
// 150 seconds * 48000 * 2 channels = 14.4M samples = 57.6MB
#define TOTAL_SDRAM_SAMPLES 15000000
float DSY_SDRAM_BSS big_sdram_buffer[TOTAL_SDRAM_SAMPLES];

// Pointers for FDN (reusing the start of the big buffer)
DelayLine<float, 240000> delay_lines[8];

// --- Engines ---
UberFDN<8> fdn_engine;
OmniResonatorEngine res_engine;
oam::legacy::LegacyStereoEngine legacy_engine;

// --- State ---
enum AppMode {
  APP_STUDIO,
  APP_SHIMMER,
  APP_MASSIVE,
  APP_RESONATOR,
  APP_LEGACY
};
AppMode current_mode = APP_STUDIO;

// Controls
float gains[8];
float vcas[9] = {1, 1, 1, 1, 1, 1, 1, 1, 1}; // Dummy for legacy
float sliders_raw[8];
float dry_mix;

// Global Control Vars
float k_time, k_mod, k_decay;

void AudioCallbackReal(AudioHandle::InputBuffer in,
                       AudioHandle::OutputBuffer out, size_t size) {
  const float *in_l = in[0];
  const float *in_r = in[1];
  float *out_l = out[0];
  float *out_r = out[1];

  if (current_mode == APP_RESONATOR) {
    res_engine.ProcessBlock(in_l, in_r, out_l, out_r, size, gains, k_time,
                            k_mod, k_decay);
    for (size_t i = 0; i < size; i++) {
      out[0][i] = (out_l[i] * (1.0f - dry_mix)) + (in[0][i] * dry_mix);
      out[1][i] = (out_r[i] * (1.0f - dry_mix)) + (in[1][i] * dry_mix);
    }
  } else if (current_mode == APP_LEGACY) {
    // Update legacy controls block-rate (or frame rate, acceptable)
    // Note: For best quality we'd do it per sample but legacy impl did it per
    // blockish We defer control update to main loop, passed via globals? We'll
    // trust the main loop called UpdateControls.
    legacy_engine.ProcessBlock(in_l, in_r, out_l, out_r, size);
    for (size_t i = 0; i < size; i++) {
      out[0][i] = out_l[i];
      out[1][i] = out_r[i];
    }
  } else {
    // FDN Modes
    fdn_engine.ProcessBlock(in_l, in_r, out_l, out_r, size, gains,
                            0.2f + (k_time * 3.0f), 0.5f, k_mod);

    for (size_t i = 0; i < size; i++) {
      out[0][i] = (out_l[i] * (1.0f - dry_mix)) + (in[0][i] * dry_mix);
      out[1][i] = (out_r[i] * (1.0f - dry_mix)) + (in[1][i] * dry_mix);
    }
  }
}

int main(void) {
  hw.Init();
  hw.SetAudioBlockSize(32); // Slight optimization
  float samplerate = hw.AudioSampleRate();

  // 1. Initial Control Read for Mode Selection
  hw.ProcessAllControls();
  float selector = hw.GetSliderValue(1); // Slider 1

  // Check range - Updated for 5 modes
  // 0-20, 20-40, 40-60, 60-80, 80-100
  if (selector < 0.2f) {
    current_mode = APP_STUDIO;
    fdn_engine.SetMode(MODE_STUDIO);
  } else if (selector < 0.4f) {
    current_mode = APP_SHIMMER;
    fdn_engine.SetMode(MODE_SHIMMER);
  } else if (selector < 0.6f) {
    current_mode = APP_MASSIVE;
    fdn_engine.SetMode(MODE_MASSIVE);
  } else if (selector < 0.8f) {
    current_mode = APP_RESONATOR;
  } else {
    current_mode = APP_LEGACY;
  }

  // Blink LED to confirm Mode
  int blinks = (int)current_mode + 1;
  for (int i = 0; i < blinks; i++) {
    hw.SetLed(true);
    hw.Delay(150);
    hw.SetLed(false);
    hw.Delay(150);
  }

  // 2. Engine Init
  if (current_mode == APP_LEGACY) {
    // Split big buffer into two halves
    float *bufL = &big_sdram_buffer[0];
    float *bufR = &big_sdram_buffer[7500000]; // Halfway
    legacy_engine.Init(samplerate, bufL, bufR);
  } else if (current_mode == APP_RESONATOR) {
    res_engine.Init(samplerate);
  } else {
    // FDN Init - Pass the start of the big buffer
    fdn_engine.Init(samplerate, &big_sdram_buffer[0]);
  }

  // RE-FIXING FDN BUFFER ALLOCATION
  // We cannot instantiate `DelayLine<float, 240000>` if we also have
  // `big_sdram_buffer[15000000]`. We need to use `DelayLine<float, 1>` (dummy)
  // and `SetDelay` with external buffer? DaisySP `DelayLine` implementation: If
  // we use default constructor with <MaxDelay>, it has `float line_[MaxDelay]`.
  // We must switch FDN to use Pointers.

  if (current_mode != APP_RESONATOR && current_mode != APP_LEGACY) {
    // Manual assignments
    // Requires modification to UberFDN to accept raw pointers or DelayLine
    // objects initialized with pointers. Let's assume UberFDN Init logic needs
    // update. Actually, let's cast segments of `big_sdram_buffer` into
    // something FDN can use. Or just modify UberFDN to take `mid_buffer`? NO
    // efficiently: Change `delay_lines` to `DelayLine<float, MAX_DELAY>
    // *delay_lines`? No, standard `DelayLine` class owns its memory. We should
    // use `DelayLine<float, 1>`? No that limits max delay? DaisySP `DelayLine`
    // template param `max_delay` defines the array size.

    // OK, Plan B:
    // The `big_sdram_buffer` IS the memory.
    // For Legacy: we use it as 2 huge arrays.
    // For FDN: we cast start of it to 8 distinct arrays.
    // UberFDN expects `DelayLine<float, 240000>*`.
    // This is incompatible with raw float pointers.
    // I will modify `UberFDN` to use `float*` buffers directly + a minimal
    // Delay helper.
  }

  hw.StartAudio(AudioCallbackReal);

  while (1) {
    hw.ProcessAllControls();

    // --- Read Controls (Knob + CV) ---
    // Knobs
    float raw_time_k = hw.GetAdcValue(patch_sm::ADC_10);
    float raw_skew_k = hw.GetAdcValue(patch_sm::ADC_9);
    float raw_fb_k = hw.GetAdcValue(patch_sm::CV_8);

    // CVs (Summing)
    float raw_time_cv = hw.GetAdcValue(patch_sm::CV_2);
    float raw_skew_cv = hw.GetAdcValue(patch_sm::CV_1);
    float raw_fb_cv = hw.GetAdcValue(patch_sm::CV_3);

    // Combine & Clamp
    k_time = raw_time_k + raw_time_cv;
    if (k_time < 0.0f)
      k_time = 0.0f;
    if (k_time > 1.0f)
      k_time = 1.0f;

    k_mod = raw_skew_k + raw_skew_cv;
    if (k_mod < 0.0f)
      k_mod = 0.0f;
    if (k_mod > 1.0f)
      k_mod = 1.0f;

    k_decay = raw_fb_k + raw_fb_cv;
    if (k_decay < 0.0f)
      k_decay = 0.0f;
    if (k_decay > 1.0f)
      k_decay = 1.0f; // Note: Legacy engine might expect >1.0 for self-osc?
    // Legacy engine multiplies feedback by 3.0 internally in UpdateControls, so
    // 0..1 input is correct.

    dry_mix = hw.GetSliderValue(0);

    for (int i = 0; i < 8; i++) {
      sliders_raw[i] = hw.GetSliderValue(i + 1);
      gains[i] = sliders_raw[i];
      vcas[i + 1] = 1.0f;
    }
    vcas[0] = 1.0f;

    // --- Update Engines ---
    if (current_mode == APP_LEGACY) {
      legacy_engine.UpdateControls(k_time, k_mod, k_decay, dry_mix, sliders_raw,
                                   vcas);
    } else if (current_mode != APP_RESONATOR) {
      // FDN Modes
      float safe_decay = k_decay;
      if (current_mode != APP_MASSIVE) {
        safe_decay *= 0.98f; // Limit feedback for non-massive modes
      }
      fdn_engine.SetDecay(safe_decay);
    }

    hw.SetLed(System::GetNow() & 1024);
    hw.Delay(4);
  }
}
