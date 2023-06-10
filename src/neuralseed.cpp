#include "daisy_petal.h"
#include "daisysp.h"
#include "terrarium.h"
#include <RTNeural/RTNeural.h>

// Model Weights (edit this file to add model weights trained with Colab script)
#include "all_model_data_gru9_4count.h"

using namespace daisy;
using namespace daisysp;
using namespace terrarium;  // This is important for mapping the correct controls to the Daisy Seed on Terrarium PCB

// Declare a local daisy_petal for hardware access
DaisyPetal hw;
Parameter Gain, Level, Mix, reverbTime, delayTime_tremFreq, delayFdbk_tremDepth;
bool            bypass;
int             modelInSize;
unsigned int    modelIndex;
bool            pswitches[4];
int             switches[4];
float           nnLevelAdjust;

bool            effects_only_mode;
bool            switch1_hold;

Led led1, led2;

float     mix_amp;
float     mix_effects;

// Looper Parameters
Looper looper;
#define MAX_SIZE (48000 * 60 * 4) // 4 minutes of floats at 48 khz
float DSY_SDRAM_BSS buf[MAX_SIZE];
Oscillator led_osc; // For pulsing the led when recording
float ledBrightness;
Oscillator led_osc2; // For pulsing the led when recording
float ledBrightness2;
int doubleTapCounter;
bool checkDoubleTap;
bool pausePlayback;

// Tremolo
Tremolo tremolo;
int waveform;
float pTremDepth, pTremFreq;  // Previous values, for detecting changes from knob

// Reverb
ReverbSc  verb;
float pReverbTime;          // Previous values, for detecting changes from knob

// Delay
#define MAX_DELAY static_cast<size_t>(48000 * 1.f) // 1 second max delay
DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS delayLine;

struct delay
{
    DelayLine<float, MAX_DELAY> *del;
    float                        currentDelay;
    float                        delayTarget;
    float                        feedback;
    float                        active = false;
    
    float Process(float in)
    {
        //set delay times
        fonepole(currentDelay, delayTarget, .0002f);
        del->SetDelay(currentDelay);

        float read = del->Read();
        if (active) {
            del->Write((feedback * read) + in);
        } else {
            del->Write((feedback * read)); // if not active, don't write any new sound to buffer
        }

        return read;
    }
};

delay delay1;


// These are each of the Neural Model options.
// GRU (Gated Recurrent Unit) networks with input sizes ranging 1 - 4.
// Currently only using snapshot models, they tend to sound better and 
//   we can use input level as gain.

RTNeural::ModelT<float, 1, 1,
    RTNeural::GRULayerT<float, 1, 9>,
    RTNeural::DenseT<float, 9, 1>> model;

// Notes: With default settings, GRU 10 is max size currently able to run on Daisy Seed
//        - Parameterized 1-knob GRU 10 is max, GRU 8 with effects is max
//        - Parameterized 2-knob/3-knob at GRU 8 is max
//        - With multi effect (reverb, etc.) added GRU 9 is recommended to allow for processing space
//        - These models should be trained using 48kHz audio data, since Daisy uses 48kHz by default.
//             Models trained with other samplerates, or running Daisy at a different samplerate will sound off.

// Loads a new model based on the first two switch positions, total of 4 possible combinations
// Models 0 -> 3 
//    (0=off, 1=on) 11=0, 01=1,  10=2,  00=3
void changeModel()
{
    unsigned int modelIndex_temp = 0;

    if (pswitches[0] == true && pswitches[1] == true ) {
        modelIndex_temp = 0;
        nnLevelAdjust = 1.0;
    } else if (pswitches[0] == false && pswitches[1] == true) {
        modelIndex_temp = 1;
        nnLevelAdjust = 1.0;
    } else if (pswitches[0] == true && pswitches[1] == false) {
        modelIndex_temp = 2;
        nnLevelAdjust = 1.0;
    } else if (pswitches[0] == false && pswitches[1] == false) {
        modelIndex_temp = 3;
        nnLevelAdjust = 0.6;
    }
    if ( modelIndex_temp > (model_collection.size() - 1) ) {  // If model is not available, don't change model
        return;
    } else {
        modelIndex = modelIndex_temp;

        auto& gru = (model).template get<0>();
        auto& dense = (model).template get<1>();
        modelInSize = 1;
        gru.setWVals(model_collection[modelIndex].rec_weight_ih_l0);
        gru.setUVals(model_collection[modelIndex].rec_weight_hh_l0);
        gru.setBVals(model_collection[modelIndex].rec_bias);
        dense.setWeights(model_collection[modelIndex].lin_weight);
        dense.setBias(model_collection[modelIndex].lin_bias.data());
        model.reset();
    }
}


