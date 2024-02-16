#pragma once
#include "daisy_pod.h"
#include "daisysp.h"
#include <array>
#include <vector>
#include "arp.h"
#include "player.h"

#define LogPrint(...) daisy::DaisySeed::Print(__VA_ARGS__)
//#define LogPrint(...) 

struct Step {
  std::vector<uint8_t> notes;
};


class Seq {
  public:

  private:
  daisysp::Metro tick{};
  bool next{false};
  bool paused{false};

  std::vector<Step> steps{};
  uint8_t current_step{0};

  public:
  Seq(float samplerate) {
    tick.Init(4.0, samplerate);
    steps.reserve(16);
    add_step();
  }
  void step_inc(int inc) { 
    current_step = (current_step + inc) % steps.size(); 
  }
  void next_step() {
    step_inc(1);
  }
  void prev_step() {
    step_inc(-1);
  }
  void pop_note() {
    steps[current_step].notes.pop_back();
  }
  void push_note(uint8_t note) {
    steps[current_step].notes.push_back(note);
  }
  void set_arp(Arp& arp) {
    arp.set_notes(steps[current_step].notes);
  }
  int get_num_steps() { return steps.size(); }
  Step& get_step() {
    return steps[current_step];
  }
  void add_step() {
    steps.push_back(Step{});
  }
  void del_step() {
    if(steps.size() > 0) {
      steps.pop_back();
      if(current_step >= steps.size()) 
        step_inc(-1);
    }
  }
  void pause() {
    paused = true;
  }
  void unpause() {
    paused = false;
    tick.Reset();
  }
  void pause_toggle() {
    paused = !paused;
  }

  uint8_t get_step_num() { return current_step; }
  Step& step() { return steps[current_step]; }
  Step& step(uint8_t s) { return steps[s]; }

  void set_tempo(float secs) {
    tick.SetFreq(1.0 / secs);
  }

  void update(Player& player, Arp& arp) {
    if(!next || paused) return;
    next = false;

    next_step();

    //if(steps[current_step].mode == StepMode::chord)
    //  player.play_chord(steps[current_step].notes);
    //else
    arp.set_notes(steps[current_step].notes);

    //LogPrint("Seq::update - set %u / %u notes\n", notes.size(), steps[current_step].notes.size());
  }

  void process() {
    if(!next)
      next = tick.Process();
  }

 // void randomize() {
 //   for(int i = 0; i < nsteps; i++) {
 //     if(i < 8) {
 //       steps[i].active = true;
 //       steps[i].notes.clear();
 //       steps[i].notes.push_back({.channel = 0, .note = 60, .velocity=127});
 //       //steps[i].notes.push_back({.channel = 0, .note = 62, .velocity=127});
 //       //steps[i].notes.push_back({.channel = 0, .note = 64, .velocity=127});
 //       steps[i].mode = StepMode::arp;
 //     }
 //     if(i < 4) {
 //       steps[i].mode = StepMode::chord;
 //     }
 //     if(i >= 8) {
 //       steps[i].active = false;
 //     }
 //   }
 //   return;
 //   for(auto& step : steps) {
 //     step.active = true;
 //     step.notes.clear();
 //     //daisy::NoteOnEvent
 //     step.notes.push_back({.channel = 0, .note = 60, .velocity=127});
 //     step.notes.push_back({.channel = 0, .note = 62, .velocity=127});
 //     step.notes.push_back({.channel = 0, .note = 64, .velocity=127});
 //     step.mode = StepMode::arp;
 //   }
 // }
};
