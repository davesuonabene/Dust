#include "processing.h"
#include <string.h> 
#include <math.h>   
#include <stdlib.h> 
#include <cstdio> 

using namespace daisy;
using namespace daisysp;

// --- Menu Pages Definition ---
const MenuItem kItemsMix[] = {
    {"Pre Gain", TYPE_PARAM, PARAM_PRE_GAIN},
    {"Post Gain",TYPE_PARAM, PARAM_POST_GAIN},
    {"Mix",      TYPE_PARAM, PARAM_MIX},
    {"Fbk",      TYPE_PARAM, PARAM_FEEDBACK}
};

const MenuItem kItemsGrain[] = {
    {"Pitch",    TYPE_PARAM, PARAM_PITCH},
    {"Size",     TYPE_PARAM, PARAM_GRAIN_SIZE},
    {"Density",  TYPE_PARAM, PARAM_GRAINS},
    {"Spray",    TYPE_PARAM, PARAM_SPRAY},
    {"Stereo",   TYPE_PARAM, PARAM_STEREO}
};

const MenuItem kItemsTime[] = {
    {"BPM",      TYPE_PARAM, PARAM_BPM},
    {"Div",      TYPE_PARAM, PARAM_DIVISION}
};

const MenuItem kItemsLooper[] = {
    {"(Visual Only)", TYPE_PARAM, -1}
};

const MenuItem kItemsAdvanced[] = {
    {"Map Amt",  TYPE_PARAM, PARAM_MAP_AMT}
};

const MenuPage kPages[] = {
    {"MIX",    kItemsMix,    sizeof(kItemsMix)/sizeof(MenuItem)},
    {"GRAIN",  kItemsGrain,  sizeof(kItemsGrain)/sizeof(MenuItem)},
    {"TIME",   kItemsTime,   sizeof(kItemsTime)/sizeof(MenuItem)},
    {"LOOPER", kItemsLooper, 0}
};
const int kNumPages = sizeof(kPages) / sizeof(MenuPage);

Processing::Grain Processing::grains_l[MAX_GRAINS];
Processing::Grain Processing::grains_r[MAX_GRAINS];

void Processing::Init(Hardware &hw)
{
    sample_rate_ = hw.sample_rate;
    active_buffer = hw.buffer_a;
    rec_buffer    = hw.buffer_b;
    ResetLooper(hw);

    params[PARAM_PRE_GAIN] = 0.5f; params[PARAM_FEEDBACK] = 0.5f; params[PARAM_MIX] = 0.5f;
    params[PARAM_POST_GAIN] = 0.5f; params[PARAM_BPM] = 120.0f; params[PARAM_DIVISION] = 1.0f; 
    params[PARAM_PITCH] = 1.0f; params[PARAM_GRAIN_SIZE] = 0.1f; params[PARAM_GRAINS] = 10.0f; 
    params[PARAM_SPRAY] = 0.0f; params[PARAM_STEREO] = 0.0f;
    params[PARAM_MAP_AMT] = 0.0f; 
    
    for(int i=0; i<PARAM_COUNT; i++) effective_params[i] = params[i];
    
    division_idx = 0; params[PARAM_DIVISION] = (float)division_vals[division_idx];
    
    current_page_idx = 0;
    advanced_mode = false;
    SetPage(0);
    
    enc1_holding = false;
    trigger_blink = false;

    UpdateBufferLen();
    UpdateGrainParams();
}

void Processing::ResetLooper(Hardware &hw) {
    looper_state = LP_EMPTY;
    rec_pos = 0;
    play_pos = 0;
    loop_len = 0;
    active_buffer = hw.buffer_a;
    rec_buffer    = hw.buffer_b;
    write_pos = 0;
    memset(active_buffer, 0, LOOPER_MAX_SAMPLES * sizeof(float));
    
    // Reset flags
    btn1_held_event = false;
}

void Processing::SetPage(int page_idx) {
    if (page_idx < 0) page_idx = kNumPages - 1;
    if (page_idx >= kNumPages) page_idx = 0;
    
    current_page_idx = page_idx;
    current_page_name = kPages[current_page_idx].name;
    current_menu_items = kPages[current_page_idx].items;
    current_menu_size = kPages[current_page_idx].num_items;
    
    selected_item_idx = 0;
    view_top_item_idx = 0;
    ui_state = STATE_MENU_NAV;
}

void Processing::SetAdvancedMode(bool enabled) {
    advanced_mode = enabled;
    ui_state = STATE_MENU_NAV;
    selected_item_idx = 0;
    view_top_item_idx = 0;
    enc1_holding = false; 
    
    if (advanced_mode) {
        current_page_name = "ADVANCED";
        current_menu_items = kItemsAdvanced;
        current_menu_size = sizeof(kItemsAdvanced)/sizeof(MenuItem);
    } else {
        SetPage(current_page_idx);
    }
}

