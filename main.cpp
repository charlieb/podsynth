#include "daisy_pod.h"
#include "daisysp.h"
#include <cmath>
#include <string>
#include <util/MappedValue.h>
#include <cstdio>
#include <string.h>
#include <unordered_map>
#include <cstdarg>
#include <array>
#include <vector>
#include <cstring>

#include "player.h"
#include "arp.h"
#include "lcd.h"
#include "seq.h"

//#define LogPrint(...) daisy::DaisySeed::Print(__VA_ARGS__)
#define LogPrint(...) 

class TempoUtils
{
public:
  static float tempo_to_freq(uint8_t tempo) { return tempo / 60.0f; }
  static uint8_t freq_to_tempo(float freq) { return freq * 60.0f; }
  static float bpm_to_freq(uint32_t tempo) { return tempo / 60.0f; }
  static uint32_t ms_to_bpm(uint32_t ms) { return 60000 / ms; }
  static uint32_t us_to_bpm(uint32_t us) { return 60000000 / us; }

  static uint32_t fus_to_bpm(uint32_t us)
  {
    float fus = static_cast<float>(us);
    float val = std::roundf(60000000.0f / fus);
    return static_cast<uint32_t>(val);
  }
};

enum class SynthControl {
  wave_shape,
  seq_step_length,
  seq_pause_toggle,
  vcf_cutoff,
  vcf_resonance,
  vcf_envelope_depth,
  delay_time,
  delay_mix,
  reverb_damp_freq,
  reverb_feedback,
  reverb_wet,
  oscillator_detune,
  envelope_a_vca,
  envelope_d_vca,
  envelope_a_vcf,
  envelope_d_vcf,
  mode_toggle,
  arp_note_length,
  arp_mode,
  seq_step_add_del,
  Count
};

/* Arturia Minilab mkII

 click midi, turn midi
   o
 knob numbering   
       
 113,112  74   71   76   77   93   73   75
   O      o    o    o    o    o    o    o
   1      2    3    4    5    6    7    8  

 115,114  ?    19   16   17   91   79   72
   O      o    o    o    o    o    o    o
   9     10   11   12   13   14   15   16

 */

class Controller {
  // Map of daisy::ControlChangeEvent::control_number aka midi control number
  // to the synth control.
  // If you're hooking up your own controller, this is the place
  // to map your controls.
  std::unordered_map<uint8_t, SynthControl> midi_map {
      {112, SynthControl::seq_step_add_del}, // Knob 1 turn (value 64 ignore, 63 down, 65 up)
      {113, SynthControl::seq_pause_toggle}, // Knob 1 press
      //{114, SynthControl::arp_mode}, // Knob 9 turn (value 64 ignore, 63 down, 65 up)
      {115, SynthControl::arp_mode}, // Knob 9 press

      {74, SynthControl::wave_shape}, // Knob 2
      {18, SynthControl::seq_step_length}, // Knob 10

      {71, SynthControl::vcf_cutoff}, // Knob 3
      {19, SynthControl::vcf_resonance}, // Knob 11

      {76, SynthControl::vcf_envelope_depth}, // Knob 4
      {16, SynthControl::arp_note_length}, // Knob 12
                                           //
      {77, SynthControl::delay_mix}, // Knob 5
      {17, SynthControl::delay_time}, // Knob 13
      
      {91, SynthControl::reverb_feedback}, // Knob 6
      {93, SynthControl::reverb_wet}, // Knob 14

      {73, SynthControl::envelope_a_vca}, // Knob 7
      {79, SynthControl::envelope_a_vcf}, // Knob 15

      {75, SynthControl::envelope_d_vca}, // Knob 8
      {72, SynthControl::envelope_d_vcf}, // Knob 16
  };
  daisy::Color red;
  daisy::Color green;
  daisy::Color blue;
  float samplerate;
  Player& player;
  LCD& lcd;
  Seq& seq;
  Arp& arp;
  daisy::DaisyPod& pod;
  bool edit_mode = false;
  
  daisy::Parameter detune;

  char lcd_top[17]{0,};
  char lcd_bot[17]{0,};

