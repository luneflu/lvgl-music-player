#ifndef ALBUM_MODEL_H
#define ALBUM_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sqlite3.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char *name;
    char *artist;
    char *sample_track_path;
    int32_t track_count;
} album_item_t;

typedef struct {
    album_item_t *items;
    size_t count;
    size_t capacity;
} album_list_model_t;

void album_list_model_init(album_list_model_t *model);
void album_list_model_free(album_list_model_t *model);
bool album_list_model_fetch(album_list_model_t *model, sqlite3 *db);

#ifdef __cplusplus
}
#endif

#endif /* ALBUM_MODEL_H */
