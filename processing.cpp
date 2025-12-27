#include "processing.h"
#include <string.h> 
#include <math.h>   
#include <stdlib.h> 
#include <cstdio> 

using namespace daisy;
using namespace daisysp;

// --- Menu Tree Definition ---
const MenuItem kMenuGenericEdit[] = {
    {"BACK",    TYPE_BACK,  0,             kMenuMain, 0},
    {"Map Amt", TYPE_PARAM, PARAM_MAP_AMT, nullptr, 0}
};
const int kMenuGenericEditSize = sizeof(kMenuGenericEdit) / sizeof(kMenuGenericEdit[0]);

const MenuItem kMenuPostEdit[] = {
    {"BACK",    TYPE_BACK,  0,              kMenuMain, 0},
    {"Map Amt", TYPE_PARAM, PARAM_MAP_AMT,  nullptr, 0}, 
    {"Pre",     TYPE_PARAM, PARAM_PRE_GAIN, nullptr, 0}
};
const int kMenuPostEditSize = sizeof(kMenuPostEdit) / sizeof(kMenuPostEdit[0]);

const MenuItem kMenuBpmEdit[] = {
    {"BACK",    TYPE_BACK,  0,              kMenuMain, 0},
    {"Map Amt", TYPE_PARAM, PARAM_MAP_AMT,  nullptr, 0},
    {"Div",     TYPE_PARAM, PARAM_DIVISION, nullptr, 0}
};
const int kMenuBpmEditSize = sizeof(kMenuBpmEdit) / sizeof(kMenuBpmEdit[0]);

const MenuItem kMenuGrainsEdit[] = {
    {"BACK",    TYPE_BACK,  0,              kMenuMain, 0},
    {"Map Amt", TYPE_PARAM, PARAM_MAP_AMT,  nullptr, 0},
    {"Spray",   TYPE_PARAM, PARAM_SPRAY,    nullptr, 0},
    {"Stereo",  TYPE_PARAM, PARAM_STEREO,   nullptr, 0}
};
const int kMenuGrainsEditSize = sizeof(kMenuGrainsEdit) / sizeof(kMenuGrainsEdit[0]);

const MenuItem kMenuMain[] = {
    {"Post",    TYPE_PARAM_SUBMENU, PARAM_POST_GAIN,    kMenuPostEdit,    kMenuPostEditSize},
    {"Fbk",     TYPE_PARAM,         PARAM_FEEDBACK,     nullptr,          0},
    {"Mix",     TYPE_PARAM,         PARAM_MIX,          nullptr,          0},
    {"BPM",     TYPE_PARAM_SUBMENU, PARAM_BPM,          kMenuBpmEdit,     kMenuBpmEditSize},
    {"Pitch",   TYPE_PARAM,         PARAM_PITCH,        nullptr,          0},
    {"Size",    TYPE_PARAM,         PARAM_GRAIN_SIZE,   nullptr,          0},
    {"Grains",  TYPE_PARAM_SUBMENU, PARAM_GRAINS,       kMenuGrainsEdit,  kMenuGrainsEditSize}
};
const int kMenuMainSize = sizeof(kMenuMain) / sizeof(kMenuMain[0]);

float DSY_SDRAM_BSS Processing::buffer[MAX_BUFFER_SAMPLES];
Processing::Grain Processing::grains_l[MAX_GRAINS];
Processing::Grain Processing::grains_r[MAX_GRAINS];

void Processing::Init(Hardware &hw)
{
    memset(buffer, 0, MAX_BUFFER_SAMPLES * sizeof(float));
    sample_rate_ = hw.sample_rate;
    params[PARAM_PRE_GAIN] = 0.5f; params[PARAM_FEEDBACK] = 0.5f; params[PARAM_MIX] = 0.5f;
    params[PARAM_POST_GAIN] = 0.5f; params[PARAM_BPM] = 120.0f; params[PARAM_DIVISION] = 1.0f; 
    params[PARAM_PITCH] = 1.0f; params[PARAM_GRAIN_SIZE] = 0.1f; params[PARAM_GRAINS] = 10.0f; 
    params[PARAM_SPRAY] = 0.0f; params[PARAM_STEREO] = 0.0f;
    for(int i=0; i<PARAM_COUNT; i++) {
        knob_map_amounts[i] = 0.0f; 
        effective_params[i] = params[i]; 
    }
    snprintf(parent_menu_name, sizeof(parent_menu_name), " ");
    division_idx = 0; params[PARAM_DIVISION] = (float)division_vals[division_idx];
    last_looper_toggle = 0;
    long_press_active = false;
    UpdateBufferLen();
    UpdateGrainParams();
}