  public:
  Controller(float samplerate, Player& player, Seq& seq, Arp& arp, LCD& lcd, daisy::DaisyPod& pod)
    : samplerate(samplerate)
    , player(player)
    , lcd(lcd)
    , seq(seq)
    , arp(arp)
     ,pod(pod)  {
    red.Init(1, 0, 0);
    green.Init(0, 1, 0);
    blue.Init(0, 1, 0);
    detune.Init(pod.knob1, 1., 2., daisy::Parameter::LINEAR);
    //p_inversion.Init(hw.knob2, 0, 5, Parameter::LINEAR);
  }

  void redraw() {    
    lcd.clear();
    lcd.setCursor(0,0);
    static std::string tmp{""};
    if(edit_mode) {
      tmp.clear();
      for(uint8_t note : seq.get_step().notes) {
        if(note < 10)
          tmp.push_back('0' + note);
        else
          tmp.push_back('A' + note - 10);
      }
      lcd.print(tmp);
      tmp.clear();

      lcd.setCursor(0, 1);
      for(int i = 0; i < seq.get_num_steps(); i++)
        if(i == seq.get_step_num())
            lcd.print("^");
        else
            lcd.print("-");
    }
    else {
      lcd.print(lcd_top);
      lcd.setCursor(0,1);
      lcd.print(lcd_bot);
    }

    //lcd.setCursor(lcd_seq_step % LCD::cols, lcd_seq_step >= LCD::cols ? 1 : 0);
    //lcd.cursor_on();
    //lcd.blink_on();
  }

  // Returns whether to redraw or not
  bool HandlePodControls() {
    pod.ProcessDigitalControls();
    pod.ProcessAnalogControls();
    bool redraw{false};

    // Knob 1 is for osc detune
    static float dt{0};
    float new_dt = detune.Process();
    if(fabs(dt - new_dt) > 0.001) {
      std::sprintf(lcd_top, "Detune %.3i", static_cast<int>(1000 * dt));
      redraw = true;
      dt = new_dt;
      player.set_detune(dt);
    }

    // Encoder turns select the current step
    int32_t inc = pod.encoder.Increment();
    if(inc && edit_mode) {
      seq.step_inc(inc);
      seq.set_arp(arp);
      LogPrint("Step Inc %i -> %i\n", inc, seq.get_step_num());
      redraw = true;
    }
    // Encoder Button Release captures the keys as the keys for this step
    if(pod.encoder.RisingEdge()) {
      edit_mode = !edit_mode;
      if(edit_mode)
        seq.pause();
      else
        seq.unpause();
      redraw = true;
    }
    // Button 1 removes the last note from the current arp
    if(pod.button1.RisingEdge()) {
      if(edit_mode) {
        seq.pop_note();
        seq.set_arp(arp);
      }
      redraw = true;
    }
    // Button 2 inserts a rest
    if(pod.button2.RisingEdge()) {
      if(edit_mode) {
        seq.push_note(127);
        seq.set_arp(arp);
      }
      //LogPrint("Pod Button2 Click\n");
      //seq.step(lcd_seq_step).active = !seq.step(lcd_seq_step).active;
      redraw = true;
    }
    
    return redraw;
  }

