typedef struct {
    char func_name[10];
    int handle;
    char path[20];
    int open_flags;
    int permissions;
    int fd;
} OpenatLog;

void
restore_openat_log();

int
is_fd_in_use(int fd);
int
find_unused_fd();
void
shift_fd(int fd);