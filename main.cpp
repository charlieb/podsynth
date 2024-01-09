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

enum class SynthControl {
  wave_shape,
  vcf_cutoff,
  vcf_resonance,
  vcf_envelope_depth,
  vca_bias,
  envelope_a_vca,
  envelope_d_vca,
  envelope_s_vca,
  envelope_r_vca,
  envelope_a_vcf,
  envelope_d_vcf,
  envelope_s_vcf,
  envelope_r_vcf,
  mode_toggle,
  arp_note_length,
  arp_mode,
  Count
};

enum class PlayerMode {
  keyboard,
  arp,
  seq,
  Count
};

class Player {
  // Map of daisy::ControlChangeEvent::control_number aka midi control number
  // to the synth control.
  // If you're hooking up your own controller, this is the place
  // to map your controls.
  std::unordered_map<uint8_t, SynthControl> midi_map {
    {74, SynthControl::wave_shape}, // Knob 2
      {71, SynthControl::vcf_cutoff}, // Knob 3
      {19, SynthControl::vcf_resonance}, // Knob 11
      {76, SynthControl::vcf_envelope_depth}, // Knob 4
      //{16, SynthControl::vca_bias}, // Knob 12
      {77, SynthControl::envelope_a_vca}, // Knob 5
      {93, SynthControl::envelope_d_vca}, // Knob 6
      {73, SynthControl::envelope_s_vca}, // Knob 7
      {75, SynthControl::envelope_r_vca}, // Knob 8
      {17, SynthControl::envelope_a_vcf}, // Knob 13
      {91, SynthControl::envelope_d_vcf}, // Knob 14
      {79, SynthControl::envelope_s_vcf}, // Knob 15
      {72, SynthControl::envelope_r_vcf}, // Knob 16
      {113, SynthControl::mode_toggle}, // Knob 1 press
      {16, SynthControl::arp_note_length}, // Knob 12
      {115, SynthControl::arp_mode}, // Knob 9 press
  };


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
  daisy::Color red;
  daisy::Color green;
  daisy::Color blue;

  Arp<poly> arp;
  daisy::DaisyPod& pod;

  public:

  Player(float samplerate, daisy::DaisyPod& pod) :
  arp(samplerate),
  pod(pod)
  {
    for(auto& note : notes)
      note.init(samplerate);
    arp.set_note_len(0.05125);
    red.Init(1, 0, 0);
    green.Init(0, 1, 0);
    blue.Init(0, 1, 0);
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

    switch(static_cast<int>(mode)) {
      case static_cast<int>(PlayerMode::keyboard):
        pod.led1.SetColor(green);
        break;
      case static_cast<int>(PlayerMode::arp):
        pod.led1.SetColor(blue);
        break;
    }
    pod.UpdateLeds();
  }

