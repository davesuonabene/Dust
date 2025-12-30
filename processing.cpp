#include "processing.h"
#include <string.h> 
#include <math.h>   
#include <stdlib.h> 
#include <cstdio> 

using namespace daisy;
using namespace daisysp;

// --- Menu Pages Definition ---

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

// Advanced Menu
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
    params[PARAM_MAP_AMT]   = 0.0f; // Default map amt
    
    for(int i=0; i<PARAM_COUNT; i++) {
        effective_params[i] = params[i]; 
    }
    
    division_idx = 0; params[PARAM_DIVISION] = (float)division_vals[division_idx];
    
    // Init Page System
    current_page_idx = 0;
    advanced_mode = false;
    SetPage(0);
    
    enc1_holding = false;
    trigger_blink = false;

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
    
    // Reset cursor for new page
    selected_item_idx = 0;
    view_top_item_idx = 0;
    ui_state = STATE_MENU_NAV;
}

void Processing::SetAdvancedMode(bool enabled) {
    advanced_mode = enabled;
    
    // CRITICAL: Reset UI state to ensure we start fresh in the new mode
    ui_state = STATE_MENU_NAV;
    selected_item_idx = 0;
    view_top_item_idx = 0;
    enc1_holding = false; // Reset holding state
    
    if (advanced_mode) {
        current_page_name = "ADVANCED";
        current_menu_items = kItemsAdvanced;
        current_menu_size = sizeof(kItemsAdvanced)/sizeof(MenuItem);
    } else {
        // Return to the last active page
        SetPage(current_page_idx);
    }
}

void Processing::Controls(Hardware &hw)
{
    // --- Encoder 2: Page Scroll (Disabled in Advanced Mode) ---
    if (!advanced_mode && ui_state == STATE_MENU_NAV) {
        int pg_inc = hw.encoder2.Increment();
        if (pg_inc != 0) {
            SetPage(current_page_idx + pg_inc);
        }
    }

    // --- Encoder 1: Interaction (BlackBox Logic) ---
    
    // 1. Hold Detection
    if(hw.encoder1.RisingEdge()) {
        enc1_hold_start = System::GetNow();
        enc1_holding = true;
    }

    // 2. Hold Action Trigger (Time Threshold)
    if(enc1_holding && hw.encoder1.TimeHeldMs() >= kHoldTimeMs) {
        enc1_holding = false; // Consumed
        
        // Enter/Exit Advanced Menu
        SetAdvancedMode(!advanced_mode);
        trigger_blink = true;
    }

    // 3. Release (Click) Logic
    if(hw.encoder1.FallingEdge()) {
        if(enc1_holding) {
            // If we are here, Short Click occurred (Hold didn't timeout)
            enc1_holding = false;
            
            // Toggle Edit / Action
            const MenuItem &item = GetSelectedItem();
            if (ui_state == STATE_MENU_NAV) {
                if (item.type == TYPE_PARAM) {
                    edit_param_target = item.param_id;
                    ui_state = STATE_PARAM_EDIT;
                }
            } else {
                // Exit Edit Mode
                ui_state = STATE_MENU_NAV;
            }
        }
    }

    // 4. Value Change (Turn)
    int inc = hw.encoder1.Increment();
    if (inc != 0) {
        if (ui_state == STATE_MENU_NAV) {
            // Scroll List
            selected_item_idx += inc;
            if (selected_item_idx < 0) selected_item_idx = 0;
            if (selected_item_idx >= current_menu_size) selected_item_idx = current_menu_size - 1;

            // Scroll View Window
            if(selected_item_idx < view_top_item_idx) view_top_item_idx = selected_item_idx;
            if(selected_item_idx >= view_top_item_idx + 4) view_top_item_idx = selected_item_idx - 3;
        } 
        else if (ui_state == STATE_PARAM_EDIT) {
            // Modify Parameter
            float &val = params[edit_param_target];
            float delta = 0.01f;
            if (hw.encoder1.Pressed()) delta = 0.05f;

            // Specific scaling matches BlackBox logic
            switch(edit_param_target) {
                case PARAM_BPM: val = fclamp(val + (float)inc, 20.0f, 300.0f); break;
                case PARAM_DIVISION:
                    division_idx += inc;
                    if(division_idx < 0) division_idx = 0;
                    if(division_idx > 3) division_idx = 3;
                    val = (float)division_vals[division_idx];
                    break;
                case PARAM_PITCH:
                    // Semitone steps approx
                    val += (float)inc * 0.05f; 
                    break;
                case PARAM_GRAIN_SIZE:
                    val = fclamp(val + (float)inc * 0.005f, 0.002f, 0.5f);
                    break;
                case PARAM_GRAINS:
                    val = fclamp(val + (float)inc, 0.5f, 50.0f);
                    break;
                case PARAM_MAP_AMT:
                    val = fclamp(val + (float)inc * delta, 0.0f, 1.0f);
                    break;
                default:
                    val = fclamp(val + (float)inc * delta, 0.0f, 1.0f);
                    break;
            }
            effective_params[edit_param_target] = val;
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