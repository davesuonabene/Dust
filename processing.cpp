#include "processing.h"
#include <string.h> 
#include <math.h>   
#include <stdlib.h> 
#include <cstdio> 

using namespace daisy;
using namespace daisysp;

// --- Menu Definitions ---

// Page 1: Mix
const MenuItem kItemsMix[] = {
    {"Pre Gain", TYPE_PARAM, PARAM_PRE_GAIN},
    {"Post Gain",TYPE_PARAM, PARAM_POST_GAIN},
    {"Mix",      TYPE_PARAM, PARAM_MIX},
    {"Fbk",      TYPE_PARAM, PARAM_FEEDBACK}
};

// Page 2: Granular
const MenuItem kItemsGrain[] = {
    {"Pitch",    TYPE_PARAM, PARAM_PITCH},
    {"Size",     TYPE_PARAM, PARAM_GRAIN_SIZE},
    {"Density",  TYPE_PARAM, PARAM_GRAINS},
    {"Spray",    TYPE_PARAM, PARAM_SPRAY},
    {"Stereo",   TYPE_PARAM, PARAM_STEREO}
};

// Page 3: Time
const MenuItem kItemsTime[] = {
    {"BPM",      TYPE_PARAM, PARAM_BPM},
    {"Div",      TYPE_PARAM, PARAM_DIVISION}
};

// Advanced Menu (Secondary Page)
const MenuItem kItemsAdvanced[] = {
    {"Map Amt",  TYPE_PARAM, PARAM_MAP_AMT}
};

// Page Registry
const MenuPage kPages[] = {
    {"MIX",   kItemsMix,   sizeof(kItemsMix)/sizeof(MenuItem)},
    {"GRAIN", kItemsGrain, sizeof(kItemsGrain)/sizeof(MenuItem)},
    {"TIME",  kItemsTime,  sizeof(kItemsTime)/sizeof(MenuItem)}
};
const int kNumPages = sizeof(kPages) / sizeof(MenuPage);

float DSY_SDRAM_BSS Processing::buffer[MAX_BUFFER_SAMPLES];
Processing::Grain Processing::grains_l[MAX_GRAINS];
Processing::Grain Processing::grains_r[MAX_GRAINS];

void Processing::Init(Hardware &hw)
{
    memset(buffer, 0, MAX_BUFFER_SAMPLES * sizeof(float));
    sample_rate_ = hw.sample_rate;
    
    // Defaults
    params[PARAM_PRE_GAIN] = 0.5f; params[PARAM_FEEDBACK] = 0.5f; params[PARAM_MIX] = 0.5f;
    params[PARAM_POST_GAIN] = 0.5f; params[PARAM_BPM] = 120.0f; params[PARAM_DIVISION] = 1.0f; 
    params[PARAM_PITCH] = 1.0f; params[PARAM_GRAIN_SIZE] = 0.1f; params[PARAM_GRAINS] = 10.0f; 
    params[PARAM_SPRAY] = 0.0f; params[PARAM_STEREO] = 0.0f;
    
    for(int i=0; i<PARAM_COUNT; i++) {
        knob_map_amounts[i] = 0.0f; 
        effective_params[i] = params[i]; 
    }
    
    division_idx = 0; params[PARAM_DIVISION] = (float)division_vals[division_idx];
    
    // UI Init
    current_page_idx = 0;
    advanced_mode = false;
    SetPage(0);
    
    // Control Init
    enc1_holding = false;
    enc1_hold_time = 0.0f;

    UpdateBufferLen();
    UpdateGrainParams();
}

void Processing::SetPage(int page_idx) {
    if (page_idx < 0) page_idx = kNumPages - 1;
    if (page_idx >= kNumPages) page_idx = 0;
    
    current_page_idx = page_idx;
    current_page_name = kPages[current_page_idx].name;
    current_menu_items = kPages[current_page_idx].items;
    current_menu_size = kPages[current_page_idx].num_items;
    selected_item_idx = 0;
    ui_state = STATE_MENU_NAV;
}

void Processing::SetAdvancedMode(bool enabled) {
    advanced_mode = enabled;
    ui_state = STATE_MENU_NAV;
    selected_item_idx = 0;
    
    if (advanced_mode) {
        current_page_name = "ADVANCED";
        current_menu_items = kItemsAdvanced;
        current_menu_size = sizeof(kItemsAdvanced)/sizeof(MenuItem);
    } else {
        // Return to last active page
        SetPage(current_page_idx);
    }
}

void Processing::Controls(Hardware &hw)
{
    // --- Encoder 1: Interaction Logic ---

    // 1. Hold Time Accumulation
    bool enc1_pressed = hw.encoder1.Pressed();
    if (enc1_pressed) {
        // Increment based on actual Callback Rate (approx 12kHz for block size 4)
        enc1_hold_time += 1.0f / hw.seed.AudioCallbackRate();

        // Trigger HOLD ACTION immediately when threshold is crossed
        if (enc1_hold_time > 0.6f && !enc1_holding) {
            SetAdvancedMode(!advanced_mode);
            enc1_holding = true; // Mark as handled so we don't trigger again or trigger click on release
        }
    }

    // 2. Release Logic (Click)
    if (hw.encoder1.FallingEdge()) {
        // Only trigger click if the hold action was NOT triggered
        if (!enc1_holding) {
            // --- SHORT CLICK: Toggle Edit Mode ---
            if (ui_state == STATE_MENU_NAV) {
                 const MenuItem &item = current_menu_items[selected_item_idx];
                 if (item.type == TYPE_PARAM) {
                     edit_param_target = item.param_id;
                     ui_state = STATE_PARAM_EDIT;
                 }
            } else if (ui_state == STATE_PARAM_EDIT) {
                ui_state = STATE_MENU_NAV;
            }
        }
    }

    // 3. Reset Hold State on Release
    if (!enc1_pressed) {
        enc1_hold_time = 0.0f;
        enc1_holding = false;
    }


    // --- Encoder 2: Page Navigation ---
    // Only works if NOT in Advanced Mode and NOT editing a parameter
    if (!advanced_mode && ui_state == STATE_MENU_NAV) {
        int page_inc = hw.encoder2.Increment();
        if (page_inc != 0) {
            SetPage(current_page_idx + page_inc);
        }
    }


    // --- Encoder 1: Value / Selection ---
    int enc1_inc = hw.encoder1.Increment();
    
    if (ui_state == STATE_MENU_NAV)
    {
        // Navigate List
        if (enc1_inc != 0) {
            selected_item_idx += enc1_inc;
            if (selected_item_idx < 0) selected_item_idx = 0;
            if (selected_item_idx >= current_menu_size) selected_item_idx = current_menu_size - 1;
        }
    }
    else if (ui_state == STATE_PARAM_EDIT)
    {
        // Change Value
        if (enc1_inc != 0) {
            float scale = 0.01f;
            if (hw.encoder1.Pressed()) scale = 0.05f; // Faster if pressed while turning
            
            params[edit_param_target] += (float)enc1_inc * scale;
            params[edit_param_target] = fclamp(params[edit_param_target], 0.0f, 1.0f);
            effective_params[edit_param_target] = params[edit_param_target];
        }
    }

    UpdateBufferLen();
    UpdateGrainParams();
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