#include "daisy_pod.h"
#include "daisysp.h"
#include <cstdio>
#include <string.h>
#include <unordered_map>
#include <cstdarg>

static daisy::DaisyPod pod;
static daisysp::Oscillator osc, lfo;
static bool gate = false;
static daisysp::MoogLadder flt;
static daisysp::Adsr adsr_vca, adsr_vcf;
static daisy::Parameter  pitchParam, cutoffParam, lfoParam;
static float vcf_env_depth = 0.;
static float vca_env_depth = 0.;
static float vca_bias = 0.;
static float vcf_freq = 440;
static float vcf_res = 0;

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


void AudioCallback(daisy::AudioHandle::InterleavingInputBuffer  in,
    daisy::AudioHandle::InterleavingOutputBuffer out,
    size_t                                size)
{
  float sig;
  for(size_t i = 0; i < size; i += 2)
  {
    // Set/get values for sig processing
    float vcf_env = adsr_vcf.Process(gate);
    flt.SetFreq(vcf_freq + vcf_env * vcf_env_depth);
    flt.SetRes(vcf_res);
    float vca_env = adsr_vca.Process(gate);

    // Apply the processing values
    sig = osc.Process();
    sig = flt.Process(sig);
    sig *= vca_env;
    out[i] = out[i + 1] = sig;
  }
}


// Typical Switch case for Message Type.
void HandleMidiMessage(daisy::MidiEvent m)
{
  switch(m.type) {
    case daisy::NoteOn:
      {
        daisy::NoteOnEvent p = m.AsNoteOn();
        logger.Print("Note Received:\t%d\t%d\t%d\r\n", m.channel, m.data[0], m.data[1]);
        // This is to avoid Max/MSP Note outs for now..
        if(m.data[1] != 0)
        {
          p = m.AsNoteOn();
          osc.SetFreq(daisysp::mtof(p.note));
          osc.SetAmp((p.velocity / 127.0f));
        }
        gate = true;
      }
      break;
    case daisy::NoteOff: 
      {
        //daisy::NoteOnEvent p = m.AsNoteOn();
        gate = false;
      }
      break;
    case daisy::ControlChange: 
      {
        daisy::ControlChangeEvent p = m.AsControlChange();
        logger.Print("Control Received:\t%d\t%d -> %i / %i\n", p.control_number, p.value, midi_map[p.control_number], SynthControl::wave_shape);
        switch(midi_map[p.control_number]) {
          case SynthControl::wave_shape: 
            {
              logger.Print("Control Received: Waveform -> %d\r\n",static_cast<uint8_t>(p.value / 9));
              osc.SetWaveform(static_cast<uint8_t>(p.value / 9));
            }
            break;
          case SynthControl::vcf_cutoff:
            {
              vcf_freq = 20 + 20'000 * (p.value / 128.0);
              logger.Print("Control Received: vcf_freq -> %f\r\n", vcf_freq);
            }
            break;
          case SynthControl::vcf_resonance:
            {
              vcf_res = 20 + 20'000 * (p.value / 128.0);
              logger.Print("Control Received: vcf_res -> %f\r\n", vcf_res);
            }
            break;
          case SynthControl::vcf_envelope_depth:
            {
              vcf_env_depth = p.value / 128.0;
              logger.Print("Control Received: vcf_env_depth -> %f\r\n", vcf_env_depth);
            }
            break;
          case SynthControl::vca_bias:
            {
              vca_bias = p.value / 128.0;
              logger.Print("Control Received: vca_bias -> %f\r\n", vca_bias);
            }
            break;
          case SynthControl::envelope_a_vca:
            {
              logger.Print("Control Received: VCA Attack -> %f\r\n", p.value / 128.0);
              adsr_vca.SetAttackTime(p.value / 128.0); // secs
            }
            break;
          case SynthControl::envelope_d_vca:
            {
              logger.Print("Control Received: VCA Decay -> %f\r\n", p.value / 128.0);
              adsr_vca.SetDecayTime(p.value / 128.0); // secs
            }
            break;
          case SynthControl::envelope_s_vca:
            {
              logger.Print("Control Received: VCA Sustain -> %f\r\n", p.value / 128.0);
              adsr_vca.SetSustainLevel(p.value / 128.0);
            }
            break;
          case SynthControl::envelope_r_vca:
            {
              logger.Print("Control Received: VCA Release -> %f\r\n", p.value / 128.0);
              adsr_vca.SetReleaseTime(p.value / 128.0); // secs
            }
            break;
          case SynthControl::envelope_a_vcf:
            {
              logger.Print("Control Received: VCF Attack -> %f\r\n", p.value / 128.0);
              adsr_vcf.SetAttackTime(p.value / 128.0); // secs
            }
            break;
          case SynthControl::envelope_d_vcf:
            {
              logger.Print("Control Received: VCF Decay -> %f\r\n", p.value / 128.0);
              adsr_vcf.SetDecayTime(p.value / 128.0); // secs
            }
            break;
          case SynthControl::envelope_s_vcf:
            {
              logger.Print("Control Received: VCF Sustain -> %f\r\n", p.value / 128.0);
              adsr_vcf.SetSustainLevel(p.value / 128.0);
            }
            break;
          case SynthControl::envelope_r_vcf:
            {
              logger.Print("Control Received: VCF Release -> %f\r\n", p.value / 128.0);
              adsr_vcf.SetReleaseTime(p.value / 128.0); // secs
            }
            break;
          default: 
            {
              logger.Print("Control Received: Not Mapped -> %f\r\n", p.control_number);
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
  osc.Init(samplerate);
  osc.SetWaveform(daisysp::Oscillator::WAVE_POLYBLEP_SAW);
  flt.Init(samplerate);

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
