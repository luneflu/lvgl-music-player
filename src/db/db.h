#ifndef DB_H
#define DB_H

#include <sqlite3.h>
#include <stdbool.h>

#define DB_NAME "fluyer.db"

extern sqlite3 *g_db;

bool db_init(const char *custom_path);
void db_close(void);

#endif /* DB_H */
