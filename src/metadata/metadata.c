#include "metadata.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <taglib/tag_c.h>

static char *strdup_safe(const char *s)
{
    if (!s) return NULL;
    size_t len = strlen(s);
    char *copy = (char *)malloc(len + 1);
    if (copy) {
        memcpy(copy, s, len + 1);
    }
    return copy;
}

static char *trim_in_place(char *s)
{
    if (!s) return NULL;
    while (isspace((unsigned char)*s)) s++;
    if (*s == 0) return s;

    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
    return s;
}

static const char *get_filename_from_path(const char *path)
{
    if (!path) return "";
    const char *p1 = strrchr(path, '/');
    const char *p2 = strrchr(path, '\\');
    const char *slash = p1 > p2 ? p1 : p2;
    return slash ? slash + 1 : path;
}

void music_metadata_init(music_metadata_t *meta)
{
    if (!meta) return;
    memset(meta, 0, sizeof(*meta));
}

void music_metadata_free(music_metadata_t *meta)
{
    if (!meta) return;
    free(meta->path);
    free(meta->filename);
    free(meta->title);
    free(meta->artist);
    free(meta->album);
    free(meta->album_artist);
    free(meta->track_number);
    free(meta->genre);
    free(meta->date);
    free(meta->image_path);
    music_metadata_init(meta);
}

void music_cover_art_free(music_cover_art_t *cover)
{
    if (!cover) return;
    free(cover->data);
    cover->data = NULL;
    cover->size = 0;
    cover->mime_type[0] = '\0';
}

void music_metadata_parse_filename(const char *filename, char **out_artist, char **out_title)
{
    if (out_artist) *out_artist = NULL;
    if (out_title) *out_title = NULL;
    if (!filename || !*filename) return;

    char buf[512];
    strncpy(buf, filename, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *dot = strrchr(buf, '.');
    if (dot) *dot = '\0';

    char *clean = trim_in_place(buf);
    if (!clean || !*clean) return;

    char *sep = strstr(clean, " - ");
    if (!sep) sep = strstr(clean, " – ");
    if (!sep) sep = strstr(clean, " — ");

    if (sep) {
        *sep = '\0';
        char *left = trim_in_place(clean);
        char *right = trim_in_place(sep + (sep[1] == '-' ? 3 : 5));

        while (*left && (isdigit((unsigned char)*left) || *left == '.' || *left == ' ' || *left == '-')) {
            left++;
        }
        left = trim_in_place(left);

        if (left && *left && out_artist) {
            *out_artist = strdup_safe(left);
        }
        if (right && *right && out_title) {
            *out_title = strdup_safe(right);
        }
    } else {
        if (out_title) {
            *out_title = strdup_safe(clean);
        }
    }
}

bool music_metadata_read(const char *file_path, music_metadata_t *out_meta)
{
    if (!file_path || !out_meta) return false;
    music_metadata_init(out_meta);

    taglib_set_strings_unicode(1);

    TagLib_File *file = taglib_file_new(file_path);
    if (!file || !taglib_file_is_valid(file)) {
        if (file) taglib_file_free(file);
        return false;
    }

    out_meta->path = strdup_safe(file_path);
    out_meta->filename = strdup_safe(get_filename_from_path(file_path));

    TagLib_Tag *tag = taglib_file_tag(file);
    if (tag) {
        char *title = taglib_tag_title(tag);
        char *artist = taglib_tag_artist(tag);
        char *album = taglib_tag_album(tag);
        char *genre = taglib_tag_genre(tag);
        unsigned int year = taglib_tag_year(tag);
        unsigned int track = taglib_tag_track(tag);

        if (title && *title) out_meta->title = strdup_safe(trim_in_place(title));
        if (artist && *artist) out_meta->artist = strdup_safe(trim_in_place(artist));
        if (album && *album) out_meta->album = strdup_safe(trim_in_place(album));
        if (genre && *genre) out_meta->genre = strdup_safe(trim_in_place(genre));

        if (year > 0) {
            char ybuf[16];
            snprintf(ybuf, sizeof(ybuf), "%u", year);
            out_meta->date = strdup_safe(ybuf);
        }

        if (track > 0) {
            char tbuf[16];
            snprintf(tbuf, sizeof(tbuf), "%u", track);
            out_meta->track_number = strdup_safe(tbuf);
        }

        taglib_tag_free_strings();
    }

    const TagLib_AudioProperties *props = taglib_file_audioproperties(file);
    if (props) {
        out_meta->duration_ms = (uint64_t)taglib_audioproperties_length(props) * 1000ULL;
        out_meta->sample_rate = (uint32_t)taglib_audioproperties_samplerate(props);
        out_meta->channels = (uint32_t)taglib_audioproperties_channels(props);
        out_meta->bitrate = (uint32_t)taglib_audioproperties_bitrate(props);
    }

    taglib_file_free(file);

    if (!out_meta->title || !*out_meta->title || !out_meta->artist || !*out_meta->artist) {
        char *parsed_artist = NULL;
        char *parsed_title = NULL;
        music_metadata_parse_filename(out_meta->filename, &parsed_artist, &parsed_title);

        if ((!out_meta->title || !*out_meta->title) && parsed_title) {
            free(out_meta->title);
            out_meta->title = parsed_title;
            parsed_title = NULL;
        }
        if ((!out_meta->artist || !*out_meta->artist) && parsed_artist) {
            free(out_meta->artist);
            out_meta->artist = parsed_artist;
            parsed_artist = NULL;
        }

        free(parsed_artist);
        free(parsed_title);
    }

    if (!out_meta->title) out_meta->title = strdup_safe(METADATA_DEFAULT_TITLE);
    if (!out_meta->artist) out_meta->artist = strdup_safe(METADATA_DEFAULT_ARTIST);

    return true;
}

char *track_metadata_read_lyrics(const char *file_path)
{
    if (!file_path) return NULL;

    char lrc_path[1024];
    strncpy(lrc_path, file_path, sizeof(lrc_path) - 1);
    lrc_path[sizeof(lrc_path) - 1] = '\0';

    char *dot = strrchr(lrc_path, '.');
    if (dot && (size_t)(dot - lrc_path + 5) < sizeof(lrc_path)) {
        strcpy(dot, ".lrc");
    } else {
        strncat(lrc_path, ".lrc", sizeof(lrc_path) - strlen(lrc_path) - 1);
    }

    FILE *f = fopen(lrc_path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || size > 1024 * 1024) {
        fclose(f);
        return NULL;
    }

    char *buf = (char *)malloc(size + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    size_t read_bytes = fread(buf, 1, size, f);
    fclose(f);

    buf[read_bytes] = '\0';
    return buf;
}
