#pragma once
#include "daisy_pod.h"
#include "daisysp.h"
#include <util/MappedValue.h>
#include <cstdio>
#include <string.h>
#include <unordered_map>
#include <cstdarg>
#include <array>
#include <vector>
#include "note.h"

//#define LogPrint(...) daisy::DaisySeed::Print(__VA_ARGS__)
#define LogPrint(...) 

// Printing Functions

void wave_name(char *out, int val) {
  switch(val) {
    case 0:
      strcpy(out, "sin");
      break;
    case 1:
      strcpy(out, "tri");
      break;
    case 2:
      strcpy(out, "saw");
      break;
    case 3:
      strcpy(out, "ramp");
      break;
    case 4:
      strcpy(out, "square");
      break;
    case 5:
      strcpy(out, "pb_tri");
      break;
    case 6:
      strcpy(out, "pb_saw");
      break;
    case 7:
      strcpy(out, "pb_square");
      break;
    default:
      break;
  }
}

class KeyTracker {
  public:
    std::vector<daisy::NoteOnEvent> keys;
  void press(daisy::NoteOnEvent on) {
    LogPrint("Key Pressed: Chan: %d Note: %d Vel: %d\n", on.channel, on.note, on.velocity);
    keys.push_back(on);
  }

  void release(daisy::NoteOnEvent off) {
    LogPrint("Key Released: Chan: %d Note: %d\n", off.channel, off.note);
    std::erase_if(keys, [off](daisy::NoteOnEvent k) {return k.note == off.note;});
  }
};

static uint32_t DSY_SDRAM_BSS reverb_heap[sizeof(daisysp::ReverbSc)];

class Player {
  public:
  static constexpr size_t poly{1};

  private:
  float vcf_env_depth = 0.;
  float vcf_freq = 5000;
  float vcf_res = 0;
  float samplerate = 0;

  float last_note_total{-1.};

  daisysp::DelayLine<float, 48000> delay;
  float delay_mix{0};
  daisysp::Overdrive drive;
  daisysp::ReverbSc* reverb = new(reverb_heap) daisysp::ReverbSc();
  float reverb_wet{0.0};
  float reverb_gain{0.25}; // Reverb output is SUPER LOUD so cut it
  //float gain{2.};

  // This initlizer kinda sucks, boo c++
  std::array<Note, poly> notes{{{0}}}; //,{0},{0},{0},{0}}};

  public:

  Player(float samplerate) : samplerate(samplerate) {
    for(auto& note : notes) {
      Note n{samplerate};
      note = n;
    }
    delay.Init();
    reverb->Init(samplerate);
    reverb->SetLpFreq(18000.0f);
    reverb->SetFeedback(0.85f);
  }

  void play_chord(std::vector<daisy::NoteOnEvent>& keys) {
    for(auto& note : notes)
      note.note_off();

    for(size_t i = 0; i < std::min(poly, keys.size()); i++) {
      notes[i].note_on(keys[i]);
    }
  }

  void play_rest() {
    for(auto& note : notes)
      note.note_off();
  }
  void play_note(daisy::NoteOnEvent& key) {
    for(auto& note : notes)
      note.note_off();

    notes[0].note_on(key);
    //LogPrint("Player.note_on last_note_total: %i\n", static_cast<int>(last_note_total * 1000.));
  }

  // AUDIO CALLBACK
  void AudioCallback(daisy::AudioHandle::InterleavingInputBuffer in,
      daisy::AudioHandle::InterleavingOutputBuffer out,
      size_t size)
  {
    float note_total{0};
    for(size_t i = 0; i < size; i += 2) {
      for(auto& note : notes)
        note_total += note.process(vcf_freq, vcf_res, vcf_env_depth);
      note_total /= poly;
      note_total += delay.Read() * delay_mix;
      note_total /= 1. + delay_mix;
      delay.Write(note_total);
      float rvb_out1{0}, rvb_out2{0};
      reverb->Process(note_total, note_total, &rvb_out1, &rvb_out2);
      // Apply gain after reverb processing because rvb feedback makes it 
      // very loud
      //note_total *= gain;
      rvb_out1 *= reverb_gain;
      rvb_out2 *= reverb_gain;
      out[i] = rvb_out1 * reverb_wet + note_total * (1 - reverb_wet);
      out[i + 1] = rvb_out2 * reverb_wet + note_total * (1 - reverb_wet);
    }
  }

