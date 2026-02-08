#pragma once
#include "daisysp.h"
#include <cmath>

using namespace daisysp;

class OmniResonatorVoice {
public:
  void Init(float sr) {
    sr_ = sr;
    svf_.Init(sr);
    freq_ = 440.0f;
    res_ = 0.5f;
  }

  float Process(float in) {
    svf_.SetFreq(freq_);
    svf_.SetRes(res_);
    svf_.Process(in);
    return svf_.Band();
  }
  void SetFreq(float f) { freq_ = f; }
  void SetRes(float r) { res_ = r; }

private:
  float sr_, freq_, res_;
  Svf svf_;
};

class OmniResonatorEngine {
public:
  void Init(float sample_rate) {
    sr_ = sample_rate;
    for (int i = 0; i < 8; i++) {
      voices_l_[i].Init(sr_);
      voices_r_[i].Init(sr_);
    }
    root_freq_ = 110.0f;
  }

  void ProcessBlock(const float *in_l, const float *in_r, float *out_l,
                    float *out_r, size_t size, const float *harmonic_gains,
                    float note_cv, float structure, float damping) {
    float midi_note = 36.0f + (note_cv * 60.0f);
    midi_note = floorf(midi_note + 0.5f);
    root_freq_ = mtof(midi_note);
    UpdateRatios(structure);

    float t_damp = damping * damping;
    float res_val = 0.80f + (t_damp * 0.1995f);

    for (size_t i = 0; i < size; i++) {
      float input = (in_l[i] + in_r[i]) * 0.5f;
      static float prev = 0.0f;
      float exciter = input - prev;
      prev = input;
      exciter = exciter * 4.0f; // Boost

      float sum_l = 0.0f, sum_r = 0.0f;

      for (int k = 0; k < 8; k++) {
        float f = root_freq_ * ratios_[k];
        if (f > 16000.0f)
          f = 16000.0f;
        float detune = 1.0f + (0.01f * (k % 2 == 0 ? 1 : -1));

        voices_l_[k].SetFreq(f);
        voices_r_[k].SetFreq(f * detune);
        voices_l_[k].SetRes(res_val);
        voices_r_[k].SetRes(res_val);

        sum_l += voices_l_[k].Process(exciter * harmonic_gains[k]);
        sum_r += voices_r_[k].Process(exciter * harmonic_gains[k]);
      }
      out_l[i] = sum_l * 0.8f;
      out_r[i] = sum_r * 0.8f;
    }
  }

private:
  float sr_;
  OmniResonatorVoice voices_l_[8];
  OmniResonatorVoice voices_r_[8];
  float root_freq_;
  float ratios_[8];

  void UpdateRatios(float structure) {
    for (int i = 0; i < 8; i++) {
      float h = (float)(i + 1);
      float odd = 1.0f + (i * 2.0f);
      float inharm = 1.0f + (i * 1.5f) + (sinf(i * 34.0f) * 0.5f);

      if (structure < 0.5f) {
        float t = structure * 2.0f;
        ratios_[i] = (h * (1.0f - t)) + (odd * t);
      } else {
        float t = (structure - 0.5f) * 2.0f;
        ratios_[i] = (odd * (1.0f - t)) + (inharm * t);
      }
    }
  }
};
