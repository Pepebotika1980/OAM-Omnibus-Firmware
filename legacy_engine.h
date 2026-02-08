#pragma once
#include "daisysp.h"
#include <algorithm>
#include <cmath>

namespace oam {
namespace legacy {

inline float clamp(float x, float a, float b) {
  return std::max(a, std::min(b, x));
}

inline float s_mix(float x, float a, float b) { return x * (1 - x) + b * x; }

// --- Helpers from dsp.h ---
// (Assuming standard math usage).

struct LegacyHelpers {
  static float softClip(float x, float kneeStart = 0.9f,
                        float kneeCurve = 5.0f) {
    float linPart = clamp(x, -kneeStart, kneeStart);
    float clipPart = x - linPart;
    clipPart = atanf(clipPart * kneeCurve) / kneeCurve;
    return linPart + clipPart;
  }

  static float spread(float x, float s, float e = 2.5f) {
    s = clamp(s, 0.0f, 1.0f);
    if (s > 0.5f) {
      s = (s - 0.5f) * 2.0f;
      s = s * e + 1.0f;
      return 1.0f - powf(1.0f - x, s);
    } else if (s < 0.5f) {
      s = 1.0f - (s * 2.0f);
      s = s * e + 1.0f;
      return powf(x, s);
    } else {
      return x;
    }
  }

  static float minMaxSlider(float in, float dz = 0.002f) {
    in = in - dz * 0.5f;
    in = in * (1.0f + dz);
    return std::min(1.0f, std::max(0.0f, in));
  }

  static int seconds_to_samples(float x, float sr) { return (int)(x * sr); }
  static int wrap_buffer_index(int x, int size) {
    while (x >= size)
      x -= size;
    while (x < 0)
      x += size;
    return x;
  }
};

class Slew {
public:
  float lastVal = 0.0f;
  float coef = 0.001f;
  void Init(float c = 0.001f) { coef = c; }
  float Process(float x) {
    lastVal = lastVal + (x - lastVal) * coef;
    return lastVal;
  }
};

class Limiter {
public:
  float gainCoef;
  float releaseCoef;
  void Init(float sampleRate) {
    gainCoef = 1.0f;
    releaseCoef = 16.0f / sampleRate;
  }
  float Process(float in) {
    float targetGainCoef = 1.0f / std::max(fabsf(in), 1.0f);
    if (targetGainCoef < gainCoef) {
      gainCoef = targetGainCoef;
    } else {
      gainCoef = gainCoef * (1.0f - releaseCoef) + targetGainCoef * releaseCoef;
    }
    return in * gainCoef;
  }
};

class LoudnessDetector {
public:
  Slew slew;
  float lastVal = 0;
  void Init() { slew.Init(); }
  float Get() { return this->lastVal; }
  float Process(float x) {
    lastVal = slew.Process(fabsf(x));
    return x;
  }
};

class ReadHead {
public:
  LoudnessDetector loudness;
  float *buffer;
  int bufferSize;
  float delayA = 0.0f, delayB = 0.0f, targetDelay = -1.0f;
  float ampA = 0.0f, ampB = 0.0f, targetAmp = -1.0f;
  float sampleRate;
  float phase = 1.0f;
  float delta;
  float blurAmount;

  void Init(float sr, float *buf, int size) {
    sampleRate = sr;
    delta = 5.0f / sr;
    buffer = buf;
    bufferSize = size;
    blurAmount = 0.0f;
    loudness.Init();
  }

  void Set(float delay, float amp, float blur = 0) {
    targetDelay = delay;
    targetAmp = amp;
    blurAmount = blur;
  }

  float Process(float writeHeadPosition) {
    if (phase >= 1.0f && (targetDelay >= 0.0f || targetAmp >= 0.0f)) {
      if (targetDelay >= 0.0f) {
        delayA = delayB;
        delayB = targetDelay;
        targetDelay = -1.0f;
      }
      if (targetAmp >= 0.0f) {
        ampA = ampB;
        ampB = targetAmp;
        targetAmp = -1.0f;
      }
      phase = 0.0f;
      // Simple random for blur
      float r = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
      delta = (5.0f + (r * blurAmount)) / sampleRate;
    }

    int idxA = LegacyHelpers::wrap_buffer_index(
        writeHeadPosition -
            LegacyHelpers::seconds_to_samples(delayA, sampleRate),
        bufferSize);
    int idxB = LegacyHelpers::wrap_buffer_index(
        writeHeadPosition -
            LegacyHelpers::seconds_to_samples(delayB, sampleRate),
        bufferSize);

    float outA = buffer[idxA];
    float outB = buffer[idxB];

    float output = ((1.0f - phase) * outA) + (phase * outB);
    float outputAmp = ((1.0f - phase) * ampA) + (phase * ampB);

    phase = phase <= 1.0f ? phase + delta : 1.0f;
    return loudness.Process(output) * outputAmp;
  }
};

class LegacyMonoEngine {
public:
  ReadHead readHeads[8];
  LoudnessDetector loudness;
  float sampleRate;
  float *buffer;
  int bufferSize;
  int writeHeadPosition;
  float dryAmp, feedback, blur;

