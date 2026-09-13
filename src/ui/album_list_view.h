#ifndef ALBUM_LIST_VIEW_H
#define ALBUM_LIST_VIEW_H

#include "../../lvgl/lvgl.h"
#include "album_list_model.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *album_list_view_create(lv_obj_t *parent, const album_list_model_t *model);

#ifdef __cplusplus
}
#endif

#endif /* ALBUM_LIST_VIEW_H */
