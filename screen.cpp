#include "screen.h"
#include <cstdio>
#include <string.h> 
#include <math.h>   
#include "processing.h" 

using namespace daisy;
using daisy::OledDisplay;

static daisy::OledDisplay<daisy::SSD130xI2c128x64Driver> display;

void Screen::Init(DaisySeed &seed) {
    OledDisplay<OledDriver>::Config disp_cfg;
    disp_cfg.driver_config.transport_config.i2c_config.periph = I2CHandle::Config::Peripheral::I2C_1;
    disp_cfg.driver_config.transport_config.i2c_config.pin_config.sda = seed.GetPin(12);
    disp_cfg.driver_config.transport_config.i2c_config.pin_config.scl = seed.GetPin(11);
    display.Init(disp_cfg);
}

void Screen::Blink(uint32_t now) { blink_active = true; blink_start = now; }

void Screen::DrawStatus(Processing &proc, Hardware &hw) {
    display.Fill(false);
    
    // Blink Indicator
    if (proc.trigger_blink) { Blink(System::GetNow()); proc.trigger_blink = false; }
    if (blink_active) {
        display.DrawRect(0, 0, 127, 63, true, false);
        if (System::GetNow() - blink_start > 100) blink_active = false;
    }

    char buf[32];

    if (proc.ui_state == Processing::STATE_MENU_NAV) {
        // --- Menu Navigation Mode ---
        
        // Draw Header
        display.SetCursor(0, 0);
        display.WriteString("MENU", Font_7x10, true);

        // Draw Menu Items
        int y_start = 12;
        int line_h = 10;
        
        for (int i = 0; i < 5; i++) {
            int item_idx = proc.view_top_item_idx + i;
            if (item_idx >= proc.current_menu_size) break;

            int y = y_start + (i * line_h);
            
            // Cursor
            if (item_idx == proc.selected_item_idx) {
                display.SetCursor(0, y);
                display.WriteString(">", Font_6x8, true);
            }
            
            // Item Name
            display.SetCursor(10, y);
            display.WriteString(proc.current_menu[item_idx].name, Font_6x8, true);
        }
        
    } else if (proc.ui_state == Processing::STATE_PARAM_EDIT) {
        // --- Parameter Edit Mode ---
        
        // Display the name of the parameter being controlled
        // instead of [advanced]
        const MenuItem &item = proc.GetSelectedItem();
        
        display.SetCursor(0, 0);
        snprintf(buf, 32, "[ %s ]", item.name);
        display.WriteString(buf, Font_7x10, true);
        
        // Value
        display.SetCursor(0, 25);
        float val = proc.params[proc.edit_param_target];
        
        // Formatting based on type
        if(proc.edit_param_target == PARAM_BPM) {
            snprintf(buf, 32, "%.1f BPM", val);
        } else if (proc.edit_param_target == PARAM_DIVISION) {
            snprintf(buf, 32, "1/%d", (int)val);
        } else {
            snprintf(buf, 32, "%.2f", val);
        }
        display.WriteString(buf, Font_11x18, true);
        
        // Bar
        display.DrawRect(0, 50, (int)(val * 127.0f), 60, true, true);
    }

    display.Update();
}