void Processing::Controls(Hardware &hw)
{
    // --- Encoder 2: Page Scroll ---
    if (!advanced_mode && ui_state == STATE_MENU_NAV) {
        int pg_inc = hw.encoder2.Increment();
        if (pg_inc != 0) SetPage(current_page_idx + pg_inc);
    }

    // --- Button 1: Looper Control ---
    if (hw.button1.TimeHeldMs() > 1000 && looper_state != LP_EMPTY) {
        // HOLD ACTION: Clear
        // Only trigger once per hold
        if (!btn1_held_event) {
            ResetLooper(hw);
            trigger_blink = true;
            btn1_held_event = true; // Mark as held to ignore release
        }
    } 
    else if (hw.button1.FallingEdge()) {
        // RELEASE ACTION (Click)
        
        // If we processed a Hold event, ignore this release (prevents bugged rec)
        if (btn1_held_event) {
            btn1_held_event = false; 
        } 
        else if (hw.button1.TimeHeldMs() < 500) {
            // Valid Click
            uint32_t now = System::GetNow();
            if (now - btn1_release_time < 300) {
                // DOUBLE CLICK: Stop
                if (looper_state == LP_PLAY || looper_state == LP_REC) {
                    looper_state = LP_STOP;
                }
                btn1_handled = true; 
            } else {
                // SINGLE CLICK
                if (looper_state == LP_EMPTY) {
                    // Start Recording
                    looper_state = LP_REC;
                    rec_pos = 0;
                    memset(rec_buffer, 0, LOOPER_MAX_SAMPLES * sizeof(float));
                } 
                else if (looper_state == LP_REC) {
                    // Finish Rec -> Play
                    looper_state = LP_PLAY;
                    loop_len = rec_pos;
                    if (loop_len < 4800) loop_len = 4800; 
                    
                    // Swap Buffers
                    float* temp = active_buffer;
                    active_buffer = rec_buffer;
                    rec_buffer = temp;
                    
                    play_pos = 0;
                } 
                else if (looper_state == LP_PLAY) {
                    // Play -> Rec (Resampling / Overdub)
                    looper_state = LP_REC;
                    rec_pos = 0;
                    memset(rec_buffer, 0, LOOPER_MAX_SAMPLES * sizeof(float));
                }
                else if (looper_state == LP_STOP) {
                    looper_state = LP_PLAY;
                    play_pos = 0;
                }
                btn1_release_time = now;
                btn1_handled = false;
            }
        }
    }

    // --- Encoder 1: Interaction ---
    if(hw.encoder1.RisingEdge()) {
        enc1_hold_start = System::GetNow();
        enc1_holding = true;
    }
    if(enc1_holding && hw.encoder1.TimeHeldMs() >= kHoldTimeMs) {
        enc1_holding = false; 
        SetAdvancedMode(!advanced_mode);
        trigger_blink = true;
    }
    if(hw.encoder1.FallingEdge()) {
        if(enc1_holding) {
            enc1_holding = false;
            const MenuItem &item = GetSelectedItem();
            if (ui_state == STATE_MENU_NAV && item.param_id >= 0) {
                 edit_param_target = item.param_id;
                 ui_state = STATE_PARAM_EDIT;
            } else {
                ui_state = STATE_MENU_NAV;
            }
        }
    }

    int inc = hw.encoder1.Increment();
    if (inc != 0) {
        if (ui_state == STATE_MENU_NAV) {
            selected_item_idx += inc;
            if (selected_item_idx < 0) selected_item_idx = 0;
            if (selected_item_idx >= current_menu_size) selected_item_idx = current_menu_size - 1;
            if(selected_item_idx < view_top_item_idx) view_top_item_idx = selected_item_idx;
            if(selected_item_idx >= view_top_item_idx + 4) view_top_item_idx = selected_item_idx - 3;
        } 
        else if (ui_state == STATE_PARAM_EDIT) {
            float &val = params[edit_param_target];
            float delta = (hw.encoder1.Pressed()) ? 0.05f : 0.01f;
            
            switch(edit_param_target) {
                case PARAM_BPM: val = fclamp(val + (float)inc, 20.0f, 300.0f); break;
                case PARAM_DIVISION:
                    division_idx = (division_idx + inc < 0) ? 0 : (division_idx + inc > 3 ? 3 : division_idx + inc);
                    val = (float)division_vals[division_idx];
                    break;
                case PARAM_PITCH: val += (float)inc * 0.05f; break;
                case PARAM_GRAIN_SIZE: val = fclamp(val + (float)inc * 0.005f, 0.002f, 0.5f); break;
                case PARAM_GRAINS: val = fclamp(val + (float)inc, 0.5f, 50.0f); break;
                default: val = fclamp(val + (float)inc * delta, 0.0f, 1.0f); break;
            }
            effective_params[edit_param_target] = val;
        }
    }

    UpdateBufferLen();
    UpdateGrainParams();
}

void Processing::UpdateBufferLen() {
    if (looper_state == LP_PLAY || looper_state == LP_STOP) {
        buffer_len_samples = loop_len;
    } else {
        float bpm = effective_params[PARAM_BPM]; 
        float division = params[PARAM_DIVISION]; 
        float loop_len_sec = (1.0f / (bpm / 60.0f)) * (4.0f / division);
        buffer_len_samples = (uint32_t)(loop_len_sec * sample_rate_);
    }
    
    if(buffer_len_samples > LOOPER_MAX_SAMPLES) buffer_len_samples = LOOPER_MAX_SAMPLES;
    if(buffer_len_samples < 4) buffer_len_samples = 4;
}

