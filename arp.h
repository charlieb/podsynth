#pragma once

#include "daisy_pod.h"
#include "daisysp.h"

#include <random>

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

  std::vector<daisy::NoteOnEvent>& notes;

  public:
  Arp(float samplerate, std::vector<daisy::NoteOnEvent>& notes) :
  rng(0),
  notes(notes) {
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

  void set_notes(std::vector<daisy::NoteOnEvent>& notes) {
    static const auto asc{[](daisy::NoteOnEvent& a, daisy::NoteOnEvent& b) {return a.note < b.note;}};
    static const auto desc{[](daisy::NoteOnEvent& a, daisy::NoteOnEvent& b) {return a.note > b.note;}};

    this->notes = notes;

    if(notes.size() > 1) {
      switch(static_cast<int>(mode)) {
        case static_cast<int>(ArpMode::asis): 
          update_asis(notes);
          break;
        case static_cast<int>(ArpMode::asc): 
          std::sort(notes.begin(), notes.end(), asc);
          update_asis(notes);
          break;
        case static_cast<int>(ArpMode::desc): 
          std::sort(notes.begin(), notes.end(), desc);
          update_asis(notes);
          break;
        case static_cast<int>(ArpMode::pingpong): 
          update_pingpong(notes);
          break;
        case static_cast<int>(ArpMode::random): 
          update_random(notes);
          break;
      }
    }
    else {
      arp_select = 0;
    }
  }

  void update(Player& player) {
    if(!next) return;
    next = false;
    
    if(notes.size() > 1) {
      switch(static_cast<int>(mode)) {
        case static_cast<int>(ArpMode::asis): 
          update_asis(notes);
          break;
        case static_cast<int>(ArpMode::asc): 
          update_asis(notes);
          break;
        case static_cast<int>(ArpMode::desc): 
          update_asis(notes);
          break;
        case static_cast<int>(ArpMode::pingpong): 
          update_pingpong(notes);
          break;
        case static_cast<int>(ArpMode::random): 
          update_random(notes);
          break;
      }
    }
    else {
      arp_select = 0;
    }

    player.play_note(notes[arp_select]);
  }

  void update_pingpong(std::vector<daisy::NoteOnEvent>& keys) {
    if(arp_select >= keys.size() - 1) {
      pingpong_dir = -1;
    }
    if(arp_select <= 0) {
      pingpong_dir = 1;
    }
    arp_select = (arp_select + pingpong_dir) % keys.size();
  }

  void update_random(std::vector<daisy::NoteOnEvent>& keys) {
    std::uniform_int_distribution<> distrib(0, keys.size() - 1);
    arp_select = distrib(rng);
  }

  void update_asis(std::vector<daisy::NoteOnEvent>& keys) {
    arp_select = (arp_select + 1) % keys.size();
  }

  void process() {
    if(!next) 
      next = tick.Process();
  }
};
