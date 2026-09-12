/**
 * @file main.c
 */

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE /* needed for usleep() */
#endif

#include <stdlib.h>
#include <stdio.h>
#ifdef _MSC_VER
#include <Windows.h>
#elif !defined(__APPLE__)
#include <unistd.h>
#include <pthread.h>
#endif

#include "lvgl/lvgl.h"
#include "lvgl/examples/lv_examples.h"
#include "lvgl/demos/lv_demos.h"

#include "hal/hal.h"
#include "db/db.h"
#include "scanner/scanner.h"
#include "ui/ui.c"

#if LV_USE_OS != LV_OS_FREERTOS

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;

  db_init(NULL);
  scanner_scan_and_save(g_db, "/Users/alvindimas05/Music/The Meaning of Life");

  lv_init();

  hal_init(800, 480);

  lv_demo_widgets();
  // ui_example();

#ifdef __APPLE__
  /* On macOS the Cocoa run-loop drives lv_timer_handler via NSTimer */
  hal_run();
#else
  while (1)
  {
    uint32_t sleep_time_ms = lv_timer_handler();
    if (sleep_time_ms == LV_NO_TIMER_READY)
    {
      sleep_time_ms = LV_DEF_REFR_PERIOD;
    }
#ifdef _MSC_VER
    Sleep(sleep_time_ms);
#else
    usleep(sleep_time_ms * 1000);
#endif
  }
#endif

  db_close();
  return 0;
}

#endif
