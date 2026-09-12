#include "path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
  #include <direct.h>
  #define mkdir_p(p) _mkdir(p)
#else
  #include <unistd.h>
  #define mkdir_p(p) mkdir(p, 0755)
#endif

void app_get_data_path(const char *filename, char *buf, size_t size)
{
    const char *home = getenv("HOME");
    if (!home) home = ".";

#ifdef __APPLE__
    snprintf(buf, size, "%s/Library/Application Support/%s", home, APP_BUNDLE_ID);
#elif defined(_WIN32)
    const char *appdata = getenv("APPDATA");
    if (appdata) snprintf(buf, size, "%s/%s", appdata, APP_BUNDLE_ID);
    else snprintf(buf, size, "%s/.%s", home, APP_BUNDLE_ID);
#else
    const char *xdg_data = getenv("XDG_DATA_HOME");
    if (xdg_data && xdg_data[0]) snprintf(buf, size, "%s/%s", xdg_data, APP_BUNDLE_ID);
    else snprintf(buf, size, "%s/.local/share/%s", home, APP_BUNDLE_ID);
#endif

    mkdir_p(buf);
    if (filename && filename[0]) {
        strncat(buf, "/", size - strlen(buf) - 1);
        strncat(buf, filename, size - strlen(buf) - 1);
    }
}