  bool HandleMidiMessage(daisy::MidiEvent m)
  {
    bool redraw = false;
    char tmp[25]{0,};
    static daisy::MappedFloatValue vcf_freq_map{
      100, samplerate / 3  + 1, 440,
        daisy::MappedFloatValue::Mapping::log, "Hz"};

    switch(m.type) {
      case daisy::NoteOn: 
        {
        //keys.press(m.AsNoteOn());
          daisy::NoteOnEvent n{m.AsNoteOn()};
          if(edit_mode) {
            seq.push_note(n.note);
            seq.set_arp(arp);
          }
          redraw = true;
        }
        break;
      case daisy::NoteOff: 
        //keys.release(m.AsNoteOn());
        break;
      case daisy::ControlChange: 
        {
          daisy::ControlChangeEvent p = m.AsControlChange();
          LogPrint("Control Received:\t%d\t%d -> %i / %i\n", p.control_number, p.value, midi_map[p.control_number], SynthControl::wave_shape);
          switch(static_cast<int>(midi_map[p.control_number])) {
            case static_cast<int>(SynthControl::wave_shape): 
              {
              int wave_num{static_cast<uint8_t>(8 * p.value / 127.0)};
              wave_name(tmp, wave_num);
              std::sprintf(lcd_bot, "Shape %s", tmp);
              redraw = true;
              player.set_wave_shape(wave_num);
              }
              break;
            case static_cast<int>(SynthControl::seq_pause_toggle): 
              {
                seq.pause_toggle();
              }
              break;
            case static_cast<int>(SynthControl::seq_step_length): 
              {
                seq.set_tempo(0.002 + 5. * p.value / 127.f);
                std::sprintf(lcd_top, "Seq Tempo %.3i", static_cast<int>(1000 * (0.002 + 5. * p.value / 127.f)));
                redraw = true;
              }
              break;
            case static_cast<int>(SynthControl::oscillator_detune): 
              //player.set_oscillator_detune(p.value);
              break;
            case static_cast<int>(SynthControl::vcf_cutoff): 
              {
                vcf_freq_map.SetFrom0to1(p.value / 127.0);
              std::sprintf(lcd_top, "VCF C %i", static_cast<int>(vcf_freq_map.Get()));
              redraw = true;
              player.set_vcf_cutoff(p.value / 127.0);
              }
              break;
            case static_cast<int>(SynthControl::vcf_resonance):
              {
              std::sprintf(lcd_top, "VCF R %.3i", static_cast<int>(1000 * (1. + p.value) / 129.0));
              redraw = true;
              player.set_vcf_resonance((1.0 + p.value) / 129.0);
              }
              break;
            case static_cast<int>(SynthControl::vcf_envelope_depth):
              {
              std::sprintf(lcd_top, "VCF Env %.3i", static_cast<int>(1000 * p.value / 127.0));
              redraw = true;
              player.set_vcf_envelope_depth(p.value / 127.0);
              }
              break;
            case static_cast<int>(SynthControl::envelope_a_vca):
              {
              std::sprintf(lcd_top, "VCA Env A %.3i", static_cast<int>(1000 * p.value / 127.0));
              redraw = true;
              player.set_envelope_a_vca(0.007 + p.value / 127.0);
              }
              break;
            case static_cast<int>(SynthControl::envelope_d_vca):
              {
              std::sprintf(lcd_top, "VCA Env D %.3i", static_cast<int>(1000 * p.value / 127.0));
              redraw = true;
              player.set_envelope_d_vca(0.007 + p.value / 127.0);
              }
              break;
            case static_cast<int>(SynthControl::envelope_a_vcf):
              {
              std::sprintf(lcd_top, "VCF Env A %.3i", static_cast<int>(1000 * p.value / 127.0));
              redraw = true;
              player.set_envelope_a_vcf(0.007 + p.value / 127.0);
              }
              break;
            case static_cast<int>(SynthControl::envelope_d_vcf):
              {
              std::sprintf(lcd_top, "VCF Env D %.3i", static_cast<int>(1000 * p.value / 127.0));
              redraw = true;
              player.set_envelope_d_vcf(0.007 + p.value / 127.0);
              }
              break;
            case static_cast<int>(SynthControl::mode_toggle):
              {
              // Buttons always get a press and a release event
              // only respond to the press
              if(p.value != 127)
                break;
              }
              break;
            case static_cast<int>(SynthControl::arp_note_length):
              {
                arp.set_note_len(0.002 + 0.25 * p.value / 127.f);
                std::sprintf(lcd_top, "Arp Len %.3i", static_cast<int>(1000 * (0.002 + 0.25 * p.value / 127.f)));
                redraw = true;
              }
              break;
            case static_cast<int>(SynthControl::arp_mode):
              {
                // Buttons always get a press and a release event
                // only respond to the press
                if(p.value != 127)
                  break;
                arp.next_mode();
                std::sprintf(lcd_bot, "Arp ");
                arp.mode_name(lcd_bot + 4);
                redraw = true;
              }
              break;
            case static_cast<int>(SynthControl::seq_step_add_del):
              {
                if(p.value == 65) {
                  seq.add_step();
                } else if(p.value == 63) {
                  seq.del_step();
                }
                LogPrint("Step add/remove %i\n", seq.get_num_steps());
                redraw = true;
              }
              break;
            case static_cast<int>(SynthControl::delay_time):
              {
                // Delay time is in samples
                player.set_delay_time(samplerate * p.value / 127.0);
                std::sprintf(lcd_top, "Delay T %.3i", static_cast<int>(samplerate * p.value / 127.));
                redraw = true;
              }
              break;
            case static_cast<int>(SynthControl::delay_mix):
              {
                player.set_delay_mix(p.value / 127.0);
                std::sprintf(lcd_top, "Delay Mix %.3i", static_cast<int>(1000 * p.value / 127.f));
                redraw = true;
              }
              break;
            case static_cast<int>(SynthControl::reverb_feedback):
              {
                static daisy::MappedFloatValue rv_freq_map{
                  100, samplerate / 3  + 1, 440,
                    daisy::MappedFloatValue::Mapping::log, "Hz"};
                rv_freq_map.SetFrom0to1(p.value / 127.0);
                player.set_reverb_feedback(p.value / 127.0);
                std::sprintf(lcd_top, "Rev FB %.4i", static_cast<int>(rv_freq_map.Get()));
                redraw = true;
              }
              break;
            case static_cast<int>(SynthControl::reverb_damp_freq):
              {
                //player.set_reverb_damp_freq(p.value / 127.0);
                std::sprintf(lcd_top, "Rev Damp %.3i", static_cast<int>(1000 * p.value / 127.f));
                redraw = true;
              }
              break;
            case static_cast<int>(SynthControl::reverb_wet):
              {
                player.set_reverb_wet(p.value / 127.0);
                std::sprintf(lcd_top, "Rev wet %.3i", static_cast<int>(1000 * p.value / 127.f));
                redraw = true;
              }
              break;
            default: 
              {
                LogPrint("Control Received: Not Mapped -> %f\n", p.control_number);
              }
              break;
          }
          break;
        }
      default: break;
    }
    return redraw;
  }
};

