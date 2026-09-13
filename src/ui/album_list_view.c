#include "album_list_view.h"
#include "../metadata/metadata.h"

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "../../lvgl/src/libs/gltf/stb_image/stb_image.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CARD_GAP 12

// Per-card decoded cover art bitmap
typedef struct
{
    lv_draw_buf_t *draw_buf;
    char *track_path;
    lv_obj_t *cover_img;
    bool in_flight;
    bool deleted;
} album_card_asset_t;

// Queue item for background worker thread
typedef struct cover_task
{
    album_card_asset_t *asset;
    char *track_path;
    lv_draw_buf_t *decoded_buf;
    struct cover_task *next;
} cover_task_t;

// Global thread pool queue for image decoding
static pthread_mutex_t g_queue_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_queue_cond = PTHREAD_COND_INITIALIZER;
static cover_task_t *g_queue_head = NULL;
static cover_task_t *g_queue_tail = NULL;
static bool g_worker_running = false;
static pthread_t g_worker_thread;

// View state keeping track of card elements to resize on container width changes
typedef struct
{
    lv_obj_t **cards;
    lv_obj_t **cover_imgs;
    album_card_asset_t **assets;
    size_t count;
} album_list_view_t;

// Calculate responsive column count from container width
static uint32_t get_visible_col_count(int32_t width)
{
    if (width >= 1536)
        return 6;
    if (width >= 1280)
        return 5;
    if (width >= 1024)
        return 4;
    if (width >= 768)
        return 3;
    if (width >= 640)
        return 2;
    return 2;
}

// Decode raw JPEG/PNG cover bytes into ARGB8888 lv_draw_buf_t (runs on background thread)
static lv_draw_buf_t *load_cover_draw_buf(const char *track_path)
{
    if (!track_path)
        return NULL;

    music_cover_art_t cover = {0};
    if (!music_metadata_read_cover(track_path, &cover) || !cover.data || cover.size == 0)
    {
        return NULL;
    }

    int w = 0, h = 0, channels = 0;
    // Decode directly to 4-channel RGBA (ARGB8888 in LVGL)
    unsigned char *pixels = stbi_load_from_memory(cover.data, (int)cover.size, &w, &h, &channels, 4);
    music_cover_art_free(&cover);

    if (!pixels || w <= 0 || h <= 0)
    {
        if (pixels)
            stbi_image_free(pixels);
        return NULL;
    }

    // Convert RGBA -> ARGB in native lv_draw_buf_t
    lv_draw_buf_t *draw_buf = lv_draw_buf_create((uint32_t)w, (uint32_t)h, LV_COLOR_FORMAT_ARGB8888, 0);
    if (draw_buf)
    {
        uint8_t *dst = (uint8_t *)draw_buf->data;
        uint32_t stride = draw_buf->header.stride;
        for (int y = 0; y < h; y++)
        {
            const uint8_t *src_row = pixels + (y * w * 4);
            uint8_t *dst_row = dst + (y * stride);
            for (int x = 0; x < w; x++)
            {
                uint8_t r = src_row[x * 4 + 0];
                uint8_t g = src_row[x * 4 + 1];
                uint8_t b = src_row[x * 4 + 2];
                uint8_t a = src_row[x * 4 + 3];

                // ARGB8888 memory order (BGRA on little-endian)
                dst_row[x * 4 + 0] = b;
                dst_row[x * 4 + 1] = g;
                dst_row[x * 4 + 2] = r;
                dst_row[x * 4 + 3] = a;
            }
        }
    }

    stbi_image_free(pixels);
    return draw_buf;
}

// UI thread callback: update image widget with decoded draw buffer
static void apply_cover_async_cb(void *user_data)
{
    cover_task_t *task = (cover_task_t *)user_data;
    if (!task)
        return;

    pthread_mutex_lock(&g_queue_lock);
    album_card_asset_t *asset = task->asset;
    if (asset && !asset->deleted)
    {
        asset->draw_buf = task->decoded_buf;
        if (asset->draw_buf && asset->cover_img)
        {
            lv_image_set_src(asset->cover_img, asset->draw_buf);
        }
        asset->in_flight = false;
    }
    else
    {
        if (task->decoded_buf)
        {
            lv_draw_buf_destroy(task->decoded_buf);
        }
        if (asset && asset->deleted)
        {
            free(asset);
        }
    }
    pthread_mutex_unlock(&g_queue_lock);

    free(task->track_path);
    free(task);
}

