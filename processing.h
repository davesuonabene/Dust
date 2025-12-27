#pragma once
#include "daisysp.h"
#include "hw.h"
#include "config.h"

using namespace daisy;
using namespace daisysp;

// --- Parameter Enum ---
enum Param
{
    PARAM_PRE_GAIN,
    PARAM_FEEDBACK,
    PARAM_MIX,
    PARAM_POST_GAIN,
    PARAM_BPM,
    PARAM_DIVISION,
    PARAM_PITCH,
    PARAM_GRAIN_SIZE,
    PARAM_GRAINS, 
    PARAM_SPRAY,  
    PARAM_STEREO,
    PARAM_MAP_AMT, 
    PARAM_COUNT
};

enum MenuItemType
{
    TYPE_PARAM,           
    TYPE_SUBMENU,         
    TYPE_PARAM_SUBMENU,   
    TYPE_BACK             
};

struct MenuItem
{
    const char* name;
    MenuItemType type;
    int param_id;             
    const MenuItem* submenu;  
    int num_children; 
};

extern const MenuItem kMenuMain[];
extern const int kMenuMainSize;
extern const MenuItem kMenuBpmEdit[];
extern const int kMenuBpmEditSize;
extern const MenuItem kMenuPostEdit[];
extern const int kMenuPostEditSize;
extern const MenuItem kMenuGrainsEdit[];
extern const int kMenuGrainsEditSize;
extern const MenuItem kMenuGenericEdit[];
extern const int kMenuGenericEditSize;

struct Processing
{
    struct Grain
    {
        inline float TriEnv(float pos)
        {
            if(pos < 0.5f) return pos * 2.0f; 
            return (1.0f - pos) * 2.0f;
        }
        bool     active = false;
        float    read_pos;
        float    increment;
        float    env_pos;
        float    env_inc;
        uint32_t size_samples;

        void Start(float start_pos, float pitch, uint32_t size_samps, float sample_rate, size_t buffer_len)
        {
            active       = true;
            while(start_pos < 0.0f) start_pos += (float)buffer_len;
            while(start_pos >= (float)buffer_len) start_pos -= (float)buffer_len;
            read_pos     = start_pos;
            increment    = pitch;
            size_samples = size_samps < 4 ? 4 : size_samps;
            env_pos      = 0.0f;
            env_inc      = 1.0f / (float)size_samples;
        }

        float Process(float *buffer, size_t buffer_len)
        {
            if(!active) return 0.0f;
            int32_t i_idx  = (int32_t)read_pos;
            float   frac   = read_pos - i_idx;
            float   samp_a = buffer[i_idx];
            float   samp_b = buffer[(i_idx + 1) % buffer_len];
            float   samp   = samp_a + (samp_b - samp_a) * frac;
            float amp = TriEnv(env_pos);
            read_pos += increment;
            while(read_pos >= buffer_len) read_pos -= buffer_len;
            while(read_pos < 0) read_pos += buffer_len;
            env_pos += env_inc;
            if(env_pos >= 1.0f) active = false;
            return samp * amp;
        }
    };

    struct Rand
    {
        uint32_t seed_ = 1;
        float Process()
        {
            seed_ = (seed_ * 1664525L + 1013904223L) & 0xFFFFFFFF;
            return (float)seed_ / 4294967295.0f;
        }
    };

    enum UiState { STATE_MENU_NAV, STATE_PARAM_EDIT };

    static float    DSY_SDRAM_BSS buffer[MAX_BUFFER_SAMPLES];
    uint32_t        write_pos         = 0;
    uint32_t        buffer_len_samples = 48000;

    static Grain    grains_l[MAX_GRAINS];
    static Grain    grains_r[MAX_GRAINS];
    uint32_t        grain_trig_counter_l = 0;
    uint32_t        grain_trig_counter_r = 0;
    uint32_t        grain_trig_interval_l = 2400; 
    uint32_t        grain_trig_interval_r = 2400; 

    float           params[PARAM_COUNT];           
    float           knob_map_amounts[PARAM_COUNT]; 
    float           effective_params[PARAM_COUNT]; 
    
    int             division_idx = 0; 
    const int       division_vals[4] = {1, 2, 4, 8}; 
    float           sample_rate_ = 48000.0f;
    Rand            rand_;
    
    UiState         ui_state = STATE_MENU_NAV;
    const MenuItem* current_menu = kMenuMain; 
    int             current_menu_size = kMenuMainSize;
    int             selected_item_idx = 0;
    int             view_top_item_idx = 0; 

    int             edit_param_target = 0; 
    char            parent_menu_name[16]; 
    
    const uint32_t  kHoldTimeMs = 500; 
    bool            enc_is_holding = false;
    uint32_t        enc_hold_start = 0;

    // --- Button Logic Variables ---
    uint32_t        last_looper_toggle = 0; 
    bool            long_press_active = false; 
    bool            trigger_blink = false;

    void Init(Hardware &hw);
    void Controls(Hardware &hw);
    void GetSample(float &outl, float &outr, float inl, float inr);
    void UpdateBufferLen();
    void UpdateGrainParams();
    
    const MenuItem& GetSelectedItem() { return current_menu[selected_item_idx]; }
};