void UpdateButtons()
{

    // (De-)Activate bypass and toggle LED when left footswitch is let go, or enable/disable amp if held for greater than 1 second //
    // Can only disable/enable amp when not in bypass mode
    if(hw.switches[Terrarium::FOOTSWITCH_1].FallingEdge())
    {
        if (switch1_hold) {
            switch1_hold = false;
        } else {
            bypass = !bypass;
            led1.Set(bypass ? 0.0f : 1.0f);
        }
    }

    if(hw.switches[Terrarium::FOOTSWITCH_1].TimeHeldMs() >= 1000 && !bypass && !switch1_hold) { 
        effects_only_mode = !effects_only_mode;
        switch1_hold = true;  // This is to make sure effects_only_mode is only toggled once
        if (!effects_only_mode) {
            led1.Set(bypass ? 0.0f : 1.0f);
        }
    }

    //switch2 pressed
    if(hw.switches[Terrarium::FOOTSWITCH_2].RisingEdge())
    {
        looper.TrigRecord();
        led2.Set(looper.Recording() ? 0.0f : 1.0f); // Turn on LED when loop is playing but not recording
        if (!pswitches[3]) {
            looper.SetReverse(true);
        }



        // Start or end double tap timer
        if (checkDoubleTap) {
            // if second press comes before 0.75 seconds, pause playback
            if (doubleTapCounter <= 1125) {
                if (looper.Recording()) {  // Ensure looper is not recording when double tapped (in case it gets double tapped while recording)
                    looper.TrigRecord();
                }
                pausePlayback = !pausePlayback;
                if (pausePlayback) {        // Blink LED if paused, otherwise set to sawtooth wave for pulsing while recording
                    led_osc2.SetWaveform(4); // WAVE_SIN = 0, WAVE_TRI = 1, WAVE_SAW = 2, WAVE_RAMP = 3, WAVE_SQUARE = 4
                } else {
                    led_osc2.SetWaveform(1); 
                }

            }
            //doubleTapCounter = 0;
            //checkDoubleTap = false;  //TODO Figure out what happens if tapped 3+ times in under 0.75 seconds
        } else {
            checkDoubleTap = true;
        }
    }

    if (checkDoubleTap) {
        doubleTapCounter += 1;          // Increment by 1 (48000 * 0.75)/blocksize = 1125   (blocksize is 32)
        if (doubleTapCounter > 1125) {  // If timer goes beyond 0.75 seconds, stop double tap checking
            doubleTapCounter = 0;
            checkDoubleTap = false;
        }
    }

    // If switch2 is held, clear the looper and turn off LED      TODO: Make LED blink twice when cleared
    if(hw.switches[Terrarium::FOOTSWITCH_2].TimeHeldMs() >= 1000)
    {
        looper.Clear();
        led2.Set(looper.Recording() ? 1.0f : 0.0f); 
    }
}

void UpdateSwitches()
{
    // Select appropriate model based on 2 switch positions ////
    bool model_changed = false;
    for(int i=0; i<2; i++)
        if (hw.switches[switches[i]].Pressed() != pswitches[i]) {
            pswitches[i] = hw.switches[switches[i]].Pressed();
            model_changed = true;
        }
    if (model_changed) {    
        changeModel();
    }

    // Select appropriate effects mode for lower 3 knobs based on 3rd switch (either Reverb/Tremolo, or 3BandEQ) ////
    if (hw.switches[switches[2]].Pressed() != pswitches[2]) {
            pswitches[2] = hw.switches[switches[2]].Pressed();
    }

    // Reverse the loop when 4th switch is engaged
    if (hw.switches[switches[3]].Pressed() != pswitches[3]) {
        pswitches[3] = hw.switches[switches[3]].Pressed();
        looper.SetReverse(!pswitches[3]);
    }
}

