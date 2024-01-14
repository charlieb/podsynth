#include "daisy_pod.h"
#include "daisysp.h"
#include <util/MappedValue.h>
#include <cstdio>
#include <string.h>
#include <unordered_map>
#include <cstdarg>
#include <array>
#include <vector>

#include "player.h"
#include "arp.h"
#include "lcd.h"
#include "seq.h"

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

class Controller {
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
  daisy::Color red;
  daisy::Color green;
  daisy::Color blue;
  Player& player;
  LCD& lcd;
  Seq<Player::poly>& seq;
  daisy::DaisyPod& pod;

  // This is the current step for editing only, not for playing
  uint8_t lcd_seq_step{0};

  public:
  Controller(Player& player, Seq<Player::poly>& seq, LCD& lcd, daisy::DaisyPod& pod)
    : player(player)
    , lcd(lcd)
    , seq(seq)
     ,pod(pod)  {
    red.Init(1, 0, 0);
    green.Init(0, 1, 0);
    blue.Init(0, 1, 0);
  }

  char step_letter(Step& step) {
    if(step.active) 
      return step.mode == StepMode::chord ? 'C' : 'A';
    else
      return '_';
  }

  void redraw() {
    char top_letters[Seq<Player::poly>::nsteps + 1]{0,};
    char bot_letters[Seq<Player::poly>::nsteps + 1]{0,};

    // A for Arp, C for Chord (or C(k)eys)
    for(int i = 0; i < Seq<Player::poly>::nsteps / 2; i++)
      top_letters[i] = step_letter(seq.step(i));
    for(int i = Seq<Player::poly>::nsteps / 2; i < Seq<Player::poly>::nsteps; i++)
      bot_letters[i - Seq<Player::poly>::nsteps / 2] = step_letter(seq.step(i));
   
    
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print(top_letters);
    lcd.setCursor(0,1);
    lcd.print(bot_letters);

    lcd.setCursor(lcd_seq_step % LCD::cols, lcd_seq_step >= LCD::cols ? 1 : 0);
    lcd.cursor_on();
    lcd.blink_on();
  }

  // Returns whether to redraw or not
  bool HandlePodControls() {
    pod.ProcessDigitalControls();
    bool redraw{false};
    // Encoder turns select the current step
    int32_t inc = pod.encoder.Increment();
    if(inc) {
      lcd_seq_step += inc;
      lcd_seq_step = lcd_seq_step % Seq<Player::poly>::nsteps;
      redraw = true;
      daisy::DaisySeed::Print("Pod Encoder Increment -> %u\n", lcd_seq_step);
    }
    // Encoder Button Release captures the keys as the keys for this step
    if(pod.encoder.FallingEdge()) {
      daisy::DaisySeed::Print("Pod Encoder Click\n");
      seq.step(lcd_seq_step).keys.clear();
      auto keys{player.get_keys()};
      for(auto k : keys)
        seq.step(lcd_seq_step).keys.push_back(k);
      redraw = true;
    }
    // Button 1 switches between arp and chord mode for that step
    if(pod.button1.RisingEdge()) {
      daisy::DaisySeed::Print("Pod Button1 Click\n");
      seq.step(lcd_seq_step).mode = seq.step(lcd_seq_step).mode == StepMode::chord ? StepMode::arp : StepMode::chord;
      redraw = true;
    }
    // Button 2 switches step off and on
    if(pod.button2.RisingEdge()) {
      daisy::DaisySeed::Print("Pod Button2 Click\n");
      seq.step(lcd_seq_step).active = !seq.step(lcd_seq_step).active;
      redraw = true;
    }
    
    return redraw;
  }

  // Typical Switch case for Message Type.
  void HandleMidiMessage(daisy::MidiEvent m)
  {
    switch(m.type) {
      case daisy::NoteOn:
        player.key_pressed(m.AsNoteOn());
        break;
      case daisy::NoteOff: 
        player.key_released(m.AsNoteOn());
        break;
      case daisy::ControlChange: 
        {
          daisy::ControlChangeEvent p = m.AsControlChange();
          daisy::DaisySeed::Print("Control Received:\t%d\t%d -> %i / %i\n", p.control_number, p.value, midi_map[p.control_number], SynthControl::wave_shape);
          switch(static_cast<int>(midi_map[p.control_number])) {
            case static_cast<int>(SynthControl::wave_shape): 
              player.set_wave_shape(static_cast<uint8_t>(8 * p.value / 127.0));
              break;
            case static_cast<int>(SynthControl::vcf_cutoff): 
              player.set_vcf_cutoff(p.value / 127.0);
              break;
            case static_cast<int>(SynthControl::vcf_resonance):
              player.set_vcf_resonance(p.value / 127.0);
              break;
            case static_cast<int>(SynthControl::vcf_envelope_depth):
              player.set_vcf_envelope_depth(10'000 * p.value / 127.0);
              break;
            case static_cast<int>(SynthControl::vca_bias):
              player.set_vca_bias(p.value / 127.0);
              break;
            case static_cast<int>(SynthControl::envelope_a_vca):
              player.set_envelope_a_vca(p.value / 127.0);
              break;
            case static_cast<int>(SynthControl::envelope_d_vca):
              player.set_envelope_d_vca(p.value / 127.0);
              break;
            case static_cast<int>(SynthControl::envelope_s_vca):
              player.set_envelope_s_vca(p.value / 127.0);
              break;
            case static_cast<int>(SynthControl::envelope_r_vca):
              player.set_envelope_r_vca(p.value / 127.0);
              break;
            case static_cast<int>(SynthControl::envelope_a_vcf):
              player.set_envelope_a_vcf(p.value / 127.0);
              break;
            case static_cast<int>(SynthControl::envelope_d_vcf):
              player.set_envelope_d_vcf(p.value / 127.0);
              break;
            case static_cast<int>(SynthControl::envelope_s_vcf):
              player.set_envelope_s_vcf(p.value / 127.0);
              break;
            case static_cast<int>(SynthControl::envelope_r_vcf):
              player.set_envelope_r_vcf(p.value / 127.0);
              break;
            case static_cast<int>(SynthControl::mode_toggle):
              {
              // Buttons always get a press and a release event
              // only respond to the press
              if(p.value != 127)
                break;

              char new_mode_name[16]{};
              if(player.get_mode() == PlayerMode::keyboard)
                player.set_mode(PlayerMode::arp);
              else if(player.get_mode() == PlayerMode::arp)
                player.set_mode(PlayerMode::seq);
              else if(player.get_mode() == PlayerMode::seq)
                player.set_mode(PlayerMode::keyboard);

              playermode_name(new_mode_name, player.get_mode());
              lcd.clear();
              lcd.print(new_mode_name);
              }
              break;
            case static_cast<int>(SynthControl::arp_note_length):
                player.set_arp_note_len(0.002 + 0.25 * p.value / 127.f);
              break;
            case static_cast<int>(SynthControl::arp_mode):
                // Buttons always get a press and a release event
                // only respond to the press
                if(p.value != 127)
                  break;
                player.next_arp_mode();
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
  LCD lcd{};
  lcd.init();
  lcd.backlight_on();
  lcd.print("Starting ...");
  daisy::DaisySeed::Print("lcd started\n");

  samplerate = pod.AudioSampleRate();
  static Seq<Player::poly> seq(samplerate);
  seq.randomize();
  static Player player(samplerate, seq);
  static Controller controller(player, seq, lcd, pod);

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
      controller.HandleMidiMessage(pod.midi.PopEvent());
    }
    player.update();
    if(controller.HandlePodControls()) {
      controller.redraw();
    }
  }
}
