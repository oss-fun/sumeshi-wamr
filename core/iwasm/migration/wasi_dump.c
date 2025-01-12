#include "wasi_dump.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

void
dump_openat_log(int handle, const char *path, int open_flags, int permissions,
                int fd)
{
    FILE *file = fopen("openat_log.img", "ab");
    if (file == NULL) {
        return;
    }
    OpenatLog log;
    strcpy(log.func_name, "openat");
    log.handle = handle;
    strcpy(log.path, path);
    log.open_flags = open_flags;
    log.permissions = permissions;
    log.fd = fd;
    fwrite(&log, sizeof(OpenatLog), 1, file);
    fclose(file);
}