// This runs at a fixed rate, to prepare audio samples
static void AudioCallback(AudioHandle::InputBuffer  in,
                          AudioHandle::OutputBuffer out,
                          size_t                    size)
{
    //hw.ProcessAllControls();
    hw.ProcessAnalogControls();
    hw.ProcessDigitalControls();

    UpdateButtons();
    UpdateSwitches();

    //float input_arr[2] = { 0.0, 0.0 };
    float input_arr[1] = { 0.0 };    // Neural Net Input
    float sendl, sendr, wetl, wetr;  // Reverb Inputs/Outputs
    float delay_out;

    // Get current knob parameters ////////////////////
    float in_level = Gain.Process();
    float out_level = Level.Process(); 
    float mix_effects = Mix.Process();

    float vReverbTime = reverbTime.Process();
    float vdelayTime_tremFreq = delayTime_tremFreq.Process();
    float vdelayFdbk_tremDepth = delayFdbk_tremDepth.Process();

    // Check for changes in effects knobs /////////

    // REVERB //
    if ((pReverbTime != vReverbTime))
    {
        if (vReverbTime < 0.02) { // if knob < 2%, set reverb to 0 // TODO if mix is full wet or close, audible jump from reverb off to 0.5, maybe ramp it?
            verb.SetFeedback(0.0);
        } else {
            verb.SetFeedback(vReverbTime * 0.5 + 0.5); // Reverb time range 0.5 to 1.0
        }
        pReverbTime = vReverbTime;
    }

    // DELAY //
    if (pswitches[2] == true) {

        if (vdelayTime_tremFreq < 0.02) {   // if knob < 2%, set delay to inactive
            delay1.active = false;
        } else {
            delay1.active = true;
        }
        delay1.delayTarget = 2400 + vdelayTime_tremFreq * 45600 ; // in samples 50ms to 1 second  // Note: changing delay time with heavy reverb creates a cool modulation effect
        delay1.feedback = vdelayFdbk_tremDepth;
    
    // TREMOLO //
    } else {

        if (pTremFreq != vdelayTime_tremFreq)
        {
            tremolo.SetFreq(vdelayTime_tremFreq * 12.0 + 0.5); // range from 0.5 - 12.5 Hz
            pTremFreq = vdelayTime_tremFreq;
        }

        if (pTremDepth != vdelayFdbk_tremDepth)
        {
            tremolo.SetDepth(vdelayFdbk_tremDepth);
            pTremDepth = vdelayFdbk_tremDepth;
        }
    }

    // Loop through the current audio block and process all effects ////////
    // Order of effects is:
    //           Gain -> Neural Model -> Delay ->
    //           Reverb -> Effects Mixer -> Tremolo -> Looper 
    //

    for(size_t i = 0; i < size; i++)
    {

        ledBrightness = led_osc.Process();
        ledBrightness2 = led_osc2.Process();

        // Process your signal here
        if(bypass)
        {
            /// Process Looper // Able to use looper when in bypass mode // 
            float loop_out = 0.0;
            if (!pausePlayback) {
                loop_out = looper.Process(in[0][i]);
            }
            out[0][i] = in[0][i] + loop_out;
        }
        else
        {   
            float ampOut;
            input_arr[0] = in[0][i] * in_level;

            // Process Neural Net Model //
            if (effects_only_mode) {
                ampOut = input_arr[0] * 1.5; // apply gain level to dry signal and slight boost to match NN model levels
            } else {
               
                ampOut = model.forward (input_arr) + input_arr[0];   // Run Model and add Skip Connection; CHANGE FROM v0.1, was calculating skip wrong, should have also added input gain, fixed here
                ampOut *= nnLevelAdjust; // Manually adjust model volume for quiet or loud models TODO: move to Model parameter
            }
            
            // Process Delay
            delay_out = delay1.Process(ampOut);  

            // Process Reverb //
            float final_effects = 0.0;
            sendl = (ampOut + delay_out) * 0.5;  // adding in pre-effects signal to go to reverb, volume reduction on effects input for better balance
            sendr = sendl;
            verb.Process(sendl, sendr, &wetl, &wetr);

            final_effects = wetl + delay_out;

            // Process Wet/Dry Effects Mix //
            float final_effects_mix;
            final_effects_mix = ampOut * (1.0- mix_effects) + final_effects * mix_effects; // Mix amp out with delay/reverb

            final_effects_mix = tremolo.Process(final_effects_mix);   // Process Tremolo on total output (not controlled by effects mix, depth control acts as effect level)
            final_effects_mix *= out_level;                           // Set output level

            /// Process Looper //
            float loop_out = 0.0;
            if (!pausePlayback) {
                loop_out = looper.Process(final_effects_mix);
            } 

            out[0][i] = loop_out + final_effects_mix;

        }
    }

    // Handle Pulsing LEDs
    if (looper.Recording()) {
        led2.Set(ledBrightness2*0.5 + 0.5);       // Pulse the LED when recording
    } 

    if (pausePlayback) {
        led2.Set(ledBrightness2*2.0);       // Blink the LED when paused
    }
   
    if (effects_only_mode && !bypass) {
        led1.Set(ledBrightness*0.5 + 0.5);  // Pulse the 1st LED when in effects only mode and not bypassed
        //led1.Set(1.0 - led_osc.Process() * 0.5 + 0.5);
    }

    led1.Update();
    led2.Update();
}