  Slew dryAmpSlew, feedbackSlew, ampCoefSlew;
  Limiter outputLimiter, feedbackLimiter;
  daisysp::Compressor compressor;
  // Skipping DC Blocker for simplicity/size, compressor handles dynamics

  void Init(float sr, float maxDelay, float *buf) {
    sampleRate = sr;
    bufferSize = LegacyHelpers::seconds_to_samples(maxDelay, sr);
    buffer = buf;
    for (int i = 0; i < bufferSize; i++)
      buffer[i] = 0.0f;
    for (int i = 0; i < 8; i++)
      readHeads[i].Init(sr, buffer, bufferSize);
    writeHeadPosition = 0;

    dryAmpSlew.Init();
    feedbackSlew.Init(0.01f);
    ampCoefSlew.Init(0.0001f);
    outputLimiter.Init(sr);
    feedbackLimiter.Init(sr);
    loudness.Init();

    compressor.Init(sr);
    compressor.SetAttack(0.02f);
    compressor.SetRelease(0.2f);
    compressor.SetRatio(5.0f);
    compressor.SetThreshold(0.0f); // 0dB? Original was 0.0.
  }

  void Set(float d, float f, float b) {
    dryAmp = d;
    feedback = f;
    blur = b;
  }

  float Process(float in) {
    float out = 0.0f;
    float ampCoef = 0.0f;
    for (int i = 0; i < 8; i++) {
      // Approximate targetAmp access
      ampCoef += readHeads[i].ampB; // close enough
    }
    ampCoef = ampCoefSlew.Process(1.0f / std::max(1.0f, ampCoef));

    buffer[writeHeadPosition] = loudness.Process(in);

    for (int i = 0; i < 8; i++)
      out += readHeads[i].Process((float)writeHeadPosition);

    // Compressor sidechaining to input?
    out = compressor.Process(out, buffer[writeHeadPosition] + out);

    float fb_val = buffer[writeHeadPosition] +
                   (out * feedbackSlew.Process(feedback) * ampCoef);
    buffer[writeHeadPosition] = -feedbackLimiter.Process(fb_val);

    float final_out =
        outputLimiter.Process(out + in * dryAmpSlew.Process(dryAmp));

    writeHeadPosition++;
    if (writeHeadPosition >= bufferSize)
      writeHeadPosition = 0;

    return final_out;
  }
};

class LegacyStereoEngine {
public:
  LegacyMonoEngine left, right;
  float time_val;

  void Init(float sr, float *bufL, float *bufR) {
    // Hardcoded 150s max?
    // With 64MB split by 2, we have 32MB per ch. 32MB / 4 bytes = 8M samples.
    // 8M / 48k = 166 seconds. Fits.
    left.Init(sr, 150.0f, bufL);
    right.Init(sr, 150.0f, bufR);
  }

  // Call this once per block with control values
  void UpdateControls(float time_knob, float skew_knob, float fb_knob,
                      float dry_slider, const float *sliders,
                      const float *vcas) {
    // Logic adapted from original main.cpp
    // Skew -> Distribution
    float distribution = skew_knob;
    float time =
        time_knob * 150.0f; // Linear mapping simplification for Omnibus

    // Feedback map
    float feedback = fb_knob * 3.0f; // 0 to 3

    left.Set(dry_slider * vcas[0], feedback,
             feedback); // Blur = feedback (from original logic)
    right.Set(dry_slider * vcas[0], feedback, feedback);

    for (int i = 1; i < 9; i++) {
      float slider_val = sliders[i - 1];
      float vca_val = vcas[i];
      float amp = LegacyHelpers::minMaxSlider(
          (1.0f - slider_val) * vca_val); // Inverting? checks original
      // Original: minMaxSlider((1.0f - hw.GetSliderValue(i)) *
      // hw.GetVcaValue(i)) Assuming passed sliders are raw 0..1 from
      // hw.GetSliderValue

      float t = LegacyHelpers::spread((i / 8.0f), distribution) * time;
      left.readHeads[i - 1].Set(t, amp, std::max(0.0f, feedback - 1.0f));
      right.readHeads[i - 1].Set(t, amp, std::max(0.0f, feedback - 1.0f));
    }
  }

  void ProcessBlock(const float *inL, const float *inR, float *outL,
                    float *outR, size_t size) {
    for (size_t i = 0; i < size; i++) {
      outL[i] = left.Process(inL[i]);
      outR[i] = right.Process(inR[i]);
    }
  }
};

} // namespace legacy
} // namespace oam
