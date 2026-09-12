#include "db.h"
#include "../utils/path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

sqlite3 *g_db = NULL;

static const char *MIGRATIONS[] = {
    // Migration 1
    "CREATE TABLE IF NOT EXISTS musics ("
    "    id INTEGER PRIMARY KEY,"
    "    path TEXT NOT NULL,"
    "    duration INTEGER,"
    "    title TEXT,"
    "    artist TEXT,"
    "    album TEXT,"
    "    album_artist TEXT,"
    "    track_number VARCHAR(10),"
    "    genre TEXT,"
    "    date TEXT,"
    "    bits_per_sample INTEGER,"
    "    sample_rate INTEGER,"
    "    modified_at TEXT NOT NULL"
    ");",

    // Migration 2
    "CREATE TABLE IF NOT EXISTS playlists ("
    "    id INTEGER PRIMARY KEY,"
    "    name TEXT NOT NULL,"
    "    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
    "    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP"
    ");"
    "CREATE TRIGGER IF NOT EXISTS playlists_update_timestamp "
    "AFTER UPDATE ON playlists "
    "BEGIN "
    "    UPDATE playlists SET updated_at = CURRENT_TIMESTAMP WHERE id = NEW.id; "
    "END;"
    "CREATE TABLE IF NOT EXISTS playlist_musics ("
    "    id INTEGER PRIMARY KEY,"
    "    playlist_id INTEGER NOT NULL,"
    "    music_id INTEGER NOT NULL,"
    "    position INTEGER NOT NULL,"
    "    FOREIGN KEY (playlist_id) REFERENCES playlists(id) ON DELETE CASCADE,"
    "    FOREIGN KEY (music_id) REFERENCES musics(id) ON DELETE CASCADE"
    ");",

    // Migration 3
    "DROP TABLE IF EXISTS playlist_musics;"
    "DROP TABLE IF EXISTS playlists;"
    "DROP TRIGGER IF EXISTS playlists_update_timestamp;"
    "CREATE TABLE playlists ("
    "    id INTEGER PRIMARY KEY,"
    "    name TEXT NOT NULL,"
    "    image TEXT,"
    "    title TEXT,"
    "    artist TEXT,"
    "    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
    "    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP"
    ");"
    "CREATE TRIGGER playlists_update_timestamp "
    "AFTER UPDATE ON playlists "
    "BEGIN "
    "    UPDATE playlists SET updated_at = CURRENT_TIMESTAMP WHERE id = NEW.id; "
    "END;"
    "CREATE TABLE playlist_musics ("
    "    id INTEGER PRIMARY KEY,"
    "    playlist_id INTEGER NOT NULL,"
    "    path TEXT NOT NULL,"
    "    position INTEGER NOT NULL,"
    "    FOREIGN KEY (playlist_id) REFERENCES playlists(id) ON DELETE CASCADE"
    ");"
};

#define MIGRATIONS_COUNT (sizeof(MIGRATIONS) / sizeof(MIGRATIONS[0]))

static bool run_migrations(sqlite3 *db)
{
    char *err_msg = NULL;
    int rc = sqlite3_exec(db,
        "CREATE TABLE IF NOT EXISTS schema_migrations (version INTEGER PRIMARY KEY);",
        NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        sqlite3_free(err_msg);
        return false;
    }

    int current_version = 0;
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, "SELECT MAX(version) FROM schema_migrations;", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            current_version = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    for (size_t i = current_version; i < MIGRATIONS_COUNT; i++) {
        rc = sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
        if (rc != SQLITE_OK) return false;

        rc = sqlite3_exec(db, MIGRATIONS[i], NULL, NULL, &err_msg);
        if (rc != SQLITE_OK) {
            sqlite3_free(err_msg);
            sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
            return false;
        }

        char record_sql[64];
        snprintf(record_sql, sizeof(record_sql), "INSERT INTO schema_migrations (version) VALUES (%zu);", i + 1);
        rc = sqlite3_exec(db, record_sql, NULL, NULL, &err_msg);
        if (rc != SQLITE_OK) {
            sqlite3_free(err_msg);
            sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
            return false;
        }

        sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    }

    return true;
}

bool db_init(const char *custom_path)
{
    char path[1024];
    if (custom_path) {
        strncpy(path, custom_path, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
    } else {
        app_get_data_path(DB_NAME, path, sizeof(path));
    }

    int rc = sqlite3_open(path, &g_db);
    if (rc != SQLITE_OK) {
        if (g_db) {
            sqlite3_close(g_db);
            g_db = NULL;
        }
        return false;
    }

    sqlite3_exec(g_db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);

    if (!run_migrations(g_db)) {
        sqlite3_close(g_db);
        g_db = NULL;
        return false;
    }

    return true;
}

void db_close(void)
{
    if (g_db) {
        sqlite3_close(g_db);
        g_db = NULL;
    }
}
