#include "screen.h"
#include <cstdio>
#include <string.h> 
#include <math.h>   
#include "processing.h" 

using namespace daisy;
using daisy::OledDisplay;

static daisy::OledDisplay<daisy::SSD130xI2c128x64Driver> display;

const int kTextColX     = 5;
const int kBarColX      = 64; 
const int kBarColWidth  = 64; 

static void DrawSelectionIndicator(int y, bool engaged) {
    int x = 0; 
    display.DrawLine(x, y, x+3, y+4, true);
    display.DrawLine(x, y+8, x+3, y+4, true);
    if (engaged) display.DrawLine(x+1, y+1, x+1, y+7, true);
}

static float GetNormVal(int param_id, float val) {
    float norm = 0.0f;
    switch(param_id) {
        case PARAM_PRE_GAIN: case PARAM_POST_GAIN: case PARAM_FEEDBACK: 
        case PARAM_MIX: case PARAM_STEREO: case PARAM_SPRAY:     norm = val; break;
        case PARAM_BPM:       norm = (val - 20.f) / (300.f - 20.f); break;
        case PARAM_DIVISION:  norm = 0.5f; break; 
        case PARAM_PITCH:     norm = (val + 0.5f) / 2.0f; break; 
        case PARAM_GRAIN_SIZE: norm = (val - 0.002f) / (0.5f - 0.002f); break;
        case PARAM_GRAINS:    norm = (val - 0.5f) / (50.f - 0.5f); break;
        case PARAM_MAP_AMT:   norm = val; break; 
        default: break;
    }
    return fclamp(norm, 0.0f, 1.0f);
}

static void DrawValueBar(int y, float norm) {
    int bar_h  = 8; 
    int w = (int)(norm * (float)kBarColWidth);
    display.DrawRect(kBarColX, y, kBarColX + kBarColWidth - 1, y + bar_h - 1, true, false);
    if (w > 0) display.DrawRect(kBarColX, y, kBarColX + w - 1, y + bar_h - 1, true, true);
}

void Screen::Init(DaisySeed &seed) {
    OledDisplay<OledDriver>::Config disp_cfg;
    disp_cfg.driver_config.transport_config.i2c_config.periph = I2CHandle::Config::Peripheral::I2C_1;
    disp_cfg.driver_config.transport_config.i2c_config.pin_config.sda = seed.GetPin(12);
    disp_cfg.driver_config.transport_config.i2c_config.pin_config.scl = seed.GetPin(11);
    display.Init(disp_cfg);
}

void Screen::Blink(uint32_t now) { blink_active = true; blink_start = now; }

void Screen::DrawStatus(Processing &proc, Hardware &hw) {
    if (proc.trigger_blink) { Blink(System::GetNow()); proc.trigger_blink = false; }
    display.Fill(false);
    if (blink_active && (System::GetNow() - blink_start < 100)) { display.Fill(true); display.Update(); return; }

    // --- Header ---
    char buf[32];
    snprintf(buf, 32, "PAGE: %s", proc.current_page_name);
    display.SetCursor(0, 0);
    display.WriteString(buf, Font_7x10, true);
    display.DrawLine(0, 11, 127, 11, true);

    // --- Looper Page Visualization ---
    if (strcmp(proc.current_page_name, "LOOPER") == 0) {
        // State Text
        const char* state_str = "EMPTY";
        switch(proc.looper_state) {
            case Processing::LP_EMPTY: state_str = "LIVE INPUT"; break;
            case Processing::LP_REC:   state_str = "RECORDING"; break;
            case Processing::LP_PLAY:  state_str = "PLAYING"; break;
            case Processing::LP_STOP:  state_str = "STOPPED"; break;
        }
        display.SetCursor(10, 20);
        display.WriteString(state_str, Font_11x18, true);

        // Progress Bar (Only if not Empty)
        if (proc.looper_state != Processing::LP_EMPTY) {
            int bar_x = 10; int bar_y = 45; int bar_w = 108; int bar_h = 10;
            display.DrawRect(bar_x, bar_y, bar_x + bar_w, bar_y + bar_h, true, false);
            
            float progress = 0.0f;
            if (proc.looper_state == Processing::LP_REC) {
                progress = (float)proc.rec_pos / (float)LOOPER_MAX_SAMPLES; 
            } else if (proc.looper_state == Processing::LP_PLAY && proc.loop_len > 0) {
                progress = (float)proc.play_pos / (float)proc.loop_len;
            }

            if (progress > 1.0f) progress = 1.0f;
            int fill_w = (int)(progress * (float)bar_w);
            if (fill_w > 0) display.DrawRect(bar_x, bar_y, bar_x + fill_w, bar_y + bar_h, true, true);
        }
    } 
    else {
        // --- Standard List View ---
        int y_start = 14; int line_h = 12;
        for(int i = 0; i < 4; i++) {
            int idx = proc.view_top_item_idx + i;
            if(idx >= proc.current_menu_size) break;

            const MenuItem& item = proc.current_menu_items[idx];
            bool sel = (idx == proc.selected_item_idx);
            bool edit = (sel && proc.ui_state == Processing::STATE_PARAM_EDIT);
            int y = y_start + i * line_h;

            if (sel) DrawSelectionIndicator(y, edit);

            display.SetCursor(kTextColX, y);
            display.WriteString(item.name, Font_6x8, true);

            if (item.type == TYPE_PARAM && item.param_id >= 0) {
                float val = proc.params[item.param_id];
                if (item.param_id == PARAM_DIVISION) {
                    snprintf(buf, 16, "1/%d", (int)val);
                    display.SetCursor(kBarColX, y);
                    display.WriteString(buf, Font_6x8, true);
                } 
                else if (item.param_id == PARAM_BPM) {
                     snprintf(buf, 16, "%.0f", val);
                     display.SetCursor(kBarColX, y);
                     display.WriteString(buf, Font_6x8, true);
                }
                else if (item.param_id == PARAM_MAP_AMT) {
                     snprintf(buf, 16, "%d%%", (int)(val * 100.0f));
                     display.SetCursor(kBarColX, y);
                     display.WriteString(buf, Font_6x8, true);
                }
                else {
                    DrawValueBar(y, GetNormVal(item.param_id, val));
                }
            }
        }
    }
    display.Update();
}