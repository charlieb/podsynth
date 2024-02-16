#pragma once

#include "daisy_pod.h"
#include "daisysp.h"

#include <random>
#include <functional>

#include "note.h"
#include "player.h"

#define LogPrint(...) daisy::DaisySeed::Print(__VA_ARGS__)
//#define LogPrint(...) 

enum class ArpMode{
  asis,
  asc,
  desc,
  pingpong,
  random,
  Count
};

class Arp {
  daisysp::Metro tick{};
  bool next{false};
  uint8_t arp_select{0};
  int pingpong_dir = 1;
  ArpMode mode{ArpMode::asis};
  std::mt19937 rng;

  std::vector<uint8_t> notes;
  std::vector<uint8_t> notes_asc;
  std::vector<uint8_t> notes_desc;
  uint8_t current_note;

  public:
  Arp(float samplerate) :
  rng(0)
  {
    tick.Init(50.0, samplerate);
  }

  void set_note_len(float secs) { 
    tick.SetFreq(1.0 / secs);
  }
  void next_mode() {
    int imode = static_cast<int>(mode) + 1;
    int icnt = static_cast<int>(ArpMode::Count);
    if(imode >= icnt) 
      imode = 0;
    mode = static_cast<ArpMode>(imode);
   }

  void mode_name(char name_out[9]) {
    std::memset(name_out, 0, 9);
    switch(mode) {
      case ArpMode::asis:
        std::memcpy(name_out, "As Is", 5);
        break;
      case ArpMode::asc:
        std::memcpy(name_out, "Asc", 3);
        break;
      case ArpMode::desc:
        std::memcpy(name_out, "Desc", 4);
        break;
      case ArpMode::pingpong:
        std::memcpy(name_out, "Pingpong", 8);
        break;
      case ArpMode::random:
        std::memcpy(name_out, "Rand", 5);
        break;
      default:
        break;
    }
  }

  void clear() {
    notes.clear();
    notes_asc.clear();
    notes_desc.clear();
    arp_select = 0;
  }

  void set_notes(std::vector<uint8_t>& new_notes) {
    notes = new_notes;
    // Set the sorted notes
    notes_asc = notes;
    std::sort(notes_asc.begin(), notes_asc.end(), std::less<uint8_t>());
    notes_desc = notes;
    std::sort(notes_desc.begin(), notes_desc.end(), std::greater<uint8_t>());
  }

  void add_note(uint8_t note) {
    notes.push_back(note);
    // Set the sorted notes
    notes_asc = notes;
    std::sort(notes_asc.begin(), notes_asc.end(), std::less<uint8_t>());
    notes_desc = notes;
    std::sort(notes_desc.begin(), notes_desc.end(), std::greater<uint8_t>());
  }

  void insert_note(uint8_t pos, uint8_t note) {
    notes.push_back(note);
    // Set the sorted notes
    notes_asc = notes;
    std::sort(notes_asc.begin(), notes_asc.end(), std::less<uint8_t>());
    notes_desc = notes;
    std::sort(notes_desc.begin(), notes_desc.end(), std::greater<uint8_t>());
  }

  void walk_notes(int dir) {
    uint8_t n = notes.back();
    notes.pop_back();
    n += 12 * dir;
    notes.insert(notes.begin(), n);
  }

  void update(Player& player) {
    if(!next) return;
    next = false;
    
    if(notes.size() <= 0) 
      return;

    switch(static_cast<int>(mode)) {
      case static_cast<int>(ArpMode::asis): 
        update_asis();
        current_note = notes[arp_select];
        break;
      case static_cast<int>(ArpMode::asc): 
        update_asis();
        current_note = notes_asc[arp_select];
        break;
      case static_cast<int>(ArpMode::desc): 
        update_asis();
        current_note = notes_desc[arp_select];
        break;
      case static_cast<int>(ArpMode::pingpong): 
        update_pingpong();
        current_note = notes[arp_select];
        break;
      case static_cast<int>(ArpMode::random): 
        update_random();
        current_note = notes[arp_select];
        break;
    }

    if(current_note == 127)
      player.play_rest();
    else {
      static daisy::NoteOnEvent note{0, current_note, 127};
      note.note = current_note;
      player.play_note(note);
    }
  }

  void update_pingpong() {
    if(arp_select >= notes.size() - 1) {
      pingpong_dir = -1;
    }
    if(arp_select <= 0) {
      pingpong_dir = 1;
    }
    arp_select = (arp_select + pingpong_dir) % notes.size();
  }

  void update_random() {
    std::uniform_int_distribution<> distrib(0, notes.size() - 1);
    arp_select = distrib(rng);
  }

  void update_asis() {
    arp_select = (arp_select + 1) % notes.size();
  }

  void process() {
    if(!next) 
      next = tick.Process();
  }
};
