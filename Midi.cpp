#include "daisy_pod.h"
#include "daisysp.h"
#include <cstdio>
#include <string.h>
#include <unordered_map>
#include <cstdarg>
#include <array>

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
  Count
};

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
};

class SignalChain {
  private:
    // Some globals we just maintain references to
    float& vcf_env_depth;
    float& vcf_freq;
    float& vcf_res;

  public:
    daisysp::Oscillator osc;
    bool gate{false};
    uint8_t note{0};

    daisysp::MoogLadder flt;
    daisysp::Adsr adsr_vca, adsr_vcf;

    SignalChain(float& vcf_env_depth, float& vcf_freq, float& vcf_res)
      : vcf_env_depth(vcf_env_depth)
        , vcf_freq(vcf_freq)
        , vcf_res(vcf_res){
          logger.Print("SignalChain Init\n");
        };

    void init(float samplerate) {
      osc.Init(samplerate);
      osc.SetWaveform(daisysp::Oscillator::WAVE_POLYBLEP_SAW);
      osc.SetAmp(1.0);

      flt.Init(samplerate);

      adsr_vca.Init(samplerate);
      adsr_vcf.Init(samplerate);
    };

    bool note_match(daisy::NoteOnEvent& p) {
      logger.Print("SignalChain note match %i\n", note);
      return note == p.note; }
    bool note_match(daisy::NoteOffEvent& p) { 
      logger.Print("SignalChain note match %i\n", note);
      return note == p.note; }

    // Returns true if it claims the event
    bool note_on(daisy::NoteOnEvent& p) {
      // if gate is off we're not currently allocated
      // This is probably not a super safe assumption
      // given the release time but eh, one thing at a time
      if(!gate || p.note == note) {
        osc.SetFreq(daisysp::mtof(p.note));
        osc.SetAmp((p.velocity / 127.0f));
        gate = true;
        note = p.note;
        logger.Print("SignalChain note on %i\n", note);
        return true;
      }
      return false;
    }

    bool note_off(daisy::NoteOffEvent& p) {
      logger.Print("SignalChain note off %i\n", note);
      if(p.note == note) {
        gate = false;
        return true;
      }
      return false;
    }

    float process() {
      float vcf_env = adsr_vcf.Process(gate);
      flt.SetFreq(vcf_freq + vcf_env * vcf_env_depth);
      flt.SetRes(vcf_res);
      float vca_env = adsr_vca.Process(gate);

      // Apply the processing values
      float sig = osc.Process();
      sig = flt.Process(sig);
      sig *= vca_env;
      return sig;
    }
};

static float vcf_env_depth = 0.;
static float vca_env_depth = 0.;
static float vca_bias = 0.;
static float vcf_freq = 440;
static float vcf_res = 0;

static daisy::DaisyPod pod;
constexpr int poly{6};
// This initlizer kinda sucks, boo c++
std::array<SignalChain, poly> sigs{{
  {vcf_env_depth, vcf_freq, vcf_res},
  {vcf_env_depth, vcf_freq, vcf_res},
  {vcf_env_depth, vcf_freq, vcf_res},
  {vcf_env_depth, vcf_freq, vcf_res},
  {vcf_env_depth, vcf_freq, vcf_res},
  {vcf_env_depth, vcf_freq, vcf_res}
}};

//static SignalChain sig{vcf_env_depth, vcf_freq, vcf_res};