  // Controller changes
  // This is the boring repetative code
  void set_wave_shape(uint8_t wave_num) {
    char tmp[25]{0,};
    wave_name(tmp, wave_num);
    LogPrint("Control Received: Waveform %i: %s\n",wave_num, tmp);
    for(auto& note : notes) {
      note.set_wave_shape(wave_num);
    }
  }
  void set_vcf_cutoff(float cutoff_knob) {
    LogPrint("Control Received: vcf_freq -> 0.%i\n", static_cast<int>(1000*cutoff_knob));
    vcf_freq = cutoff_knob;
  }
  void set_vcf_resonance(float res) {
    LogPrint("Control Received: vcf_res -> 0.%i\n", static_cast<int>(1000*res));
    vcf_res = res;
  }
  void set_vcf_envelope_depth(float depth) {
    LogPrint("Control Received: vcf_env_depth -> 0.%i\n", static_cast<int>(1000*depth));
    vcf_env_depth = depth;
  }
  void set_envelope_a_vca(float val) {
    if(val <= 0.007)
      val = 0.007;
    LogPrint("Control Received: VCA Attack -> 0.%03i\n", static_cast<int>(1000 * val));
    for(auto& note : notes)
      note.set_vca_attack(val); // secs
  }
  void set_envelope_d_vca(float val) {
    if(val <= 0.007)
      val = 0.007;
    LogPrint("Control Received: VCA Decay -> 0.%03i\n", static_cast<int>(1000 * val));
    for(auto& note : notes)
      note.set_vca_decay(val); // secs
  }
  void set_envelope_a_vcf(float val) {
    if(val <= 0.007)
      val = 0.007;
    LogPrint("Control Received: VCF Attack -> 0.%03i\n", static_cast<int>(1000 * val));
    for(auto& note : notes)
      note.set_vcf_attack(val); // secs
  }
  void set_envelope_d_vcf(float val) {
    if(val <= 0.007)
      val = 0.007;
    LogPrint("Control Received: VCF Decay -> 0.%03i\n", static_cast<int>(1000 * val));
    for(auto& note : notes)
      note.set_vcf_decay(val); // secs
  }
  void set_delay_time(float val) {
    LogPrint("Control Received: Delay Delay -> 0.%03i\n", static_cast<int>(1000 * val));
    delay.SetDelay(val);
  }
  void set_delay_mix(float val) {
    LogPrint("Control Received: Delay Mix -> 0.%03i\n", static_cast<int>(1000 * val));
    delay_mix = val;
    }
  void set_reverb_damp_freq(float val) {
    static daisy::MappedFloatValue rv_freq_map{
      100, samplerate / 3  + 1, 440,
        daisy::MappedFloatValue::Mapping::log, "Hz"};
    rv_freq_map.SetFrom0to1(val);
    LogPrint("Control Received: Reverb Freq -> 0.%03i\n", static_cast<int>(1000 * rv_freq_map.Get()));
    reverb->SetLpFreq(val);
  }
  void set_reverb_feedback(float val) {
    LogPrint("Control Received: Reverb Feedback -> 0.%03i\n", static_cast<int>(1000 * val));
    reverb->SetFeedback(val);
  }
  void set_reverb_wet(float val) {
    LogPrint("Control Received: Reverb wet -> 0.%03i\n", static_cast<int>(1000 * val));
    reverb_wet = val;
  }
  void set_detune(float val) {
    LogPrint("Control Received: Detune -> 0.%03i\n", static_cast<int>(1000 * val));
    for(auto& note : notes)
      note.set_detune(val);
  }
};
