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
#include "arp.h"
#include "lcd.h"
#include "seq.h"


enum class PlayerMode {
  keyboard,
  arp,
  seq,
  Count
};

// Printing Functions

void playermode_name(char *out, PlayerMode mode) {
  if(mode == PlayerMode::keyboard) {
    strncpy(out, "Key", 16);
  }
  else if(mode == PlayerMode::arp) {
    strncpy(out, "Arp", 16);
  }
  else if(mode == PlayerMode::seq) {
    strncpy(out, "Seq", 16);
  }
}

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


class Player {

  float vcf_env_depth = 0.;
  float vca_bias = 0.;
  daisy::MappedFloatValue vcf_freq{100, 20'000, 440, daisy::MappedFloatValue::Mapping::log, "Hz"};
  float vcf_res = 0;

  static constexpr int poly{6};
  // This initlizer kinda sucks, boo c++
  std::array<Note, poly> notes{{
    {vcf_env_depth, vcf_freq, vcf_res},
      {vcf_env_depth, vcf_freq, vcf_res},
      {vcf_env_depth, vcf_freq, vcf_res},
      {vcf_env_depth, vcf_freq, vcf_res},
      {vcf_env_depth, vcf_freq, vcf_res},
      {vcf_env_depth, vcf_freq, vcf_res}
  }};

  std::vector<daisy::NoteOnEvent> keys;
  PlayerMode mode{PlayerMode::keyboard};

  Arp<poly> arp;
  Seq seq;
  LCD& lcd;
  daisy::DaisyPod& pod;

  public:

  Player(float samplerate, daisy::DaisyPod& pod, LCD& lcd) :
    arp(samplerate),
    lcd(lcd),
    pod(pod)
  {
    for(auto& note : notes)
      note.init(samplerate);
    arp.set_note_len(0.05125);
  }

  void update() {
    switch(static_cast<int>(mode)) {
      case static_cast<int>(PlayerMode::keyboard):
        keyboard_update();
        break;
      case static_cast<int>(PlayerMode::arp):
        arp.update(keys, notes);
        break;
    }
  }

  void keyboard_update() {
    // This code's a bit odd-looking but the idea is
    // go through all the currently pressed keys.
    // Some of them have been pressed since the last update
    // but some are still pressed from before that.
    // So first we have to match pressed keys to already 
    // playing notes, that's loop (1).
    // Bonus: we also catch re-pressed notes at this stage
    // If there's no match then maybe there's a non-playing
    // note left that we can assign to play this key. Loop (2)
    // Now all the pressed keys (up to the polyphony limit)
    // have a note that's playing them ... but wait there's more.
    // There's also the notes for keys that have been released
    // since the last update. i.e. notes with no matching key
    // That's loop (3). Maybe this should be the first thing
    // we do here but eh, it all happens so fast no-one's going
    // to notice.
    for(auto key : keys) {
      bool claimed = false;
      for(auto& note : notes) { // (1): Keep keys on
        if(note.note_match(key)) {
          note.note_on(key);
          claimed = true;
          break;
        }
      }
      if(claimed) continue;
      for(auto& note : notes) { // (2): Turn new keys on
        if(!note.gate) {
          note.note_on(key);
          claimed = true;
          break;
        }
      }
      if(claimed) continue;

      daisy::DaisySeed::Print("Player.update Failed to add note: %i\n", key.note);
    }
    for(auto& note : notes) { // (3): Turn released keys off
      bool claimed = false;
      for(auto key : keys) {
        if(note.note_match(key)) {
          claimed = true;
          break;
        }
      }
      if(!claimed) note.note_off();
    }
  }

  // AUDIO CALLBACK
  void AudioCallback(daisy::AudioHandle::InterleavingInputBuffer  in,
      daisy::AudioHandle::InterleavingOutputBuffer out,
      size_t size)
  {
    float note_total{0};
    for(size_t i = 0; i < size; i += 2) {
      for(auto& note : notes)
        note_total += note.process();
      out[i] = out[i + 1] = note_total / poly;
    }

    if(mode == PlayerMode::arp)
      arp.process();
  }

  // KEYS and other controlled events
  // This is the boring repetative code

  void key_pressed(daisy::NoteOnEvent on) {
    daisy::DaisySeed::Print("Key Pressed: Chan: %d Note: %d Vel: %d\n", on.channel, on.note, on.velocity);
    keys.push_back(on);
  }

  void key_released(daisy::NoteOnEvent off) {
    daisy::DaisySeed::Print("Key Released: Chan: %d Note: %d\n", off.channel, off.note);
    std::erase_if(keys, [off](daisy::NoteOnEvent k) {return k.note == off.note;});
  }

  void set_wave_shape(uint8_t wave_num) {
    char tmp[25]{0,};
    wave_name(tmp, wave_num);
    daisy::DaisySeed::Print("Control Received: Waveform %i: %s\n",wave_num, tmp);
    for(auto& note : notes) {
      note.osc.SetWaveform(wave_num);
    }
  }
  void set_vcf_cutoff(float cutoff_knob) {
    vcf_freq.SetFrom0to1(cutoff_knob);
    daisy::DaisySeed::Print("Control Received: vcf_freq -> %.02f\n", vcf_freq);
  }
  void set_vcf_resonance(float res) {
    vcf_res = res;
    daisy::DaisySeed::Print("Control Received: vcf_res -> %f\n", vcf_res);
  }
  void set_vcf_envelope_depth(float depth) {
    vcf_env_depth = depth;
    daisy::DaisySeed::Print("Control Received: vcf_env_depth -> %f\n", vcf_env_depth);
  }
  void set_vca_bias(float bias) {
    vca_bias = bias;
    daisy::DaisySeed::Print("Control Received: vca_bias -> %f\n", vca_bias);
  }
  void set_envelope_a_vca(float attack) {
    daisy::DaisySeed::Print("Control Received: VCA Attack -> 0.%03i\n", static_cast<int>(1000 * attack));
    for(auto& note : notes)
      note.adsr_vca.SetAttackTime(attack); // secs
  }
  void set_envelope_d_vca(float val) {
    daisy::DaisySeed::Print("Control Received: VCA Decay -> 0.%03i\n", static_cast<int>(1000 * val));
    for(auto& note : notes)
      note.adsr_vca.SetDecayTime(val); // secs
  }
  void set_envelope_s_vca(float val) {
    daisy::DaisySeed::Print("Control Received: VCA Sustain -> 0.%03i\n", static_cast<int>(1000 * val));
    for(auto& note : notes)
      note.adsr_vca.SetSustainLevel(val);
  }
  void set_envelope_r_vca(float val) {
    daisy::DaisySeed::Print("Control Received: VCA Release -> 0.%03i\n", static_cast<int>(1000 * val));
    for(auto& note : notes)
      note.adsr_vca.SetReleaseTime(val); // secs
  }
  void set_envelope_a_vcf(float val) {
    daisy::DaisySeed::Print("Control Received: VCF Attack -> 0.%03i\n", static_cast<int>(1000 * val));
    for(auto& note : notes)
      note.adsr_vcf.SetAttackTime(val); // secs
  }
  void set_envelope_d_vcf(float val) {
    daisy::DaisySeed::Print("Control Received: VCF Decay -> 0.%03i\n", static_cast<int>(1000 * val));
    for(auto& note : notes)
      note.adsr_vcf.SetDecayTime(val); // secs
  }
  void set_envelope_s_vcf(float val) {
    daisy::DaisySeed::Print("Control Received: VCF Sustain -> 0.%03i\n", static_cast<int>(1000 * val));
    for(auto& note : notes)
      note.adsr_vcf.SetSustainLevel(val);
  }
  void set_envelope_r_vcf(float val) {
    daisy::DaisySeed::Print("Control Received: VCF Release -> 0.%03i\n", static_cast<int>(1000 * val));
    for(auto& note : notes)
      note.adsr_vcf.SetReleaseTime(val); // secs
  }
  void set_mode(PlayerMode new_mode) {
    mode = new_mode;
    char new_mode_name[16]{};
    playermode_name(new_mode_name, mode);
    daisy::DaisySeed::Print("Control Received: Mode Toggle %s\n", new_mode_name);
  }
  PlayerMode get_mode() {return mode;};
  void set_arp_note_len(float len) {
    arp.set_note_len(len);
    daisy::DaisySeed::Print("Control Received: Arp note length\n");
  }
  void next_arp_mode() {
    arp.next_mode();
    daisy::DaisySeed::Print("Control Received: Arp next mode\n");
  }
};