void AudioCallback(daisy::AudioHandle::InterleavingInputBuffer  in,
    daisy::AudioHandle::InterleavingOutputBuffer out,
    size_t                                size)
{
  float sig_total{0};
  for(size_t i = 0; i < size; i += 2)
  {
    for(auto& sig : sigs)
      sig_total += sig.process();
    out[i] = out[i + 1] = sig_total / poly;
  }
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
  bool claimed = false;
  switch(m.type) {
    case daisy::NoteOn:
      {
        logger.Print("Note On:\t%d\t%d\t%d\n", m.channel, m.data[0], m.data[1]);

        daisy::NoteOnEvent on = m.AsNoteOn();
        for(auto& sig : sigs)
          logger.Print("Note : %i / %i %i\n", on.note, sig.note, sig.gate);
        // This is to avoid Max/MSP Note outs for now..
        if(m.data[1] != 0)
        {
          claimed = false;
          // First try to find an oscillator already
          // playing this note
          for(auto& sig : sigs) {
            if(sig.note_match(on)) {
              sig.note_on(on);
              claimed = true;
              break;
            }
          }
          logger.Print("Note match %i\n", claimed);
          // No oscillator is already playing this
          // note, perhaps there's one not playing
          // any note?
          if(!claimed) {
            for(auto& sig : sigs) {
              if(sig.note_on(on)) {
                claimed = true;
                break;
              }
            }
          }
          logger.Print("Note match %i\n", claimed);
          for(auto& sig : sigs) 
            logger.Print("Note %i\n", sig.note);
        }
      }
      break;
    case daisy::NoteOff: 
      {
        logger.Print("Note Off:\t%d\t%d\t%d\n", m.channel, m.data[0], m.data[1]);
        daisy::NoteOffEvent off = m.AsNoteOff();
        claimed = false;
        for(auto& sig : sigs) {
          if(sig.note_off(off)) {
            claimed = true;
            break;
          }
        }
        logger.Print("Note match %i\n", claimed);
      }
      break;
    case daisy::ControlChange: 
      {
        daisy::ControlChangeEvent p = m.AsControlChange();
        logger.Print("Control Received:\t%d\t%d -> %i / %i\n", p.control_number, p.value, midi_map[p.control_number], SynthControl::wave_shape);
        switch(midi_map[p.control_number]) {
          case SynthControl::wave_shape: 
            {
              char tmp[20]{0,};
              uint8_t wave_num = static_cast<uint8_t>(8 * p.value / 127);
              wave_name(tmp, wave_num);
              logger.Print("Control Received: Waveform %i: %s\n",wave_num, tmp);
              for(auto& sig : sigs) {
                sig.osc.SetWaveform(static_cast<uint8_t>(p.value / 9));
              }
            }
            break;
          case SynthControl::vcf_cutoff:
            {
              vcf_freq = 100 + 10'000 * (p.value / 128.0);
              logger.Print("Control Received: vcf_freq -> %d\n", static_cast<int>(vcf_freq));
            }
            break;
          case SynthControl::vcf_resonance:
            {
              vcf_res = p.value / 128.0;
              logger.Print("Control Received: vcf_res -> %f\n", vcf_res);
            }
            break;
          case SynthControl::vcf_envelope_depth:
            {
              vcf_env_depth = 10'000 * p.value / 128.0;
              logger.Print("Control Received: vcf_env_depth -> %i\n", static_cast<int>(vcf_env_depth));
            }
            break;
          case SynthControl::vca_bias:
            {
              vca_bias = p.value / 128.0;
              logger.Print("Control Received: vca_bias -> %f\n", vca_bias);
            }
            break;
          case SynthControl::envelope_a_vca:
            {
              logger.Print("Control Received: VCA Attack -> 0.%03i\n", static_cast<int>(1000 * p.value / 128.0));
              for(auto& sig : sigs)
                sig.adsr_vca.SetAttackTime(p.value / 128.0); // secs
            }
            break;
          case SynthControl::envelope_d_vca:
            {
              logger.Print("Control Received: VCA Decay -> 0.%03i\n", static_cast<int>(1000 * p.value / 128.0));
              for(auto& sig : sigs)
                sig.adsr_vca.SetDecayTime(p.value / 128.0); // secs
            }
            break;
          case SynthControl::envelope_s_vca:
            {
              logger.Print("Control Received: VCA Sustain -> 0.%03i\n", static_cast<int>(1000 * p.value / 128.0));
              for(auto& sig : sigs)
                sig.adsr_vca.SetSustainLevel(p.value / 128.0);
            }
            break;
          case SynthControl::envelope_r_vca:
            {
              logger.Print("Control Received: VCA Release -> 0.%03i\n", static_cast<int>(1000 * p.value / 128.0));
              for(auto& sig : sigs)
                sig.adsr_vca.SetReleaseTime(p.value / 128.0); // secs
            }
            break;
          case SynthControl::envelope_a_vcf:
            {
              logger.Print("Control Received: VCF Attack -> 0.%03i\n", static_cast<int>(1000 * p.value / 128.0));
              for(auto& sig : sigs)
                sig.adsr_vcf.SetAttackTime(p.value / 128.0); // secs
            }
            break;
          case SynthControl::envelope_d_vcf:
            {
              logger.Print("Control Received: VCF Decay -> 0.%03i\n", static_cast<int>(1000 * p.value / 128.0));
              for(auto& sig : sigs)
                sig.adsr_vcf.SetDecayTime(p.value / 128.0); // secs
            }
            break;
          case SynthControl::envelope_s_vcf:
            {
              logger.Print("Control Received: VCF Sustain -> 0.%03i\n", static_cast<int>(1000 * p.value / 128.0));
              for(auto& sig : sigs)
                sig.adsr_vcf.SetSustainLevel(p.value / 128.0);
            }
            break;
          case SynthControl::envelope_r_vcf:
            {
              logger.Print("Control Received: VCF Release -> 0.%03i\n", static_cast<int>(1000 * p.value / 128.0));
              for(auto& sig : sigs)
                sig.adsr_vcf.SetReleaseTime(p.value / 128.0); // secs
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


// Main -- Init, and Midi Handling
int main(void)
{
  // Init
  float samplerate;
  pod.Init();
  pod.SetAudioBlockSize(4);
  pod.seed.usb_handle.Init(daisy::UsbHandle::FS_INTERNAL);
  daisy::System::Delay(250);
  logger.StartLog();

  // Synthesis
  samplerate = pod.AudioSampleRate();
  for(auto& sig : sigs)
    sig.init(samplerate);

  // Start stuff.
  pod.StartAdc();
  pod.StartAudio(AudioCallback);
  pod.midi.StartReceive();
  for(;;)
  {
    pod.midi.Listen();
    // Handle MIDI Events
    while(pod.midi.HasEvents())
    {
      HandleMidiMessage(pod.midi.PopEvent());
    }
  }
}
