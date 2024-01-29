#pragma once
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
    std::vector<float> last_vcas{};

  public:
    daisysp::Oscillator osc1;
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

          flt.Init(samplerate);

          ad_vca.Init(samplerate);
          ad_vcf.Init(samplerate);

          last_vcas.reserve(1000);
        };

    bool note_match(daisy::NoteOnEvent& p) { return note == p.note; }
    bool note_match(daisy::NoteOffEvent& p) { return note == p.note; }

    void note_on(daisy::NoteOnEvent& p) {
      float freq{daisysp::mtof(p.note)}; 
      osc1.SetFreq(freq);
      osc1.SetAmp((p.velocity / 127.0f));
      if(!gate) {
        for(auto vca : last_vcas)
          LogPrint("VCA: %i\n", static_cast<int>(1000. * vca));
        last_vcas.clear();
        ad_vca.Trigger();
        ad_vcf.Trigger();
      }
      gate = true;
      note = p.note;
    }

    void note_off() {
      gate = false;
    }

    void retrigger() {
      ad_vcf.Trigger();
      ad_vca.Trigger();
    }

    void set_wave_shape(uint8_t wave_num) {
      osc1.SetWaveform(wave_num);
    }

    void set_vcf_attack(float t) { ad_vcf.SetTime(daisysp::AdEnvSegment::ADENV_SEG_ATTACK, t); }
    void set_vca_attack(float t) { ad_vca.SetTime(daisysp::AdEnvSegment::ADENV_SEG_ATTACK, t); }
    void set_vcf_decay(float t) { ad_vcf.SetTime(daisysp::AdEnvSegment::ADENV_SEG_DECAY, t); }
    void set_vca_decay(float t) { ad_vca.SetTime(daisysp::AdEnvSegment::ADENV_SEG_DECAY, t); }

    float process(float vcf_freq, float vcf_res, float vcf_env_depth) {
      float vca_env = ad_vca.Process();
      if(last_vcas.size() < 1000) {
        last_vcas.push_back(vca_env);
      }
      if(!ad_vca.IsRunning()) return 0.;

      daisy::MappedFloatValue vcf_freq_map{
        100, samplerate / 3  + 1, 440,
        daisy::MappedFloatValue::Mapping::log, "Hz"};
      float vcf_env = ad_vcf.Process();
      float vcf_final_freq = vcf_freq + vcf_env * vcf_env_depth;
      vcf_freq_map.SetFrom0to1(vcf_final_freq);
      flt.SetFreq(vcf_freq_map.Get());
      flt.SetRes(vcf_res);

      // Apply the processing values
      float sig = osc1.Process();
      sig = flt.Process(sig);
      sig *= vca_env;
      return sig;
    }
};
