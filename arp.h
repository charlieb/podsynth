#pragma once

#include "daisy_pod.h"
#include "daisysp.h"

#include "note.h"

class Arp {
  daisysp::Metro tick{};
  bool next{false};
  int arp_select{0};

  public:
  Arp(float samplerate) {
    tick.Init(1.0, samplerate);
  }

  void set_note_len(float secs) { 
    tick.SetFreq(1.0 / secs);
  }

  template <int P>
  void update(std::vector<daisy::NoteOnEvent>& keys, std::array<Note, P>& notes) {
    static_assert(P > 0, "Arp: Polyphony of zero is no polyphony");
    static int arp_select{0};
    if(!next) return;
    next = false;
    // turn all the notes off
    if(keys.size() <= 0) {
      for(auto& note : notes)
        note.note_off();
      return;
    }

    arp_select = (arp_select + 1) % keys.size();

    //logger.Print("Player.arp_update selected: %i\n", arp_select);

    for(size_t i = 1; i < P; i++)
      notes[i].note_off();
    notes[0].note_on(keys[arp_select]);
    notes[0].retrigger();
  }

  void process() {
    if(!next)
      next = tick.Process();
  }
};
