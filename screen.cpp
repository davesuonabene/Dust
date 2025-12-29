#include "screen.h"
#include <cstdio>
#include <string.h> 
#include <math.h>   
#include "processing.h" 

using namespace daisy;
using daisy::OledDisplay;

static daisy::OledDisplay<daisy::SSD130xI2c128x64Driver> display;

// Standard coordinates
const int kSelectorColX = 0;
const int kTextColX     = 10;
const int kBarColX      = 70; 
const int kBarColWidth  = 50; 

static void DrawChar(OledDisplay<OledDriver> &disp, int x, int y, char ch, const FontDef &font, bool on) {
    if(ch < 32 || ch > 126) { return; }
    for(int i = 0; i < (int)font.FontHeight; i++) {
        uint32_t rowBits = font.data[(ch - 32) * font.FontHeight + i];
        for(int j = 0; j < (int)font.FontWidth; j++) {
            bool bit_on = (rowBits << j) & 0x8000;
            int  rx     = x + j;
            int  ry     = y + i;
            if(rx >= 0 && ry >= 0 && rx < (int)disp.Width() && ry < (int)disp.Height()) {
                disp.DrawPixel((uint_fast8_t)rx, (uint_fast8_t)ry, bit_on ? on : !on);
            }
        }
    }
}

static void DrawString(OledDisplay<OledDriver> &disp, int x, int y, const char * str, const FontDef &font, bool on) {
    int cx = x;
    while(*str) { 
        DrawChar(disp, cx, y, *str, font, on); 
        cx += font.FontWidth; 
        ++str; 
    }
}

static void DrawValueBar(int y, float val) {
    int bar_h  = 6;
    int w = (int)(val * (float)kBarColWidth);
    
    // Draw Border
    display.DrawRect(kBarColX, y, kBarColX + kBarColWidth, y + bar_h, true, false);
    
    // Draw Fill
    if (w > 0) {
        display.DrawRect(kBarColX, y, kBarColX + w, y + bar_h, true, true);
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
    display.Fill(false);

    int y = 0;
    int line_h = 10;
    
    // 1. Draw Header (Page Name)
    char header[32];
    snprintf(header, 32, "[ %s ]", proc.current_page_name);
    DrawString(display, 0, 0, header, Font_7x10, true);
    
    // Draw Separator
    display.DrawLine(0, 11, 128, 11, true);

    // 2. Draw Menu Items
    // Simple scrolling view: always draw from the top below header
    y = 14; 
    
    int start_idx = 0; 
    // Keep selection in view (simple 4-item view window logic)
    if(proc.selected_item_idx > 3) start_idx = proc.selected_item_idx - 3;

    for (int i = start_idx; i < proc.current_menu_size && i < start_idx + 4; i++)
    {
        const MenuItem& item = proc.current_menu_items[i];
        
        // Draw Selection Cursor
        if (i == proc.selected_item_idx) {
            // If Editing, draw solid block or distinct marker
            if (proc.ui_state == Processing::STATE_PARAM_EDIT) {
                 DrawString(display, kSelectorColX, y, "*", Font_7x10, true);
            } else {
                 DrawString(display, kSelectorColX, y, ">", Font_7x10, true);
            }
        }

        // Draw Name
        DrawString(display, kTextColX, y, item.name, Font_7x10, true);

        // Draw Value
        if (item.type == TYPE_PARAM)
        {
            float val = proc.effective_params[item.param_id];
            DrawValueBar(y + 2, val);
        }
        
        y += line_h;
    }

    display.Update();
}