// Background thread worker
static void *cover_loader_thread(void *arg)
{
    (void)arg;
    while (1)
    {
        pthread_mutex_lock(&g_queue_lock);
        while (!g_queue_head && g_worker_running)
        {
            pthread_cond_wait(&g_queue_cond, &g_queue_lock);
        }

        if (!g_worker_running && !g_queue_head)
        {
            pthread_mutex_unlock(&g_queue_lock);
            break;
        }

        cover_task_t *task = g_queue_head;
        g_queue_head = task->next;
        if (!g_queue_head)
            g_queue_tail = NULL;
        pthread_mutex_unlock(&g_queue_lock);

        // Perform decoding without holding locks
        task->decoded_buf = load_cover_draw_buf(task->track_path);

        // Schedule async dispatch to LVGL main thread
        lv_async_call(apply_cover_async_cb, task);
    }
    return NULL;
}

static void ensure_worker_started(void)
{
    pthread_mutex_lock(&g_queue_lock);
    if (!g_worker_running)
    {
        g_worker_running = true;
        pthread_create(&g_worker_thread, NULL, cover_loader_thread, NULL);
        pthread_detach(g_worker_thread);
    }
    pthread_mutex_unlock(&g_queue_lock);
}

static void enqueue_cover_task(album_card_asset_t *asset)
{
    if (!asset || !asset->track_path)
        return;

    ensure_worker_started();

    cover_task_t *task = (cover_task_t *)calloc(1, sizeof(cover_task_t));
    if (!task)
        return;
    task->asset = asset;
    task->track_path = strdup(asset->track_path);

    pthread_mutex_lock(&g_queue_lock);
    asset->in_flight = true;
    if (g_queue_tail)
    {
        g_queue_tail->next = task;
        g_queue_tail = task;
    }
    else
    {
        g_queue_head = g_queue_tail = task;
    }
    pthread_cond_signal(&g_queue_cond);
    pthread_mutex_unlock(&g_queue_lock);
}

// Clean up decoded draw buffer when card object is deleted
static void card_delete_cb(lv_event_t *e)
{
    album_card_asset_t *asset = (album_card_asset_t *)lv_event_get_user_data(e);
    if (!asset)
        return;

    pthread_mutex_lock(&g_queue_lock);
    if (asset->draw_buf)
    {
        lv_draw_buf_destroy(asset->draw_buf);
        asset->draw_buf = NULL;
    }
    if (asset->track_path)
    {
        free(asset->track_path);
        asset->track_path = NULL;
    }

    if (asset->in_flight)
    {
        asset->deleted = true; // Let async callback free memory when worker finishes
        pthread_mutex_unlock(&g_queue_lock);
    }
    else
    {
        pthread_mutex_unlock(&g_queue_lock);
        free(asset);
    }
}

// Update card and cover sizes when container width changes
static void update_card_sizes(lv_obj_t *container, album_list_view_t *view)
{
    int32_t cont_w = lv_obj_get_content_width(container);
    if (cont_w <= 0 || !view)
        return;

    uint32_t vis_cols = get_visible_col_count(cont_w);
    int32_t card_w = (cont_w - (int32_t)(vis_cols - 1) * CARD_GAP) / (int32_t)vis_cols;
    if (card_w <= 0)
        card_w = 1;

    for (size_t i = 0; i < view->count; i++)
    {
        lv_obj_set_width(view->cards[i], card_w);
        lv_obj_set_size(view->cover_imgs[i], card_w, card_w);
    }
}

// Container event callback to handle resizing and cleanup
static void container_event_cb(lv_event_t *e)
{
    album_list_view_t *view = (album_list_view_t *)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *container = lv_event_get_target(e);

    if (code == LV_EVENT_SIZE_CHANGED)
    {
        update_card_sizes(container, view);
    }
    else if (code == LV_EVENT_DELETE)
    {
        if (view)
        {
            free(view->cards);
            free(view->cover_imgs);
            free(view->assets);
            free(view);
        }
    }
}

