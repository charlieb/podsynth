#pragma once
#include "Filters/moogladder.h"
#include "daisy_pod.h"
#include "daisysp.h"
#include <math.h>
#include <vector>

#define LogPrint(...) daisy::DaisySeed::Print(__VA_ARGS__)
//#define LogPrint(...) 

class Note {
  private:
    float samplerate{0};

    float last_sig{-1.};
    bool last_gate{false};

  public:
    daisysp::Oscillator osc1;
    daisysp::Oscillator osc2;
    float detune = 0.; // freq difference between osc1 and osc2
    bool gate{false};
    uint8_t note{0};

    daisysp::MoogLadder flt;
    daisysp::AdEnv ad_vca, ad_vcf;

    Note(float samplerate)
      : samplerate(samplerate) {
          LogPrint("Note Constructed: samplerate %f\n", samplerate);
          osc1.Init(samplerate);
          osc1.SetWaveform(daisysp::Oscillator::WAVE_POLYBLEP_SAW);
          osc1.SetAmp(1.0);
          osc2.Init(samplerate);
          osc2.SetWaveform(daisysp::Oscillator::WAVE_POLYBLEP_SAW);
          osc2.SetAmp(1.0);

          flt.Init(samplerate);

          ad_vca.Init(samplerate);
          ad_vcf.Init(samplerate);
        };

    bool note_match(daisy::NoteOnEvent& p) { return note == p.note; }
    bool note_match(daisy::NoteOffEvent& p) { return note == p.note; }

    void note_on(daisy::NoteOnEvent& p) {
      float freq{daisysp::mtof(p.note)}; 
      osc1.SetFreq(freq);
      osc1.SetAmp(p.velocity / 127.0f);
      osc2.SetFreq(freq * detune);
      osc2.SetAmp(p.velocity / 127.0f);
      if(!gate) {
        ad_vca.Trigger(true);
        ad_vcf.Trigger(true);
      }
      gate = true;
      note = p.note;
    }

    void note_off() {
      gate = false;
    }

    void set_wave_shape(uint8_t wave_num) {
      osc1.SetWaveform(wave_num);
      osc2.SetWaveform(wave_num);
    }

    void set_vcf_attack(float t) { ad_vcf.SetTime(daisysp::AdEnvSegment::ADENV_SEG_ATTACK, t); }
    void set_vca_attack(float t) { ad_vca.SetTime(daisysp::AdEnvSegment::ADENV_SEG_ATTACK, t); }
    void set_vcf_decay(float t) { ad_vcf.SetTime(daisysp::AdEnvSegment::ADENV_SEG_DECAY, t); }
    void set_vca_decay(float t) { ad_vca.SetTime(daisysp::AdEnvSegment::ADENV_SEG_DECAY, t); }
    void set_detune(float t) { detune = t; }; // takes effect next note_on

    float process(float vcf_freq, float vcf_res, float vcf_env_depth) {
      if(!gate) return 0.;
      float vca_env = ad_vca.Process();
      if(!ad_vca.IsRunning()) return 0.;

      static daisy::MappedFloatValue vcf_freq_map{
        100, samplerate / 3  + 1, 440,
        daisy::MappedFloatValue::Mapping::log, "Hz"};
      float vcf_env = ad_vcf.Process();
      float vcf_final_freq = vcf_freq + vcf_env * vcf_env_depth;
      vcf_freq_map.SetFrom0to1(vcf_final_freq);
      flt.SetFreq(vcf_freq_map.Get());
      flt.SetRes(vcf_res);

      // Apply the processing values
      float sig = (osc1.Process() + osc2.Process()) / 2.;
      sig = flt.Process(sig);
      sig *= vca_env;
      return sig;
    }
};
