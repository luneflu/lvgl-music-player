#include "album_list_model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void album_list_model_init(album_list_model_t *model)
{
    if (!model) return;
    model->items = NULL;
    model->count = 0;
    model->capacity = 0;
}

void album_list_model_free(album_list_model_t *model)
{
    if (!model) return;
    for (size_t i = 0; i < model->count; i++) {
        free(model->items[i].name);
        free(model->items[i].artist);
        free(model->items[i].sample_track_path);
    }
    free(model->items);
    album_list_model_init(model);
}

static void model_add_item(album_list_model_t *model, const char *name, const char *artist, const char *track_path, int32_t track_count)
{
    if (model->count >= model->capacity) {
        size_t new_cap = model->capacity == 0 ? 16 : model->capacity * 2;
        album_item_t *new_items = (album_item_t *)realloc(model->items, new_cap * sizeof(album_item_t));
        if (!new_items) return;
        model->items = new_items;
        model->capacity = new_cap;
    }

    album_item_t *it = &model->items[model->count++];
    it->name = strdup(name ? name : "Unknown Album");
    it->artist = strdup(artist ? artist : "Unknown Artist");
    it->sample_track_path = track_path ? strdup(track_path) : NULL;
    it->track_count = track_count;
}

bool album_list_model_fetch(album_list_model_t *model, sqlite3 *db)
{
    if (!model || !db) return false;
    album_list_model_free(model);

    const char *query =
        "SELECT COALESCE(album, 'Unknown Album'), "
        "       COALESCE(album_artist, artist, 'Unknown Artist'), "
        "       MIN(path), "
        "       COUNT(id) "
        "FROM musics "
        "GROUP BY COALESCE(album, 'Unknown Album') "
        "ORDER BY COALESCE(album, 'Unknown Album') ASC;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *name = (const char *)sqlite3_column_text(stmt, 0);
        const char *artist = (const char *)sqlite3_column_text(stmt, 1);
        const char *path = (const char *)sqlite3_column_text(stmt, 2);
        int32_t count = sqlite3_column_int(stmt, 3);
        model_add_item(model, name, artist, path, count);
    }

    sqlite3_finalize(stmt);
    return true;
}