void Processing::Controls(Hardware &hw)
{
    // Functionality stubbed out for hardware test
}

void Processing::UpdateBufferLen() {
    float bpm = effective_params[PARAM_BPM]; float division = params[PARAM_DIVISION]; 
    float loop_len_sec = (1.0f / (bpm / 60.0f)) * (4.0f / division);
    buffer_len_samples = (uint32_t)(loop_len_sec * sample_rate_);
    if(buffer_len_samples > MAX_BUFFER_SAMPLES) { buffer_len_samples = MAX_BUFFER_SAMPLES; }
    if(buffer_len_samples < 4) { buffer_len_samples = 4; }
    if(write_pos >= buffer_len_samples) { write_pos = 0; }
}

void Processing::UpdateGrainParams() {
    float density_hz = effective_params[PARAM_GRAINS]; float stereo_amt = effective_params[PARAM_STEREO];
    if(density_hz < 0.1f) { density_hz = 0.1f; } float base_int = sample_rate_ / density_hz;
    float l_rand = (1.0f - stereo_amt) + (rand_.Process() * stereo_amt); 
    float r_rand = (1.0f - stereo_amt) + (rand_.Process() * stereo_amt);
    grain_trig_interval_l = (uint32_t)(base_int * l_rand); 
    grain_trig_interval_r = (uint32_t)(base_int * r_rand);
    if(grain_trig_interval_l == 0) { grain_trig_interval_l = 1; }
    if(grain_trig_interval_r == 0) { grain_trig_interval_r = 1; }
}

void Processing::GetSample(float &outl, float &outr, float inl, float inr) {
    float pre_gain = effective_params[PARAM_PRE_GAIN] * 2.0f; 
    float fbk = effective_params[PARAM_FEEDBACK];
    float mix = effective_params[PARAM_MIX]; 
    float post_gain = effective_params[PARAM_POST_GAIN] * 2.0f;
    float stereo = effective_params[PARAM_STEREO]; float spray = effective_params[PARAM_SPRAY];
    float inl_g = inl * pre_gain; float inr_g = inr * pre_gain;
    float wet_in = (inl_g + inr_g) * 0.5f; 
    
    float old_samp = buffer[write_pos];
    buffer[write_pos] = fclamp(wet_in + (old_samp * fbk), -1.0f, 1.0f);
    
    if(grain_trig_counter_l == 0) {
        float sz_mod = (1.0f - stereo) + (rand_.Process() * stereo);
        uint32_t sz = (uint32_t)(effective_params[PARAM_GRAIN_SIZE] * sample_rate_ * sz_mod);
        float start = (float)write_pos - (rand_.Process() * spray * 0.5f * sample_rate_);
        for(int i = 0; i < MAX_GRAINS; i++) { 
            if(!grains_l[i].active) { grains_l[i].Start(start, effective_params[PARAM_PITCH], sz, sample_rate_, buffer_len_samples); break; }
        }
        UpdateGrainParams(); grain_trig_counter_l = grain_trig_interval_l;
    } grain_trig_counter_l--;

    if(grain_trig_counter_r == 0) {
        float sz_mod = (1.0f - stereo) + (rand_.Process() * stereo);
        uint32_t sz = (uint32_t)(effective_params[PARAM_GRAIN_SIZE] * sample_rate_ * sz_mod);
        float start = (float)write_pos - (rand_.Process() * spray * 0.5f * sample_rate_);
        for(int i = 0; i < MAX_GRAINS; i++) { 
            if(!grains_r[i].active) { grains_r[i].Start(start, effective_params[PARAM_PITCH], sz, sample_rate_, buffer_len_samples); break; }
        }
        grain_trig_counter_r = grain_trig_interval_r;
    } grain_trig_counter_r--;

    float wet_l = 0.0f; float wet_r = 0.0f;
    for(int i = 0; i < MAX_GRAINS; i++) { 
        wet_l += grains_l[i].Process(buffer, buffer_len_samples); 
        wet_r += grains_r[i].Process(buffer, buffer_len_samples); 
    }
    wet_l *= 0.5f; wet_r *= 0.5f;
    write_pos++; if(write_pos >= buffer_len_samples) { write_pos = 0; }
    
    outl = (inl_g * (1.0f - mix) + wet_l * mix) * post_gain; 
    outr = (inr_g * (1.0f - mix) + wet_r * mix) * post_gain;
}