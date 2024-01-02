#pragma once

#include "daisy_pod.h"
#include "daisysp.h"

#include <random>

#include "note.h"

enum class ArpMode{
  asis,
  asc,
  desc,
  pingpong,
  random,
  Count
};

template <int P>
class Arp {
  static_assert(P > 0, "Arp: Polyphony of zero is no polyphony");

  daisy::Logger<daisy::LOGGER_INTERNAL>& logger;
  daisysp::Metro tick{};
  bool next{false};
  uint8_t arp_select{0};
  int pingpong_dir = 1;
  ArpMode mode{ArpMode::asis};
  std::mt19937 rng;

  public:
  Arp(float samplerate, daisy::Logger<daisy::LOGGER_INTERNAL>& logger) :
  logger(logger),
  rng(0) {
    tick.Init(1.0, samplerate);
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

  void update(std::vector<daisy::NoteOnEvent>& keys, std::array<Note, P>& notes) {
    static const auto asc{[](daisy::NoteOnEvent& a, daisy::NoteOnEvent& b) {return a.note < b.note;}};
    static const auto desc{[](daisy::NoteOnEvent& a, daisy::NoteOnEvent& b) {return a.note > b.note;}};

    if(!next) return;
    next = false;
    // turn all the notes off
    if(keys.size() <= 0) {
      for(auto& note : notes)
        note.note_off();
      return;
    }

    if(keys.size() > 1) {
      switch(static_cast<int>(mode)) {
        case static_cast<int>(ArpMode::asis): 
          update_asis(keys, notes);
          break;
        case static_cast<int>(ArpMode::asc): 
          std::sort(keys.begin(), keys.end(), asc);
          update_asis(keys, notes);
          break;
        case static_cast<int>(ArpMode::desc): 
          std::sort(keys.begin(), keys.end(), desc);
          update_asis(keys, notes);
          break;
        case static_cast<int>(ArpMode::pingpong): 
          update_pingpong(keys, notes);
          break;
        case static_cast<int>(ArpMode::random): 
          update_random(keys, notes);
          break;
      }
    }
    else {
      arp_select = 0;
    }
    
    logger.Print("Arp.update mode %i, note %i: %i\n", mode, arp_select, keys[arp_select].note);

    for(size_t i = 1; i < P; i++)
      notes[i].note_off();
    notes[0].note_on(keys[arp_select]);
    notes[0].retrigger();
  }

  void update_pingpong(std::vector<daisy::NoteOnEvent>& keys, std::array<Note, P>& notes) {
    if(arp_select >= keys.size() - 1) {
      pingpong_dir = -1;
    }
    if(arp_select <= 0) {
      pingpong_dir = 1;
    }
    arp_select = (arp_select + pingpong_dir) % keys.size();
  }

  void update_random(std::vector<daisy::NoteOnEvent>& keys, std::array<Note, P>& notes) {
    std::uniform_int_distribution<> distrib(0, keys.size() - 1);
    arp_select = distrib(rng);
  }

  void update_asis(std::vector<daisy::NoteOnEvent>& keys, std::array<Note, P>& notes) {
    arp_select = (arp_select + 1) % keys.size();
  }

  void process() {
    if(!next)
      next = tick.Process();
  }
};
