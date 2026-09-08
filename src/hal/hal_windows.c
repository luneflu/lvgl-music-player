// hal_windows.c
// Native Windows Win32 HAL for LVGL using lv_windows driver.

#include "hal.h"
#include "lvgl/lvgl.h"

#if defined(_WIN32)

#include <windows.h>

lv_display_t * hal_init(int32_t w, int32_t h)
{
    int32_t zoom_level = 100;
    bool allow_dpi_override = false;
    bool simulator_mode = false;

    lv_display_t * display = lv_windows_create_display(
        L"LVGL Windows Simulator",
        (w > 0) ? w : 800,
        (h > 0) ? h : 480,
        zoom_level,
        allow_dpi_override,
        simulator_mode);
    if (!display) {
        return NULL;
    }

    lv_windows_acquire_pointer_indev(display);
    lv_windows_acquire_keypad_indev(display);
    lv_windows_acquire_encoder_indev(display);

    return display;
}

void hal_run(void)
{
    while (1) {
        uint32_t sleep_time_ms = lv_timer_handler();
        if (sleep_time_ms == LV_NO_TIMER_READY) {
            sleep_time_ms = LV_DEF_REFR_PERIOD;
        }
        Sleep(sleep_time_ms);
    }
}

#endif
