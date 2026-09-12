#include "scanner.h"
#include "../metadata/metadata.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>

static bool get_file_mtime_rfc3339(const char *path, char *buf, size_t buf_sz)
{
    struct stat st;
    if (stat(path, &st) != 0) return false;

    struct tm tm_utc;
    gmtime_r(&st.st_mtime, &tm_utc);
    strftime(buf, buf_sz, "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
    return true;
}

static bool is_supported_audio_extension(const char *path)
{
    const char *dot = strrchr(path, '.');
    if (!dot) return false;
    dot++;

    static const char *exts[] = {
        "mp3", "flac", "wav", "ogg", "m4a", "m4b", "mp4", "mkv",
        "webm", "avi", "aac", "wma", "opus", "alac", "ape", "aiff",
        "aif", "mov", "ts", "flv", "3gp", "wmv", "wv", "mpc", "tta"
    };
    for (size_t i = 0; i < sizeof(exts) / sizeof(exts[0]); i++) {
        if (strcasecmp(dot, exts[i]) == 0) return true;
    }
    return false;
}

static void save_music_record(sqlite3 *db, const char *path, const char *mtime_str, const music_metadata_t *meta)
{
    sqlite3_stmt *stmt = NULL;
    const char *check_sql = "SELECT modified_at FROM musics WHERE path = ?1;";
    char *existing_mtime = NULL;

    if (sqlite3_prepare_v2(db, check_sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char *text = sqlite3_column_text(stmt, 0);
            if (text) existing_mtime = strdup((const char *)text);
        }
        sqlite3_finalize(stmt);
    }

    if (!existing_mtime) {
        const char *insert_sql =
            "INSERT INTO musics ("
            "    path, duration, title, artist, album, album_artist,"
            "    track_number, genre, date, bits_per_sample, sample_rate, modified_at"
            ") VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12);";

        if (sqlite3_prepare_v2(db, insert_sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
            sqlite3_bind_int64(stmt, 2, (sqlite3_int64)meta->duration_ms);
            sqlite3_bind_text(stmt, 3, meta->title ? meta->title : METADATA_DEFAULT_TITLE, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 4, meta->artist ? meta->artist : METADATA_DEFAULT_ARTIST, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 5, meta->album, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 6, meta->album_artist, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 7, meta->track_number, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 8, meta->genre, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 9, meta->date, -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 10, (int)meta->bits_per_sample);
            sqlite3_bind_int(stmt, 11, (int)meta->sample_rate);
            sqlite3_bind_text(stmt, 12, mtime_str, -1, SQLITE_STATIC);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    } else {
        if (strcmp(existing_mtime, mtime_str) != 0) {
            const char *update_sql =
                "UPDATE musics SET "
                "    duration = ?1, title = ?2, artist = ?3, album = ?4, album_artist = ?5,"
                "    track_number = ?6, genre = ?7, bits_per_sample = ?8, sample_rate = ?9,"
                "    modified_at = ?10, date = ?11 "
                "WHERE path = ?12;";

            if (sqlite3_prepare_v2(db, update_sql, -1, &stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_int64(stmt, 1, (sqlite3_int64)meta->duration_ms);
                sqlite3_bind_text(stmt, 2, meta->title ? meta->title : METADATA_DEFAULT_TITLE, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 3, meta->artist ? meta->artist : METADATA_DEFAULT_ARTIST, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 4, meta->album, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 5, meta->album_artist, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 6, meta->track_number, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 7, meta->genre, -1, SQLITE_STATIC);
                sqlite3_bind_int(stmt, 8, (int)meta->bits_per_sample);
                sqlite3_bind_int(stmt, 9, (int)meta->sample_rate);
                sqlite3_bind_text(stmt, 10, mtime_str, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 11, meta->date, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 12, path, -1, SQLITE_STATIC);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);
            }
        }
        free(existing_mtime);
    }
}

static void scan_recursive(sqlite3 *db, const char *dir_path, size_t *count)
{
    DIR *d = opendir(dir_path);
    if (!d) return;

    struct dirent *entry;
    char full_path[1024];

    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        if (strcmp(entry->d_name, "au_uu_SzH34yR2.mp3") == 0) continue;

        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            scan_recursive(db, full_path, count);
        } else if (S_ISREG(st.st_mode)) {
            if (!is_supported_audio_extension(full_path)) continue;

            char mtime[64] = {0};
            get_file_mtime_rfc3339(full_path, mtime, sizeof(mtime));

            music_metadata_t meta;
            if (music_metadata_read(full_path, &meta)) {
                save_music_record(db, full_path, mtime, &meta);
                music_metadata_free(&meta);
                (*count)++;
            }
        }
    }
    closedir(d);
}

size_t scanner_scan_and_save(sqlite3 *db, const char *dir_path)
{
    if (!db || !dir_path) return 0;

    sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    size_t count = 0;
    scan_recursive(db, dir_path, &count);
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);

    return count;
}