void Processing::UpdateGrainParams() {
    float density_hz = effective_params[PARAM_GRAINS]; 
    if(density_hz < 0.1f) density_hz = 0.1f; 
    float base_int = sample_rate_ / density_hz;
    float stereo = effective_params[PARAM_STEREO];
    
    grain_trig_interval_l = (uint32_t)(base_int * ((1.0f - stereo) + (rand_.Process() * stereo)));
    grain_trig_interval_r = (uint32_t)(base_int * ((1.0f - stereo) + (rand_.Process() * stereo)));
    
    if(grain_trig_interval_l == 0) grain_trig_interval_l = 1;
    if(grain_trig_interval_r == 0) grain_trig_interval_r = 1;
}

void Processing::GetSample(float &outl, float &outr, float inl, float inr) {
    float pre_gain = effective_params[PARAM_PRE_GAIN] * 2.0f; 
    float fbk = effective_params[PARAM_FEEDBACK];
    float mix = effective_params[PARAM_MIX]; 
    float post_gain = effective_params[PARAM_POST_GAIN] * 2.0f;
    
    // 1. Process Input
    float wet_in = (inl * pre_gain + inr * pre_gain) * 0.5f; 

    // 2. Process Granular Engine (Reads from active_buffer)
    float wet_l = 0.0f; float wet_r = 0.0f;

    // Only generate/process grains if not stopped
    if (looper_state != LP_STOP) {
        float stereo = effective_params[PARAM_STEREO];
        float spray = effective_params[PARAM_SPRAY];

        // Trigger Logic
        if(grain_trig_counter_l == 0) {
            float sz_mod = (1.0f - stereo) + (rand_.Process() * stereo);
            uint32_t sz = (uint32_t)(effective_params[PARAM_GRAIN_SIZE] * sample_rate_ * sz_mod);
            
            float cursor = (looper_state == LP_EMPTY) ? (float)write_pos : (float)play_pos;
            float start = cursor - (rand_.Process() * spray * 0.5f * sample_rate_);
            
            for(int i = 0; i < MAX_GRAINS; i++) { 
                if(!grains_l[i].active) { 
                    grains_l[i].Start(start, effective_params[PARAM_PITCH], sz, sample_rate_, buffer_len_samples); 
                    break; 
                }
            }
            UpdateGrainParams(); grain_trig_counter_l = grain_trig_interval_l;
        } grain_trig_counter_l--;

        if(grain_trig_counter_r == 0) {
            float sz_mod = (1.0f - stereo) + (rand_.Process() * stereo);
            uint32_t sz = (uint32_t)(effective_params[PARAM_GRAIN_SIZE] * sample_rate_ * sz_mod);
            
            float cursor = (looper_state == LP_EMPTY) ? (float)write_pos : (float)play_pos;
            float start = cursor - (rand_.Process() * spray * 0.5f * sample_rate_);

            for(int i = 0; i < MAX_GRAINS; i++) { 
                if(!grains_r[i].active) { 
                    grains_r[i].Start(start, effective_params[PARAM_PITCH], sz, sample_rate_, buffer_len_samples); 
                    break; 
                }
            }
            grain_trig_counter_r = grain_trig_interval_r;
        } grain_trig_counter_r--;

        // Sum active grains
        for(int i = 0; i < MAX_GRAINS; i++) { 
            wet_l += grains_l[i].Process(active_buffer, buffer_len_samples); 
            wet_r += grains_r[i].Process(active_buffer, buffer_len_samples); 
        }
        wet_l *= 0.5f; wet_r *= 0.5f;
    }

    // 3. Buffer Writing (Rec / Live)
    // We calculate the signal to record *including* the granular output for resampling
    float granular_sum = (wet_l + wet_r) * 0.5f;

    if (looper_state == LP_REC) {
        // Resampling: Record Input + (GranularOutput * Feedback)
        if (rec_pos < LOOPER_MAX_SAMPLES) {
            rec_buffer[rec_pos] = wet_in + (granular_sum * fbk);
            rec_pos++;
        }
    } 
    else if (looper_state == LP_EMPTY) {
        // Live Mode: Circular buffer
        // Note: In Live mode, we usually feed the input + feedback(delay style)
        float old_samp = active_buffer[write_pos];
        active_buffer[write_pos] = fclamp(wet_in + (old_samp * fbk), -1.0f, 1.0f);
        write_pos++;
        if (write_pos >= buffer_len_samples) write_pos = 0;
    }

    // 4. Update Playhead
    if (looper_state == LP_PLAY) {
        play_pos++;
        if (play_pos >= buffer_len_samples) play_pos = 0;
    }
    
    // 5. Final Output
    outl = (inl * pre_gain * (1.0f - mix) + wet_l * mix) * post_gain; 
    outr = (inr * pre_gain * (1.0f - mix) + wet_r * mix) * post_gain;
}