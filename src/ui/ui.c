#include <stdio.h>
#include "../../lvgl/lvgl.h"
#include "../db/db.h"
#include "album_list_model.h"
#include "album_list_view.h"

void ui_example(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(screen, 16, 0);
    lv_obj_set_style_pad_row(screen, 12, 0);

    static album_list_model_t model;
    album_list_model_init(&model);
    album_list_model_fetch(&model, g_db);

    album_list_view_create(screen, &model);
}
