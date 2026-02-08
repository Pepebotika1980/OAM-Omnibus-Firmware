#pragma once
#include "daisysp.h"
#include <cmath>

using namespace daisysp;

// Helper Classes
class OmniDelay {
public:
  void Init(float *buf, int max) {
    buffer_ = buf;
    max_len_ = max;
    write_ptr_ = 0;
  }
  void Write(float sample) {
    buffer_[write_ptr_] = sample;
    write_ptr_++;
    if (write_ptr_ >= max_len_)
      write_ptr_ = 0;
  }
  float Read(float delay_samps) {
    // Linear Interpolation
    float read_pos = (float)write_ptr_ - delay_samps;
    while (read_pos < 0.0f)
      read_pos += (float)max_len_;
    while (read_pos >= (float)max_len_)
      read_pos -= (float)max_len_;

    int idx = (int)read_pos;
    float frac = read_pos - idx;
    int idx2 = idx + 1;
    if (idx2 >= max_len_)
      idx2 = 0;

    return buffer_[idx] + frac * (buffer_[idx2] - buffer_[idx]);
  }

private:
  float *buffer_;
  int max_len_;
  int write_ptr_;
};

class OmniAllpass {
public:
  void Init() {
    for (int i = 0; i < 600; i++)
      buffer_[i] = 0.0f;
    write_ptr_ = 0;
    delay_len_ = 100;
  }
  void SetDelay(int len) {
    delay_len_ = len;
    if (len > 599)
      delay_len_ = 599;
  }
  float Process(float in) {
    int read_ptr = write_ptr_ - delay_len_;
    if (read_ptr < 0)
      read_ptr += 600;
    float buf_out = buffer_[read_ptr];
    float out = -in + buf_out;
    buffer_[write_ptr_] = in + (0.5f * buf_out);
    write_ptr_++;
    if (write_ptr_ >= 600)
      write_ptr_ = 0;
    return out;
  }

private:
  float buffer_[600];
  int write_ptr_;
  int delay_len_;
};

class OmniOnePole {
public:
  void Init() {
    out_ = 0.0f;
    SetFreq(6000.0f);
  }
  void SetFreq(float freq) {
    float b1 = expf(-2.0f * 3.1415927f * freq / 48000.0f);
    b1_ = b1;
    a0_ = 1.0f - b1;
  }
  float Process(float in) {
    out_ = (in * a0_) + (out_ * b1_);
    return out_;
  }

private:
  float a0_, b1_, out_;
};

enum FdnMode { MODE_STUDIO, MODE_SHIMMER, MODE_MASSIVE };