int main(void)
{
    float samplerate;

    hw.Init();
    samplerate = hw.AudioSampleRate();

    tremolo.Init(samplerate);
    verb.Init(samplerate);

    setupWeights();
    hw.SetAudioBlockSize(32);  // 32 was about the lowest I could go (24 too low) for NN processing to keep up

    looper.Init(buf, MAX_SIZE);
    looper.SetMode(Looper::Mode::NORMAL);
    led_osc.Init(samplerate);
    led_osc.SetFreq(1.0);
    led_osc.SetWaveform(1); // WAVE_SIN = 0, WAVE_TRI = 1, WAVE_SAW = 2, WAVE_RAMP = 3, WAVE_SQUARE = 4
    ledBrightness = 0.0;

    led_osc2.Init(samplerate);
    led_osc2.SetFreq(1.5);
    led_osc2.SetWaveform(1); // WAVE_SIN = 0, WAVE_TRI = 1, WAVE_SAW = 2, WAVE_RAMP = 3, WAVE_SQUARE = 4
    ledBrightness2 = 0.0;

    pausePlayback = false;
    doubleTapCounter = 0;
    checkDoubleTap = false;

    switch1_hold = false;

    pswitches[0]= true;
    pswitches[1]= true;
    pswitches[2]= true;
    pswitches[3]= true;

    switches[0]= Terrarium::SWITCH_1;
    switches[1]= Terrarium::SWITCH_2;
    switches[2]= Terrarium::SWITCH_3;
    switches[3]= Terrarium::SWITCH_4;

    Gain.Init(hw.knob[Terrarium::KNOB_1], 0.1f, 2.5f, Parameter::LINEAR);
    Mix.Init(hw.knob[Terrarium::KNOB_2], 0.0f, 1.0f, Parameter::LINEAR);
    Level.Init(hw.knob[Terrarium::KNOB_3], 0.0f, 1.0f, Parameter::LINEAR); 
    reverbTime.Init(hw.knob[Terrarium::KNOB_4], 0.0f, 1.0f, Parameter::LINEAR);
    delayTime_tremFreq.Init(hw.knob[Terrarium::KNOB_5], 0.0f, 1.0f, Parameter::LINEAR);
    delayFdbk_tremDepth.Init(hw.knob[Terrarium::KNOB_6], 0.0f, 1.0f, Parameter::LINEAR); 

    // Initialize the correct model
    modelIndex = 0;
    changeModel();
    nnLevelAdjust = 1.0;

    // Initialize & set params for mixers 
    mix_effects = 0.5;
    effects_only_mode = false;

    tremolo.SetFreq(3.0);
    tremolo.SetDepth(0.0); 
    tremolo.SetWaveform(0);   // WAVE_SIN = 0, WAVE_TRI = 1, WAVE_SAW = 2, WAVE_RAMP = 3, WAVE_SQUARE = 4

    verb.SetFeedback(0.0);
    verb.SetLpFreq(9000.0);  // TODO Experiment with freq value, what sounds best?

    delayLine.Init();
    delay1.del = &delayLine;
    delay1.delayTarget = 2400; // in samples
    delay1.feedback = 0.0;
    delay1.active = false;     // Default to no delay

    // Init the LEDs and set activate bypass
    led1.Init(hw.seed.GetPin(Terrarium::LED_1),false);
    led1.Update();
    bypass = true;

    led2.Init(hw.seed.GetPin(Terrarium::LED_2),false);
    led2.Update();

    hw.StartAdc();
    hw.StartAudio(AudioCallback);
    while(1)
    {
        // Do Stuff Infinitely Here
        System::Delay(10);
    }
}