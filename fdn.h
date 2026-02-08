#pragma once
#include "daisysp.h"
#include <cmath>

using namespace daisysp;

/**
 * Super FDN Reverb Engine
 * Features:
 * - 8-Line Feedback Delay Network
 * - Input Diffusion (4-stage Allpass)
 * - Internal Modulation (Chorus/Ensemble)
 * - Hermite Interpolation for smooth scrubbing
 * - Per-line Feedback Control (Spectral Shaping)
 * - One-Pole Damping Filters
 */

// Helper Class for Input Diffusion
class SimpleAllpass {
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
    // Standard Schroeder Allpass:
    // out = -g * in + buf[read]
    // buf[write] = in + g * buf[read]

    int read_ptr = write_ptr_ - delay_len_;
    if (read_ptr < 0)
      read_ptr += 600;

    float buf_out = buffer_[read_ptr];
    float g = 0.5f; // Fixed gain for diffusion

    float out = -in + buf_out;
    buffer_[write_ptr_] = in + (g * buf_out);

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

// Buffers defined in main.cpp to use SDRAM
// We just refer to them via pointers here
template <int N_LINES = 8> class SuperFDN {
public:
  SuperFDN() {}
  ~SuperFDN() {}

  void Init(float sample_rate, DelayLine<float, 240000> *delays) {
    sample_rate_ = sample_rate;
    delays_ = delays; // Pointer to SDRAM array

    // Initialize Diffusers (Input Allpasses)
    // Values chosen to smear transients without ringing
    int diff_lens[4] = {225, 341, 441, 556};
    for (int i = 0; i < 4; i++) {
      diffusers_[i].Init();
      diffusers_[i].SetDelay(diff_lens[i]);
    }

    // Initialize LFOs for Modulation
    for (int i = 0; i < N_LINES; i++) {
      mod_lfos_[i].Init(sample_rate);
      mod_lfos_[i].SetWaveform(Oscillator::WAVE_SIN);
      mod_lfos_[i].SetAmp(1.0f);
      float rate = 0.1f + (i * 0.05f); // 0.1Hz to 0.5Hz spread
      mod_lfos_[i].SetFreq(rate);

      // Damping filters (DaisySP OnePole)
      damp_filters_[i].Init();
      // SetFrequency expects normalized freq (0..0.5)
      damp_filters_[i].SetFrequency(6000.0f / sample_rate_);
    }

    master_decay_ = 0.5f;
    mod_depth_ = 10.0f; // Samples
  }

  void ProcessBlock(const float *in_l, const float *in_r, float *out_l,
                    float *out_r, size_t size, const float *line_gains,
                    float time_scale, float skew) {
    for (size_t i = 0; i < size; i++) {
      float dry_l = in_l[i];
      float dry_r = in_r[i];
      float input_mix = (dry_l + dry_r) * 0.5f;

      // 1. Input Diffusion (Smear Transients)
      float diffused = input_mix;
      for (int k = 0; k < 4; k++) {
        diffused = diffusers_[k].Process(diffused);
      }

      // 2. FDN Read & Modulate
      float delay_outs[N_LINES];
      for (int k = 0; k < N_LINES; k++) {
        // Calculate Target Delay
        float ratio = base_ratios_[k];
        float skewed_ratio = powf(ratio, 0.5f + skew);

        float base_samps = skewed_ratio * time_scale * sample_rate_ * 0.1f;
        // Clamp limits
        if (base_samps > 230000.0f)
          base_samps = 230000.0f;
        if (base_samps < 100.0f)
          base_samps = 100.0f;

        // LFO Mod
        float mod = mod_lfos_[k].Process();
        float final_delay = base_samps + (mod * mod_depth_);

        // Hermite Interpolation Read
        delay_outs[k] = delays_[k].Read(final_delay);
      }

      // 3. Matrix Mixing (Householder)
      // y = x - (2/N)*sum(x)
      float sum = 0.0f;
      for (int k = 0; k < N_LINES; k++)
        sum += delay_outs[k];
      sum *= (2.0f / (float)N_LINES);

      float matrix_out[N_LINES];
      for (int k = 0; k < N_LINES; k++) {
        matrix_out[k] = delay_outs[k] - sum;
      }

      // 4. Feedback, Filter, Write
      for (int k = 0; k < N_LINES; k++) {
        float fb_gain = line_gains[k] * master_decay_;
        if (fb_gain > 0.99f)
          fb_gain = 0.99f;

        // Injection + Feedback
        float next_in = (diffused * 0.25f) + (matrix_out[k] * fb_gain);

        // Damping Filter (LPF)
        // Sliders affect filter freq: higher gain slider = brighter
        // Range 2kHz to 10kHz
        float cutoff = 2000.0f + (line_gains[k] * 8000.0f);
        damp_filters_[k].SetFrequency(cutoff / sample_rate_);

        next_in = damp_filters_[k].Process(next_in);

        // Soft Limiting
        next_in = fclamp(next_in, -2.0f, 2.0f);
        next_in = SoftLimit(next_in);

        delays_[k].Write(next_in);
      }

      // 5. Output Mix (Stereo Decorrelation)
      // Left = Sum(Odds), Right = Sum(Evens)
      float out_l_accum = 0.0f;
      float out_r_accum = 0.0f;

      // L: 0, 2, 4, 6
      out_l_accum =
          delay_outs[0] - delay_outs[2] + delay_outs[4] - delay_outs[6];
      // R: 1, 3, 5, 7
      out_r_accum =
          delay_outs[1] - delay_outs[3] + delay_outs[5] - delay_outs[7];

      out_l[i] = out_l_accum * 0.25f;
      out_r[i] = out_r_accum * 0.25f;
    }
  }

  void SetMasterDecay(float decay) { master_decay_ = decay; }

private:
  float sample_rate_;
  DelayLine<float, 240000> *delays_; // Reference to SDRAM
  SimpleAllpass diffusers_[4];

  Oscillator mod_lfos_[N_LINES];
  OnePole damp_filters_[N_LINES];

  float master_decay_;
  float mod_depth_;

  const float base_ratios_[8] = {1.000f, 1.137f, 1.289f, 1.458f,
                                 1.632f, 1.815f, 2.053f, 2.311f};

  float SoftLimit(float x) {
    return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
  }

  float fclamp(float in, float min, float max) {
    return in < min ? min : (in > max ? max : in);
  }
};