template <int N_LINES = 8> class UberFDN {
public:
  void Init(float sample_rate, float *big_buffer) {
    sample_rate_ = sample_rate;
    // manually assign chunks
    for (int i = 0; i < N_LINES; i++) {
      // 240,000 floats each
      delays_[i].Init(&big_buffer[i * 240000], 240000);
    }
    mode_ = MODE_STUDIO;

    int diff_lens[4] = {225, 341, 441, 556};
    for (int i = 0; i < 4; i++) {
      diffusers_[i].Init();
      diffusers_[i].SetDelay(diff_lens[i]);
    }

    // Init Shimmers
    for (int i = 0; i < 2; i++) {
      shimmers_[i].Init(sample_rate);
      shimmers_[i].SetTransposition(12.0f);
      shimmers_[i].SetDelSize(1600);
    }

    // Init Modulators & Filters
    for (int i = 0; i < N_LINES; i++) {
      // LFO for Studio/Shimmer
      lfo_[i].Init(sample_rate);
      lfo_[i].SetWaveform(Oscillator::WAVE_SIN);
      lfo_[i].SetAmp(1.0f);
      lfo_[i].SetFreq(0.1f + (i * 0.05f));

      // Wander LFOs for Massive
      wander1_[i].Init(sample_rate);
      wander1_[i].SetFreq(0.1f + (i * 0.03f));
      wander1_[i].SetAmp(0.5f);
      wander2_[i].Init(sample_rate);
      wander2_[i].SetFreq(0.07f + (i * 0.041f));
      wander2_[i].SetAmp(0.3f);

      // Damping (OnePole for Studio/Shimmer)
      damp_lpf_[i].Init();

      // Resonators (SVF for Massive)
      resonators_[i].Init(sample_rate);
      resonators_[i].SetRes(0.1f);
    }

    master_decay_ = 0.5f;
  }

  void SetMode(FdnMode m) { mode_ = m; }

  void ProcessBlock(const float *in_l, const float *in_r, float *out_l,
                    float *out_r, size_t size, const float *gains,
                    float size_param, float skew, float warp) {
    // Parameter setup based on mode
    float depth = 10.0f;
    if (mode_ == MODE_MASSIVE)
      depth = 100.0f; // Massive drift

    // Shimmer Setup
    float shift_mix = 0.0f;
    if (mode_ == MODE_SHIMMER) {
      // Sliders 7 & 8 control shimmer mix indirectly via code logic?
      // In Shimmer mode, sliders control feedback. We apply shimmer fixed on
      // lines 6/7. Let's hardcode effect for now or use Warp knob? Original
      // Shimmer used sliders 7/8 for feedback of shimmer lines. Here we use
      // Fixed behavior.
      shimmers_[0].SetTransposition(12.0f);
      shimmers_[1].SetTransposition(12.0f);
      shift_mix = 1.0f; // Always active on specific lines
    } else if (mode_ == MODE_MASSIVE) {
      // Massive logic from before
      if (warp > 0.6f) {
        shift_mix = (warp - 0.6f) * 2.5f;
        shimmers_[0].SetTransposition(warp > 0.85f ? 19.0f : 12.0f);
        shimmers_[1].SetTransposition(warp > 0.85f ? 19.02f : 12.02f);
      } else {
        // Detune only
        shimmers_[0].SetTransposition((warp - 0.2f) * 2.0f);
        shimmers_[1].SetTransposition((warp - 0.2f) * 2.0f + 0.02f);
        shift_mix = (warp < 0.4f) ? 0.5f : 0.0f;
      }
    }

    for (size_t i = 0; i < size; i++) {
      float input = (in_l[i] + in_r[i]) * 0.5f;
      float diffused = input;

      // Diffusion
      for (int k = 0; k < 4; k++)
        diffused = diffusers_[k].Process(diffused);

      // Read
      float delay_outs[N_LINES];
      for (int k = 0; k < N_LINES; k++) {
        // Mod
        float mod_val = 0.0f;
        if (mode_ == MODE_MASSIVE) {
          mod_val = wander1_[k].Process() + wander2_[k].Process();
        } else {
          mod_val = lfo_[k].Process();
        }

        // Delay Time
        float ratio = base_ratios_[k];
        float s = powf(ratio, 0.5f + skew);
        float base_t = s * size_param * sample_rate_ * 0.15f;
        if (base_t > 230000)
          base_t = 230000;

        float final_t = base_t + (mod_val * depth);
        delay_outs[k] = delays_[k].Read(final_t);
      }

      // Mix
      float sum = 0.0f;
      for (int k = 0; k < N_LINES; k++)
        sum += delay_outs[k];
      sum *= 0.25f;

      float matrix_out[N_LINES];
      for (int k = 0; k < N_LINES; k++)
        matrix_out[k] = delay_outs[k] - sum;

      // Feedback
      for (int k = 0; k < N_LINES; k++) {
        float fb = gains[k] * master_decay_;
        if (fb > 0.99f)
          fb = 0.99f;
        if (mode_ == MODE_MASSIVE && master_decay_ > 0.98f)
          fb = 1.0f;

        float next = matrix_out[k] * fb;
        if (mode_ != MODE_MASSIVE || master_decay_ <= 0.98f)
          next += diffused * 0.25f;

        // Tone Shaping
        if (mode_ == MODE_MASSIVE) {
          float freq = 80.0f * powf(2.0f, k); // Octaves
          resonators_[k].SetFreq(freq);
          resonators_[k].SetRes(0.1f + (gains[k] * 0.7f));
          resonators_[k].Process(next);
          next = (resonators_[k].Low() * 0.5f) + (resonators_[k].Band() * 0.8f);
        } else {
          // Studio/Shimmer uses LPF
          float co = 2000.0f + (gains[k] * 8000.0f);
          damp_lpf_[k].SetFreq(co);
          next = damp_lpf_[k].Process(next);
        }

        // Shimmer Logic
        if (mode_ == MODE_SHIMMER) {
          if (k == 6 || k == 7) {
            float s = shimmers_[k - 6].Process(next);
            // Mix 50/50
            next = (next * 0.5f) + (s * 0.5f);
          }
        } else if (mode_ == MODE_MASSIVE && shift_mix > 0.0f) {
          if (k == 3)
            next = (next * (1.0f - shift_mix)) +
                   (shimmers_[0].Process(next) * shift_mix);
          if (k == 7)
            next = (next * (1.0f - shift_mix)) +
                   (shimmers_[1].Process(next) * shift_mix);
        }

        next = SoftLimit(next);
        delays_[k].Write(next);
      }

      // Output
      float l = delay_outs[0] - delay_outs[2] + delay_outs[4] - delay_outs[6];
      float r = delay_outs[1] - delay_outs[3] + delay_outs[5] - delay_outs[7];
      out_l[i] = l * 0.25f;
      out_r[i] = r * 0.25f;
    }
  }

  void SetDecay(float d) { master_decay_ = d; }

private:
  float sample_rate_;
  OmniDelay delays_[N_LINES];
  OmniAllpass diffusers_[4];

  // Shared LFOs? No, keep separate for character
  Oscillator lfo_[N_LINES];
  Oscillator wander1_[N_LINES];
  Oscillator wander2_[N_LINES];

  OmniOnePole damp_lpf_[N_LINES];
  Svf resonators_[N_LINES];

  PitchShifter shimmers_[2];

  float master_decay_;
  FdnMode mode_;

  const float base_ratios_[8] = {1.000f, 1.137f, 1.289f, 1.458f,
                                 1.632f, 1.815f, 2.053f, 2.311f};

  float SoftLimit(float x) {
    return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
  }
};
