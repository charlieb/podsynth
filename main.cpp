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

static daisy::Logger<daisy::LOGGER_INTERNAL> logger;

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
      {16, SynthControl::vca_bias}, // Knob 12
      {77, SynthControl::envelope_a_vca}, // Knob 5
      {93, SynthControl::envelope_d_vca}, // Knob 6
      {73, SynthControl::envelope_s_vca}, // Knob 7
      {75, SynthControl::envelope_r_vca}, // Knob 8
      {17, SynthControl::envelope_a_vcf}, // Knob 13
      {91, SynthControl::envelope_d_vcf}, // Knob 14
      {79, SynthControl::envelope_s_vcf}, // Knob 15
      {72, SynthControl::envelope_r_vcf}, // Knob 16
      {113, SynthControl::mode_toggle}, // Knob 16
  };


  float vcf_env_depth = 0.;
  float vca_bias = 0.;
  daisy::MappedFloatValue vcf_freq{100, 20'000, 440, daisy::MappedFloatValue::Mapping::log, "Hz"};
  float vcf_res = 0;

  static constexpr int poly{6};
  // This initlizer kinda sucks, boo c++
  std::array<Note, poly> notes{{
    {vcf_env_depth, vcf_freq, vcf_res, logger},
      {vcf_env_depth, vcf_freq, vcf_res, logger},
      {vcf_env_depth, vcf_freq, vcf_res, logger},
      {vcf_env_depth, vcf_freq, vcf_res, logger},
      {vcf_env_depth, vcf_freq, vcf_res, logger},
      {vcf_env_depth, vcf_freq, vcf_res, logger}
  }};

  std::vector<daisy::NoteOnEvent> keys;
  PlayerMode mode{PlayerMode::keyboard};

  // ARP stuff
  daisysp::Metro tick{};
  bool arp_next{false};

  public:

  Player(float samplerate) {
    for(auto& note : notes)
      note.init(samplerate);
    tick.Init(1.0, samplerate);
    set_arp_length(0.05125);
  }

  void set_arp_length(float secs) {
    tick.SetFreq(1.0 / secs);
  }

  void update() {
    switch(static_cast<int>(mode)) {
      case static_cast<int>(PlayerMode::keyboard):
        keyboard_update();
        break;
      case static_cast<int>(PlayerMode::arp):
        arp_update();
        break;
    }
  }

  void arp_update() {
    static int arp_select{0};
    if(!arp_next) return;
    arp_next = false;
    // turn all the notes off
    if(keys.size() <= 0) {
      for(auto& note : notes)
        note.note_off();
      return;
    }

    arp_select = (arp_select + 1) % keys.size();

    //logger.Print("Player.arp_update selected: %i\n", arp_select);

    for(size_t i = 1; i < keys.size(); i++)
      notes[i].note_off();
    notes[0].note_on(keys[arp_select]);
    notes[0].retrigger();
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

      logger.Print("Player.update Failed to add note: %i\n", key.note);
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
    if(!arp_next)
      arp_next = tick.Process();
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
          logger.Print("Note On:\t%d\t%d\t%d\n", m.channel, m.data[0], m.data[1]);
          key_pressed(m.AsNoteOn());
        }
        break;
      case daisy::NoteOff: 
        {
          logger.Print("Note Off:\t%d\t%d\t%d\n", m.channel, m.data[0], m.data[1]);
          key_released(m.AsNoteOn());
        }
        break;
      case daisy::ControlChange: 
        {
          daisy::ControlChangeEvent p = m.AsControlChange();
          logger.Print("Control Received:\t%d\t%d -> %i / %i\n", p.control_number, p.value, midi_map[p.control_number], SynthControl::wave_shape);
          switch(static_cast<int>(midi_map[p.control_number])) {
            case static_cast<int>(SynthControl::wave_shape):
              {
                char tmp[25]{0,};
                uint8_t wave_num = static_cast<uint8_t>(8 * p.value / 127);
                wave_name(tmp, wave_num);
                logger.Print("Control Received: Waveform %i: %s\n",wave_num, tmp);
                for(auto& note : notes) {
                  note.osc.SetWaveform(static_cast<uint8_t>(p.value / 9));
                }
              }
              break;
            case static_cast<int>(SynthControl::vcf_cutoff):
              {
                vcf_freq.SetFrom0to1(p.value / 128.0);
                logger.Print("Control Received: vcf_freq -> %.02f\n", vcf_freq);
              }
              break;
            case static_cast<int>(SynthControl::vcf_resonance):
              {
                vcf_res = p.value / 128.0;
                logger.Print("Control Received: vcf_res -> %f\n", vcf_res);
              }
              break;
            case static_cast<int>(SynthControl::vcf_envelope_depth):
              {
                vcf_env_depth = 10'000 * p.value / 128.0;
                logger.Print("Control Received: vcf_env_depth -> %i\n", static_cast<int>(vcf_env_depth));
              }
              break;
            case static_cast<int>(SynthControl::vca_bias):
              {
                vca_bias = p.value / 128.0;
                logger.Print("Control Received: vca_bias -> %f\n", vca_bias);
              }
              break;
            case static_cast<int>(SynthControl::envelope_a_vca):
              {
                logger.Print("Control Received: VCA Attack -> 0.%03i\n", static_cast<int>(1000 * p.value / 128.0));
                for(auto& note : notes)
                  note.adsr_vca.SetAttackTime(p.value / 128.0); // secs
              }
              break;
            case static_cast<int>(SynthControl::envelope_d_vca):
              {
                logger.Print("Control Received: VCA Decay -> 0.%03i\n", static_cast<int>(1000 * p.value / 128.0));
                for(auto& note : notes)
                  note.adsr_vca.SetDecayTime(p.value / 128.0); // secs
              }
              break;
            case static_cast<int>(SynthControl::envelope_s_vca):
              {
                logger.Print("Control Received: VCA Sustain -> 0.%03i\n", static_cast<int>(1000 * p.value / 128.0));
                for(auto& note : notes)
                  note.adsr_vca.SetSustainLevel(p.value / 128.0);
              }
              break;
            case static_cast<int>(SynthControl::envelope_r_vca):
              {
                logger.Print("Control Received: VCA Release -> 0.%03i\n", static_cast<int>(1000 * p.value / 128.0));
                for(auto& note : notes)
                  note.adsr_vca.SetReleaseTime(p.value / 128.0); // secs
              }
              break;
            case static_cast<int>(SynthControl::envelope_a_vcf):
              {
                logger.Print("Control Received: VCF Attack -> 0.%03i\n", static_cast<int>(1000 * p.value / 128.0));
                for(auto& note : notes)
                  note.adsr_vcf.SetAttackTime(p.value / 128.0); // secs
              }
              break;
            case static_cast<int>(SynthControl::envelope_d_vcf):
              {
                logger.Print("Control Received: VCF Decay -> 0.%03i\n", static_cast<int>(1000 * p.value / 128.0));
                for(auto& note : notes)
                  note.adsr_vcf.SetDecayTime(p.value / 128.0); // secs
              }
              break;
            case static_cast<int>(SynthControl::envelope_s_vcf):
              {
                logger.Print("Control Received: VCF Sustain -> 0.%03i\n", static_cast<int>(1000 * p.value / 128.0));
                for(auto& note : notes)
                  note.adsr_vcf.SetSustainLevel(p.value / 128.0);
              }
              break;
            case static_cast<int>(SynthControl::envelope_r_vcf):
              {
                logger.Print("Control Received: VCF Release -> 0.%03i\n", static_cast<int>(1000 * p.value / 128.0));
                for(auto& note : notes)
                  note.adsr_vcf.SetReleaseTime(p.value / 128.0); // secs
              }
              break;
            case static_cast<int>(SynthControl::mode_toggle):
              {
                if(p.value != 127)
                  break;

                if(mode == PlayerMode::keyboard)
                  mode = PlayerMode::arp;
                else if(mode == PlayerMode::arp)
                  mode = PlayerMode::keyboard;

                logger.Print("Control Received: Mode Toggle %s\n",
                    mode == PlayerMode::keyboard ? "keyboard" : "arp");
              }
              break;
            default: 
              {
                logger.Print("Control Received: Not Mapped -> %f\n", p.control_number);
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

  static daisy::DaisyPod pod;
  // Init
  float samplerate;
  pod.Init();
  pod.SetAudioBlockSize(4);
  pod.seed.usb_handle.Init(daisy::UsbHandle::FS_INTERNAL);
  daisy::System::Delay(250);
  logger.StartLog();

  // Synthesis
  samplerate = pod.AudioSampleRate();
  static Player player(samplerate);

  // Start stuff.
  pod.StartAdc();
  pod.StartAudio([]
      (daisy::AudioHandle::InterleavingInputBuffer in, daisy::AudioHandle::InterleavingOutputBuffer out, size_t sz)
      {return player.AudioCallback(in, out, sz);});

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