void AudioCallback(daisy::AudioHandle::InterleavingInputBuffer in,
    daisy::AudioHandle::InterleavingOutputBuffer out, size_t size,
    Player& player, Arp& arp, Seq& seq) {

  constexpr int TEMPO_MIN{30};
  constexpr int TEMPO_DEFAULT{120};
  constexpr int TEMPO_MAX{240};

  constexpr float threshold = 0.20f;

  // sync tempo variables
  static uint32_t prev_timestamp = 0;
  static uint8_t tempo = TEMPO_DEFAULT;
  static float left_cached = 0;

  for (size_t i = 0; i < size; i += 2) {
    // left - sync
    float left = in[i];
    //float right = in[i+1];

    //hw.led1.Set(left, left, left);
    //hw.led2.Set(right, right, right);
    //hw.UpdateLeds();

    if (fabs(left - left_cached) > threshold) {
      // detect sync raising edge
      // Single pulse, 2.5ms long, with an amplitude of 1V above ground reference.
      if (left_cached < threshold && left > threshold) {
        uint32_t now = daisy::System::GetUs();
        uint32_t diff = now - prev_timestamp;
        uint32_t bpm = TempoUtils::fus_to_bpm(diff) / 2;

        if (bpm >= TEMPO_MIN && bpm <= TEMPO_MAX) {
          tempo = bpm;
          arp.set_note_len(60. / (tempo * 8.));
        }
        prev_timestamp = now;
      }
      left_cached = left;
    }

  }

  seq.process();
  arp.process();
  player.AudioCallback(in, out, size);
}


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

  constexpr uint32_t lcd_delay_ms{250};
  uint32_t last_t{0};
  LCD lcd{};
  lcd.init();
  lcd.backlight_on();
  lcd.print("Starting ...");
  LogPrint("lcd started\n");

  samplerate = pod.AudioSampleRate();
  static Seq seq(samplerate);
  static Arp arp(samplerate); 
  arp.set_note_len(0.0125);
  static Player player(samplerate);
  static Controller controller(samplerate, player, seq, arp, lcd, pod);

  // Start stuff.
  pod.StartAdc();
  pod.StartAudio([](daisy::AudioHandle::InterleavingInputBuffer in,
                    daisy::AudioHandle::InterleavingOutputBuffer out,
                    size_t size)
      {AudioCallback(in, out, size, player, arp, seq);});
  pod.midi.StartReceive();
  bool redraw{false};
  for(;;)
  {
    pod.midi.Listen();
    // Handle MIDI Events
    while(pod.midi.HasEvents())
    {
      redraw |= controller.HandleMidiMessage(pod.midi.PopEvent());
    }
    redraw |= controller.HandlePodControls();
    if(redraw && daisy::System::GetNow() > last_t + lcd_delay_ms) {
      controller.redraw();
      redraw = false;
      last_t = daisy::System::GetNow();
    }
    arp.update(player);
    seq.update(player, arp);
  }
}