// Create single album card (cover image placeholder + title + artist)
static lv_obj_t *create_album_card(lv_obj_t *parent, const album_item_t *item, lv_obj_t **out_cover_img, album_card_asset_t **out_asset)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_width(card, 140);
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_set_style_pad_row(card, 4, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(card, 0, 0);

    // Cover art image widget (shows grey placeholder immediately)
    lv_obj_t *cover_img = lv_image_create(card);
    lv_obj_set_size(cover_img, 140, 140);
    lv_image_set_inner_align(cover_img, LV_IMAGE_ALIGN_COVER);
    lv_obj_set_style_radius(cover_img, 6, 0);
    lv_obj_set_style_clip_corner(cover_img, true, 0);
    lv_obj_set_style_bg_color(cover_img, lv_palette_darken(LV_PALETTE_GREY, 3), 0);
    lv_obj_set_style_bg_opa(cover_img, LV_OPA_COVER, 0);

    if (out_cover_img)
        *out_cover_img = cover_img;

    // Asset state prepared for asynchronous background thread loading
    album_card_asset_t *asset = (album_card_asset_t *)calloc(1, sizeof(album_card_asset_t));
    if (asset)
    {
        asset->cover_img = cover_img;
        asset->track_path = item->sample_track_path ? strdup(item->sample_track_path) : NULL;
        lv_obj_add_event_cb(card, card_delete_cb, LV_EVENT_DELETE, asset);
    }
    if (out_asset)
        *out_asset = asset;

    // Title label
    lv_obj_t *album_label = lv_label_create(card);
    lv_label_set_text(album_label, item->name ? item->name : "Unknown Album");
    lv_label_set_long_mode(album_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(album_label, lv_pct(100));
    lv_obj_set_style_text_font(album_label, &lv_font_montserrat_14, 0);

    // Artist label
    lv_obj_t *artist_label = lv_label_create(card);
    lv_label_set_text(artist_label, item->artist ? item->artist : "Unknown Artist");
    lv_label_set_long_mode(artist_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(artist_label, lv_pct(100));
    lv_obj_set_style_text_font(artist_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_opa(artist_label, LV_OPA_70, 0);

    return card;
}

lv_obj_t *album_list_view_create(lv_obj_t *parent, const album_list_model_t *model)
{
    if (!parent || !model)
        return NULL;

    // Outer wrapper for section header + card list
    lv_obj_t *wrapper = lv_obj_create(parent);
    lv_obj_set_size(wrapper, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(wrapper, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(wrapper, 0, 0);
    lv_obj_set_style_pad_row(wrapper, 8, 0);
    lv_obj_set_style_bg_opa(wrapper, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(wrapper, 0, 0);

    // Horizontal scrollable flex container for cards
    lv_obj_t *container = lv_obj_create(wrapper);
    lv_obj_set_size(container, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW);
    lv_obj_set_scroll_dir(container, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_all(container, 8, 0);
    lv_obj_set_style_pad_column(container, CARD_GAP, 0);
    lv_obj_set_style_anim_duration(container, 0, 0); // Disable scroll momentum animation to eliminate frame drops

    // Allocate tracking state for responsive resizing and background decoding
    album_list_view_t *view = (album_list_view_t *)calloc(1, sizeof(album_list_view_t));
    if (view && model->count > 0)
    {
        view->count = model->count;
        view->cards = (lv_obj_t **)calloc(model->count, sizeof(lv_obj_t *));
        view->cover_imgs = (lv_obj_t **)calloc(model->count, sizeof(lv_obj_t *));
        view->assets = (album_card_asset_t **)calloc(model->count, sizeof(album_card_asset_t *));

        for (size_t i = 0; i < model->count; i++)
        {
            view->cards[i] = create_album_card(container, &model->items[i], &view->cover_imgs[i], &view->assets[i]);
            // Offload TagLib read + stb_image decode entirely to background thread
            enqueue_cover_task(view->assets[i]);
        }

        lv_obj_add_event_cb(container, container_event_cb, LV_EVENT_ALL, view);
        update_card_sizes(container, view);
    }

    return wrapper;
}
