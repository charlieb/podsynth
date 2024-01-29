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

#define LogPrint(...) daisy::DaisySeed::Print(__VA_ARGS__)
//#define LogPrint(...) 

// Printing Functions

void wave_name(char *out, int val) {
  switch(val) {
    case 0:
      strcpy(out, "wave_sin");
      break;
    case 1:
      strcpy(out, "wave_tri");
      break;
    case 2:
      strcpy(out, "wave_saw");
      break;
    case 3:
      strcpy(out, "wave_ramp");
      break;
    case 4:
      strcpy(out, "wave_square");
      break;
    case 5:
      strcpy(out, "wave_polyblep_tri");
      break;
    case 6:
      strcpy(out, "wave_polyblep_saw");
      break;
    case 7:
      strcpy(out, "wave_polyblep_square");
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

class Player {
  public:
  static constexpr size_t poly{6};

  private:
  float vcf_env_depth = 0.;
  float vca_bias = 0.;
  float vcf_freq = 0;
  float vcf_res = 0;

  float last_note_total{-1.};

  // This initlizer kinda sucks, boo c++
  std::array<Note, poly> notes{{{0},{0},{0},{0},{0},{0}}};

  public:

  Player(float samplerate) {
    for(auto& note : notes) {
      Note n{samplerate};
      note = n;
    }
    //arp.set_note_len(0.05125);
  }

  void play_chord(std::vector<daisy::NoteOnEvent>& keys) {
    for(auto& note : notes)
      note.note_off();

    for(size_t i = 0; i < std::min(poly, keys.size()); i++) {
      notes[i].note_on(keys[i]);
    }
  }

  void play_note(daisy::NoteOnEvent& key) {
    for(auto& note : notes)
      note.note_off();

    notes[0].note_on(key);
    //LogPrint("Player.note_on last_note_total: %i\n", static_cast<int>(last_note_total * 1000.));
  }

  // AUDIO CALLBACK
  void AudioCallback(daisy::AudioHandle::InterleavingInputBuffer  in,
      daisy::AudioHandle::InterleavingOutputBuffer out,
      size_t size)
  {
    float note_total{0};
    for(size_t i = 0; i < size; i += 2) {
      for(auto& note : notes)
        note_total += note.process(vcf_freq, vcf_res, vcf_env_depth);
      out[i] = out[i + 1] = note_total / poly;
      last_note_total = note_total;
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
  void set_vca_bias(float bias) {
    LogPrint("Control Received: vca_bias -> %f\n", vca_bias);
    vca_bias = bias;
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
  void set_envelope_s_vca(float val) {
    LogPrint("Control Received: VCA Sustain -> 0.%03i\n", static_cast<int>(1000 * val));
    //for(auto& note : notes)
    //  note.adsr_vca.SetSustainLevel(val);
  }
  void set_envelope_r_vca(float val) {
    LogPrint("Control Received: VCA Release -> 0.%03i\n", static_cast<int>(1000 * val));
    //for(auto& note : notes)
    //  note.adsr_vca.SetReleaseTime(val); // secs
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
  void set_envelope_s_vcf(float val) {
    LogPrint("Control Received: VCF Sustain -> 0.%03i\n", static_cast<int>(1000 * val));
    //for(auto& note : notes)
    //  note.adsr_vcf.SetSustainLevel(val);
  }
  void set_envelope_r_vcf(float val) {
    LogPrint("Control Received: VCF Release -> 0.%03i\n", static_cast<int>(1000 * val));
    //for(auto& note : notes)
    //  note.adsr_vcf.SetReleaseTime(val); // secs
  }
};
