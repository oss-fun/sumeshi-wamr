#include "wasm_runtime_common.h"

typedef struct FileLog {
    int fd;
    int handle;
    char path[50];
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

void
init_log_list(FileLogList *list);
void
insert_log_list(FileLogList *list, int fd, int handle, char *path,
                int open_flags, char *permission, int offset);
void
delete_log_list(FileLogList *list, int fd);
void
get_log_list(FileLogList *list);

extern FileLogList file_log_list;
void
init_log_list(FileLogList *list);

int
wasi_dump(WASMExecEnv *exec_env);

void
dump_log_list(const FileLogList *list);