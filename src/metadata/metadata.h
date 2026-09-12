#ifndef MUSIC_METADATA_H
#define MUSIC_METADATA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define METADATA_DEFAULT_TITLE "Unknown Title"
#define METADATA_DEFAULT_ARTIST "Unknown Artist"

typedef struct {
    int16_t id;
    char *path;
    char *filename;
    uint64_t duration_ms;

    char *title;
    char *artist;
    char *album;
    char *album_artist;
    char *track_number;
    char *genre;
    char *date;

    uint32_t bits_per_sample;
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t bitrate;

    char *image_path;
} music_metadata_t;

typedef struct {
    uint8_t *data;
    size_t size;
    char mime_type[64];
} music_cover_art_t;

void music_metadata_init(music_metadata_t *meta);
void music_metadata_free(music_metadata_t *meta);

bool music_metadata_read(const char *file_path, music_metadata_t *out_meta);
bool music_metadata_read_cover(const char *file_path, music_cover_art_t *out_cover);
void music_cover_art_free(music_cover_art_t *cover);

char *music_metadata_read_lyrics(const char *file_path);
void music_metadata_parse_filename(const char *filename, char **out_artist, char **out_title);

#ifdef __cplusplus
}
#endif

#endif /* MUSIC_METADATA_H */
