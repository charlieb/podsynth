#pragma once
#include "daisy_pod.h"
#include "daisysp.h"
#include <array>
#include <vector>
#include "arp.h"
#include "lcd.h"

enum StepMode { chord, arp };

struct Step {
  std::vector<daisy::NoteOnEvent> notes;
  StepMode mode{StepMode::chord};
  bool active{false};
  bool gate{true};
  bool slide{false};
  float duty{0.5}; // from 0. to 1.
};


template <int P>
class Seq {
  public:
  static constexpr uint8_t nsteps{32};

  private:
  daisysp::Metro tick{};
  bool next{false};

  std::array<Step, nsteps> steps{};
  uint8_t current_step{0};

  public:
  Seq(float samplerate) {
    tick.Init(4.0, samplerate);
  }
  void next_step() { 
    uint8_t last_step = current_step;
    current_step++; 
    current_step = current_step % nsteps; 
    while(!steps[current_step].active && last_step != current_step) {
      current_step++; 
      current_step = current_step % nsteps; 
    }
  }
  Step& step() { return steps[current_step]; }
  Step& step(uint8_t s) { return steps[s]; }

  void update(std::vector<daisy::NoteOnEvent>& keys, std::array<Note, P>& notes) {
    if(!next) return;
    next = false;

    next_step();

    // turn all the notes off
    for(auto& note : notes)
      note.note_off();

    daisy::DaisySeed::Print("Seq update: step %u\n", current_step);

    keys.clear();
    for(auto& note : steps[current_step].notes)
      keys.push_back(note);

    daisy::DaisySeed::Print("Seq::update - set %u / %u notes\n", keys.size(), steps[current_step].notes.size());

  }

  void process() {
    if(!next)
      next = tick.Process();
  }

  void randomize() {
    for(auto& step : steps) {
      step.active = true;
      step.notes.clear();
      //daisy::NoteOnEvent
      step.notes.push_back({.channel = 0, .note = 60, .velocity=127});
    }
  }
};
