#include "screen.h"
#include <cstdio>
#include <string.h> 
#include <math.h>   
#include "processing.h" 

using namespace daisy;
using daisy::OledDisplay;

static daisy::OledDisplay<daisy::SSD130xI2c128x64Driver> display;

const int kTextColX     = 5;
const int kTextColWidth = 55; 
const int kBarColX      = 64; 
const int kBarColWidth  = 64; 

static void DrawSelectionIndicator(int y, bool engaged) {
    int x = 0; 
    // Draw arrow pointing right at the item
    display.DrawLine(x, y, x+3, y+4, true);
    display.DrawLine(x, y+8, x+3, y+4, true);
    if (engaged) { 
        // Fill triangle if editing
        display.DrawLine(x+1, y+1, x+1, y+7, true);
    }
}

static void DrawHighlightBox(int x, int y, int w, int h, bool on) {
    display.DrawRect(x, y, x + w - 1, y + h - 1, on, true);
}

static float GetNormVal(int param_id, float val) {
    float norm = 0.0f;
    switch(param_id) {
        case PARAM_PRE_GAIN: 
        case PARAM_POST_GAIN: 
        case PARAM_FEEDBACK: 
        case PARAM_MIX: 
        case PARAM_STEREO: 
        case PARAM_SPRAY:     norm = val; break;
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
    
    // Draw Border
    display.DrawRect(kBarColX, y, kBarColX + kBarColWidth - 1, y + bar_h - 1, true, false);
    // Draw Fill
    if (w > 0) {
        display.DrawRect(kBarColX, y, kBarColX + w - 1, y + bar_h - 1, true, true);
    }
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

    // --- Header (Active Page) ---
    char buf[32];
    snprintf(buf, 32, "PAGE: %s", proc.current_page_name);
    display.SetCursor(0, 0);
    display.WriteString(buf, Font_7x10, true);
    display.DrawLine(0, 11, 127, 11, true);

    // --- List Items ---
    int y_start = 14;
    int line_h = 12;

    for(int i = 0; i < 4; i++) {
        int idx = proc.view_top_item_idx + i;
        if(idx >= proc.current_menu_size) { break; }

        const MenuItem& item = proc.current_menu_items[idx];
        bool sel = (idx == proc.selected_item_idx);
        bool edit = (sel && proc.ui_state == Processing::STATE_PARAM_EDIT);
        int y = y_start + i * line_h;

        // Selection Indicator
        if (sel) DrawSelectionIndicator(y, edit);

        // Name
        display.SetCursor(kTextColX, y);
        display.WriteString(item.name, Font_6x8, true);

        // Value Bar / Text
        if (item.type == TYPE_PARAM) {
            float val = proc.params[item.param_id];
            
            // For specific params, show text instead of bar
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
                 // Explicitly show percentage text for Map Amt so user sees change when clicking
                 snprintf(buf, 16, "%d%%", (int)(val * 100.0f));
                 display.SetCursor(kBarColX, y);
                 display.WriteString(buf, Font_6x8, true);
            }
            else {
                // Draw Bar for continuous values
                float n = GetNormVal(item.param_id, val);
                DrawValueBar(y, n);
            }
        }
    }

    display.Update();
}