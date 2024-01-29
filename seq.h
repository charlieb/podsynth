#pragma once
#include "daisy_pod.h"
#include "daisysp.h"
#include <array>
#include <vector>
#include "arp.h"
#include "player.h"

#define LogPrint(...) daisy::DaisySeed::Print(__VA_ARGS__)
//#define LogPrint(...) 

enum StepMode { chord, arp };

struct Step {
  std::vector<daisy::NoteOnEvent> notes;
  StepMode mode{StepMode::chord};
  bool active{false};
  bool gate{true};
  bool slide{false};
  float duty{0.5}; // from 0. to 1.
};


class Seq {
  public:
  static constexpr uint8_t nsteps{16};

  private:
  daisysp::Metro tick{};
  bool next{false};
  Arp arp;
  

  std::array<Step, nsteps> steps{};
  uint8_t current_step{0};

  public:
  Seq(float samplerate) :
  arp(samplerate, steps[0].notes)
  {
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

  void set_tempo(float freq) {
    tick.SetFreq(freq);
  }
  void set_arp_tempo(float freq) {
    tick.SetFreq(freq);
  }

  void update(Player& player) {
    // Arp needs to change notes within the seq step
    if(steps[current_step].mode == StepMode::arp) {
      arp.update(player);
    };

    if(!next) return;
    next = false;

    next_step();

    if(steps[current_step].mode == StepMode::chord)
      player.play_chord(steps[current_step].notes);
    else
      arp.set_notes(steps[current_step].notes);

    //LogPrint("Seq::update - set %u / %u notes\n", notes.size(), steps[current_step].notes.size());

  }

  void process() {
    if(!next)
      next = tick.Process();
    arp.process();
  }

  void randomize() {
    for(auto& step : steps) {
      step.active = true;
      step.notes.clear();
      //daisy::NoteOnEvent
      step.notes.push_back({.channel = 0, .note = 60, .velocity=127});
      step.notes.push_back({.channel = 0, .note = 62, .velocity=127});
      step.notes.push_back({.channel = 0, .note = 64, .velocity=127});
      step.mode = StepMode::arp;
    }
  }
};
