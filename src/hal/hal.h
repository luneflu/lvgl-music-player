/**
 * @file hal.h
 */

#ifndef LV_VSCODE_HAL_H
#define LV_VSCODE_HAL_H

#include "lvgl/lvgl.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the HAL (display, input devices, tick).
 * On macOS: creates a native Cocoa window.
 */
lv_display_t * hal_init(int32_t w, int32_t h);

/**
 * Run the platform event loop.
 * On macOS: enters [NSApp run] – does not return.
 */
void hal_run(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_VSCODE_HAL_H*/
