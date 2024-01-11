#pragma once
#include "daisy_pod.h"
#include "daisysp.h"

class Note {
  private:
    float& vcf_env_depth;
    daisy::MappedFloatValue& vcf_freq;
    float& vcf_res;

  public:
    daisysp::Oscillator osc;
    bool gate{false};
    uint8_t note{0};

    daisysp::MoogLadder flt;
    daisysp::Adsr adsr_vca, adsr_vcf;

    Note(float& vcf_env_depth, daisy::MappedFloatValue& vcf_freq, float& vcf_res)
      : vcf_env_depth(vcf_env_depth)
        , vcf_freq(vcf_freq)
        , vcf_res(vcf_res) {
          daisy::DaisySeed::Print("Note Init\n");
        };

    void init(float samplerate) {
      osc.Init(samplerate);
      osc.SetWaveform(daisysp::Oscillator::WAVE_POLYBLEP_SAW);
      osc.SetAmp(1.0);

      flt.Init(samplerate);

      adsr_vca.Init(samplerate);
      adsr_vcf.Init(samplerate);
    };

    bool note_match(daisy::NoteOnEvent& p) {
      return note == p.note; }

    bool note_match(daisy::NoteOffEvent& p) { 
      return note == p.note; }

    void note_on(daisy::NoteOnEvent& p) {
      osc.SetFreq(daisysp::mtof(p.note));
      osc.SetAmp((p.velocity / 127.0f));
      gate = true;
      note = p.note;
    }

    void note_off() {
      gate = false;
    }

    void retrigger() {
      adsr_vcf.Retrigger(false);
      adsr_vca.Retrigger(false);
    }

    float process() {
      float vcf_env = adsr_vcf.Process(gate);
      flt.SetFreq(vcf_freq + vcf_env * vcf_env_depth);
      flt.SetRes(vcf_res);
      float vca_env = adsr_vca.Process(gate);

      // Apply the processing values
      float sig = osc.Process();
      sig = flt.Process(sig);
      sig *= vca_env;
      return sig;
    }
};
