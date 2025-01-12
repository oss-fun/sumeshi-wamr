typedef struct {
    char func_name[10];
    int handle;
    char path[20];
    int open_flags;
    int permissions;
    int fd;
} OpenatLog;

void
dump_openat_log(int handle, const char *path, int open_flags, int permissions,
                int fd);