  void keyboard_update() {
    // Turn on all the notes whos keys are pressed
    for(auto key : keys) {
      bool claimed = false;
      for(auto& note : notes) {
        if(note.note_match(key)) {
          note.note_on(key);
          claimed = true;
          break;
        }
      }
      if(claimed) continue;

      for(auto& note : notes) {
        if(!note.gate) {
          note.note_on(key);
          claimed = true;
          break;
        }
      }
      if(claimed) continue;

      daisy::DaisySeed::Print("Player.update Failed to add note: %i\n", key.note);
    }
    // Now turn off the ones that aren't pressed
    for(auto& note : notes) {
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

  // KEYS and MIDI

  void key_pressed(daisy::NoteOnEvent on) {
    keys.push_back(on);
  }

  void key_released(daisy::NoteOnEvent off) {
    std::erase_if(keys, [off](daisy::NoteOnEvent k) {return k.note == off.note;});
  }

  void wave_name(char *out, int val) {
    switch(val) {
      case 0:
        strcpy(out, "WAVE_SIN");
        break;
      case 1:
        strcpy(out, "WAVE_TRI");
        break;
      case 2:
        strcpy(out, "WAVE_SAW");
        break;
      case 3:
        strcpy(out, "WAVE_RAMP");
        break;
      case 4:
        strcpy(out, "WAVE_SQUARE");
        break;
      case 5:
        strcpy(out, "WAVE_POLYBLEP_TRI");
        break;
      case 6:
        strcpy(out, "WAVE_POLYBLEP_SAW");
        break;
      case 7:
        strcpy(out, "WAVE_POLYBLEP_SQUARE");
        break;
      default:
        break;
    }
  }

  // Typical Switch case for Message Type.
  void HandleMidiMessage(daisy::MidiEvent m)
  {
    switch(m.type) {
      case daisy::NoteOn:
        {
          daisy::DaisySeed::Print("Note On:\t%d\t%d\t%d\n", m.channel, m.data[0], m.data[1]);
          key_pressed(m.AsNoteOn());
        }
        break;
      case daisy::NoteOff: 
        {
          daisy::DaisySeed::Print("Note Off:\t%d\t%d\t%d\n", m.channel, m.data[0], m.data[1]);
          key_released(m.AsNoteOn());
        }
        break;
      case daisy::ControlChange: 
        {
          daisy::ControlChangeEvent p = m.AsControlChange();
          daisy::DaisySeed::Print("Control Received:\t%d\t%d -> %i / %i\n", p.control_number, p.value, midi_map[p.control_number], SynthControl::wave_shape);
          switch(static_cast<int>(midi_map[p.control_number])) {
            case static_cast<int>(SynthControl::wave_shape):
              {
                char tmp[25]{0,};
                uint8_t wave_num = static_cast<uint8_t>(8 * p.value / 127);
                wave_name(tmp, wave_num);
                daisy::DaisySeed::Print("Control Received: Waveform %i: %s\n",wave_num, tmp);
                for(auto& note : notes) {
                  note.osc.SetWaveform(static_cast<uint8_t>(p.value / 9));
                }
              }
              break;
            case static_cast<int>(SynthControl::vcf_cutoff):
              {
                vcf_freq.SetFrom0to1(p.value / 128.0);
                daisy::DaisySeed::Print("Control Received: vcf_freq -> %.02f\n", vcf_freq);
              }
              break;
            case static_cast<int>(SynthControl::vcf_resonance):
              {
                vcf_res = p.value / 128.0;
                daisy::DaisySeed::Print("Control Received: vcf_res -> %f\n", vcf_res);
              }
              break;
            case static_cast<int>(SynthControl::vcf_envelope_depth):
              {
                vcf_env_depth = 10'000 * p.value / 128.0;
                daisy::DaisySeed::Print("Control Received: vcf_env_depth -> %i\n", static_cast<int>(vcf_env_depth));
              }
              break;
            case static_cast<int>(SynthControl::vca_bias):
              {
                vca_bias = p.value / 128.0;
                daisy::DaisySeed::Print("Control Received: vca_bias -> %f\n", vca_bias);
              }
              break;
            case static_cast<int>(SynthControl::envelope_a_vca):
              {
                daisy::DaisySeed::Print("Control Received: VCA Attack -> 0.%03i\n", static_cast<int>(1000 * p.value / 128.0));
                for(auto& note : notes)
                  note.adsr_vca.SetAttackTime(p.value / 128.0); // secs
              }
              break;
            case static_cast<int>(SynthControl::envelope_d_vca):
              {
                daisy::DaisySeed::Print("Control Received: VCA Decay -> 0.%03i\n", static_cast<int>(1000 * p.value / 128.0));
                for(auto& note : notes)
                  note.adsr_vca.SetDecayTime(p.value / 128.0); // secs
              }
              break;
            case static_cast<int>(SynthControl::envelope_s_vca):
              {
                daisy::DaisySeed::Print("Control Received: VCA Sustain -> 0.%03i\n", static_cast<int>(1000 * p.value / 128.0));
                for(auto& note : notes)
                  note.adsr_vca.SetSustainLevel(p.value / 128.0);
              }
              break;
            case static_cast<int>(SynthControl::envelope_r_vca):
              {
                daisy::DaisySeed::Print("Control Received: VCA Release -> 0.%03i\n", static_cast<int>(1000 * p.value / 128.0));
                for(auto& note : notes)
                  note.adsr_vca.SetReleaseTime(p.value / 128.0); // secs
              }
              break;
            case static_cast<int>(SynthControl::envelope_a_vcf):
              {
                daisy::DaisySeed::Print("Control Received: VCF Attack -> 0.%03i\n", static_cast<int>(1000 * p.value / 128.0));
                for(auto& note : notes)
                  note.adsr_vcf.SetAttackTime(p.value / 128.0); // secs
              }
              break;
            case static_cast<int>(SynthControl::envelope_d_vcf):
              {
                daisy::DaisySeed::Print("Control Received: VCF Decay -> 0.%03i\n", static_cast<int>(1000 * p.value / 128.0));
                for(auto& note : notes)
                  note.adsr_vcf.SetDecayTime(p.value / 128.0); // secs
              }
              break;
            case static_cast<int>(SynthControl::envelope_s_vcf):
              {
                daisy::DaisySeed::Print("Control Received: VCF Sustain -> 0.%03i\n", static_cast<int>(1000 * p.value / 128.0));
                for(auto& note : notes)
                  note.adsr_vcf.SetSustainLevel(p.value / 128.0);
              }
              break;
            case static_cast<int>(SynthControl::envelope_r_vcf):
              {
                daisy::DaisySeed::Print("Control Received: VCF Release -> 0.%03i\n", static_cast<int>(1000 * p.value / 128.0));
                for(auto& note : notes)
                  note.adsr_vcf.SetReleaseTime(p.value / 128.0); // secs
              }
              break;
            case static_cast<int>(SynthControl::mode_toggle):
              {
                // Buttons always get a press and a release event
                // only respond to the press
                if(p.value != 127)
                  break;

                if(mode == PlayerMode::keyboard)
                  mode = PlayerMode::arp;
                else if(mode == PlayerMode::arp)
                  mode = PlayerMode::keyboard;

                daisy::DaisySeed::Print("Control Received: Mode Toggle %s\n",
                    mode == PlayerMode::keyboard ? "keyboard" : "arp");

              }
              break;
            case static_cast<int>(SynthControl::arp_note_length):
              {
                arp.set_note_len(0.002 + 0.25 * p.value / 127.f);
                daisy::DaisySeed::Print("Control Received: Arp note length\n");
              }
              break;
            case static_cast<int>(SynthControl::arp_mode):
              {
                // Buttons always get a press and a release event
                // only respond to the press
                if(p.value != 127)
                  break;
                arp.next_mode();
                daisy::DaisySeed::Print("Control Received: Arp next mode\n");
              }
              break;
            default: 
              {
                daisy::DaisySeed::Print("Control Received: Not Mapped -> %f\n", p.control_number);
              }
              break;
          }
          break;
        }
      default: break;
    }
  }
};


// Main -- Init, and Midi Handling
int main(void)
{
  daisy::DaisyPod pod;
  // Init
  float samplerate;
  pod.Init();
  pod.SetAudioBlockSize(4);
  pod.seed.usb_handle.Init(daisy::UsbHandle::FS_INTERNAL);
  daisy::System::Delay(250);
  pod.seed.StartLog();

  // Synthesis
  samplerate = pod.AudioSampleRate();
  static Player player(samplerate, pod);

  // Start stuff.
  pod.StartAdc();
  pod.StartAudio([]
      (daisy::AudioHandle::InterleavingInputBuffer in, daisy::AudioHandle::InterleavingOutputBuffer out, size_t sz)
      {return player.AudioCallback(in, out, sz);});

  LCD lcd{};
  lcd.init();
  lcd.print("Hello World");
  daisy::DaisySeed::Print("lcd started");

  pod.midi.StartReceive();
  for(;;)
  {
    pod.midi.Listen();
    // Handle MIDI Events
    while(pod.midi.HasEvents())
    {
      player.HandleMidiMessage(pod.midi.PopEvent());
    }
    player.update();
  }
}
