#ifndef APP_PATH_H
#define APP_PATH_H

#include <stddef.h>

#define APP_BUNDLE_ID "org.alvindimas05.fluyer"

void app_get_data_path(const char *filename, char *buf, size_t size);

#endif /* APP_PATH_H */
