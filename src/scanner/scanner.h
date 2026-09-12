#ifndef SCANNER_H
#define SCANNER_H

#include <stddef.h>
#include <sqlite3.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t scanner_scan_and_save(sqlite3 *db, const char *dir_path);

#ifdef __cplusplus
}
#endif

#endif /* SCANNER_H */
