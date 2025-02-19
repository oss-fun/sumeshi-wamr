typedef struct FileLog {
    int fd;
    int handle;
    char path[20];
    int open_flags;
    char permission[20];
    int offset;
} FileLog;

typedef struct FileLogNode {
    FileLog *filelog;
    struct FileLogNode *next;
} FileLogNode;

typedef struct {
    FileLogNode *head;
} FileLogList;

extern FileLogList file_log_list;

int
wasi_restore(WASMExecEnv *exec_env);

void
restore_openat_log(FileLogList *list);

int
is_fd_in_use(int fd);
int
find_unused_fd();
void
shift_fd(int fd);
