#include "../libraries/libc-wasi/libc_wasi_wrapper.h"
#include "../libraries/libc-wasi/sandboxed-system-primitives/src/posix.h"
#include "wasm_runtime_common.h"
#include "wasi_dump.h"

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

FileLogList file_log_list;

void
debug_fd_table(struct fd_table *ft);

struct fd_table *
wasi_ctx_get_curfds(wasi_ctx_t wasi_ctx)
{
    if (!wasi_ctx)
        return NULL;
    return wasi_ctx->curfds;
}

int
wasi_dump(WASMExecEnv *exec_env)
{
    wasm_module_inst_t module_inst = get_module_inst(exec_env);
    if (module_inst == NULL) {
        printf("Error: module_inst is NULL\n");
        return -1;
    }
    wasi_ctx_t wasi_ctx = get_wasi_ctx(module_inst);
    if (wasi_ctx == NULL) {
        printf("Error: wasi_ctx is NULL\n");
        return -1;
    }
    struct fd_table *ft = wasi_ctx_get_curfds(wasi_ctx);
    if (ft == NULL) {
        printf("Error: Failed to get curfds\n");
        return -1;
    }

    FILE *file = fopen("fd_table.img", "wb");
    if (file == NULL) {
        perror("Failed to open file for writing");
        return -1;
    }

    // Save the fd_table metadata
    size_t wc = fwrite(&ft->size, sizeof(size_t), 1, file);
    if (wc != 1) {
        perror("Failed to write fd_table size");
        fclose(file);
        return -1;
    }
    wc = fwrite(&ft->used, sizeof(size_t), 1, file);
    // printf("dumped size: %ld\n", ft->size);
    // printf("dumped used: %ld\n", ft->used);
    if (wc != 1) {
        perror("Failed to write fd_table size");
        fclose(file);
        return -1;
    }

    wc = fwrite(&ft->lock.object, sizeof(korp_rwlock), 1, file);
    if (wc != 1) {
        perror("Failed to write fd_table size");
        fclose(file);
        return -1;
    }

    size_t index = 0;
    //  Save each fd_entry
    for (size_t i = 0; i < ft->size; i++) {
        struct fd_entry *entry = &ft->entries[i];
        // printf("count: %ld\n", i);
        if (entry->object == NULL) {
            // printf("continue\n");
            continue;
        }

        // エントリのインデックスを保存
        if (fwrite(&i, sizeof(size_t), 1, file) != 1) {
            perror("Failed to write fd_entry index");
            fclose(file);
            return -1;
        }

        //  Save rights
        wc = fwrite(&entry->rights_base, sizeof(__wasi_rights_t), 1, file);
        if (wc != 1) {
            perror("Failed to write fd_table size");
            fclose(file);
            return -1;
        }
        wc =
            fwrite(&entry->rights_inheriting, sizeof(__wasi_rights_t), 1, file);
        if (wc != 1) {
            perror("Failed to write fd_table size");
            fclose(file);
            return -1;
        }

        //  Save fd_object
        struct fd_object *obj = entry->object;
        wc = fwrite(&obj->refcount, sizeof(struct refcount), 1, file);
        if (wc != 1) {
            perror("Failed to write fd_table size");
            fclose(file);
            return -1;
        }
        wc = fwrite(&obj->type, sizeof(__wasi_filetype_t), 1, file);
        if (wc != 1) {
            perror("Failed to write fd_table size");
            fclose(file);
            return -1;
        }
        wc = fwrite(&obj->file_handle, sizeof(os_file_handle), 1, file);
        // printf("dumped handle: %d\n", obj->file_handle);
        if (wc != 1) {
            perror("Failed to write fd_table size");
            fclose(file);
            return -1;
        }
        wc = fwrite(&obj->is_stdio, sizeof(bool), 1, file);
        if (wc != 1) {
            perror("Failed to write fd_table size");
            fclose(file);
            return -1;
        }

        //  Save directory-specific data if applicable
        if (obj->type == __WASI_FILETYPE_DIRECTORY) {
            wc = fwrite(&obj->directory.lock, sizeof(struct mutex), 1, file);
            if (wc != 1) {
                perror("Failed to write fd_table size");
                fclose(file);
                return -1;
            }
            wc = fwrite(&obj->directory.handle, sizeof(os_dir_stream), 1, file);
            if (wc != 1) {
                perror("Failed to write fd_table size");
                fclose(file);
                return -1;
            }
            wc = fwrite(&obj->directory.offset, sizeof(__wasi_dircookie_t), 1,
                        file);
            if (wc != 1) {
                perror("Failed to write fd_table size");
                fclose(file);
                return -1;
            }
        }

        index++;
        if (index == ft->used) {
            break;
        }
    }
    // debug_fd_table(ft);
    fclose(file);

    return 0;
}

void
debug_fd_table(struct fd_table *ft)
{
    printf("size: %ld\n", ft->size);
    printf("used: %ld\n", ft->used);
    for (size_t fd_table_index = 0; fd_table_index < ft->size;
         fd_table_index++) {
        printf("------------------------------\n");
        struct fd_entry *fe = &ft->entries[fd_table_index];
        if (fe->object != NULL) {
            printf("fd_table_index: %ld\n", fd_table_index);
            printf("rights_base: %ld\n", fe->rights_base);
            printf("rights_inheriting: %ld\n", fe->rights_inheriting);
            printf("refcount: %d\n", fe->object->refcount.count);
            printf("type: %d\n", fe->object->type);
            printf("file_handle: %d\n", fe->object->file_handle);
            printf("is_stdio: %d\n", fe->object->is_stdio);
            if (fe->object->type == __WASI_FILETYPE_DIRECTORY) {
                printf("directory lock: %d\n", fe->object->directory.lock);
                printf("directory handle: %d\n", fe->object->directory.handle);
                printf("offset: %ld\n", fe->object->directory.offset);
            }
        }
        else if (fe->object == NULL) {
            printf("fd_table_index: %ld\n", fd_table_index);
            printf("fe->object is NULL\n");
        }
    }
}

/*
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
    fds[size++] = fd;
    int wc = fwrite(&log, sizeof(OpenatLog), 1, file);

    fclose(file);
}

void
dump_file_pointer()
{
    FILE *file = fopen("file_pointer.img", "ab");
    if (file == NULL) {
        return;
    }
    for (int i = 0; i < size; i++) {
        int offset = lseek(fds[i], 0, SEEK_CUR);
        // printf("dumped fd: %d, offset: %d\n", fds[i], offset);
        offsets[i] = offset;
    }
    size_t written = fwrite(fds, sizeof(int), size, file);
    if (written != size) {
        perror("Failed to write fds");
        fclose(file);
        return;
    }

    written = fwrite(offsets, sizeof(int), size, file);
    if (written != size) {
        perror("Failed to write offsets");
        fclose(file);
        return;
    }
    fclose(file);
}

void
insert_openat_log(int handle, const char *path, int open_flags, int permissions,
                  int fd)
{
    strcpy(logs[handle].func_name, "openat");
    logs[handle].handle = handle;
    strcpy(logs[handle].path, path);
    logs[handle].open_flags = open_flags;
    logs[handle].permissions = permissions;
    logs[handle].fd = fd;
    fds[size++] = fd;
}

void
delete_openat_log(int handle)
{
    // fdsの先頭から走査して、handleと一致するものを削除
    for (int i = 0; i < size; i++) {
        if (fds[i] == handle) {
            logs[i].func_name[0] = '\0';
            logs[i].handle = 0;
            logs[i].path[0] = '\0';
            logs[i].open_flags = 0;
            logs[i].permissions = 0;
            logs[i].fd = 0;
            size--;
            break;
        }
    }
}

*/

void
init_log_list(FileLogList *list)
{
    list->head = NULL;
}

void
insert_log_list(FileLogList *list, int fd, int handle, char *path,
                int open_flags, char *permission, int offset)
{
    FileLogNode *new_node = (FileLogNode *)malloc(sizeof(FileLogNode));
    if (!new_node) {
        printf("Failed to allocate memory for new node\n");
        perror("Failed to allocate memory for new node");
        return;
    }

    // `filelog` のメモリ確保
    new_node->filelog = (FileLog *)malloc(sizeof(FileLog));
    if (!new_node->filelog) {
        perror("メモリ確保失敗: new_node->filelog");
        free(new_node);
        return;
    }

    // NULL チェック
    if (permission == NULL) {
        printf("Error: permission is NULL\n");
        free(new_node->filelog);
        free(new_node);
        return;
    }

    // ログ情報のセット
    new_node->filelog->fd = fd;
    new_node->filelog->handle = handle;
    strcpy(new_node->filelog->path, path);
    new_node->filelog->open_flags = open_flags;
    strncpy(new_node->filelog->permission, permission,
            sizeof(new_node->filelog->permission) - 1);
    new_node->filelog->offset = offset;
    // 新しいノードをリストの先頭に追加
    new_node->next = list->head;
    list->head = new_node;
}

void
delete_log_list(FileLogList *list, int fd)
{
    FileLogNode *node = list->head;
    FileLogNode *prev = NULL;

    while (node) {
        if (node->filelog->fd == fd) {
            if (prev) {
                prev->next = node->next;
            }
            else {
                list->head = node->next;
            }
            free(node);
            return;
        }
        prev = node;
        node = node->next;
    }
}

void
get_file_pointer(FileLogNode *node)
{
    int offset = lseek(node->filelog->fd, 0, SEEK_CUR);
    node->filelog->offset = offset;
}

void
dump_log_list(const FileLogList *list)
{
    FILE *file = fopen("file.img", "w");
    if (!file) {
        perror("Failed to open file for writing");
        return;
    }

    FileLogNode *node = list->head;
    while (node) {
        get_file_pointer(node);
        // 数値データを先に保存
        fwrite(&node->filelog->fd, sizeof(int), 1, file);
        fwrite(&node->filelog->handle, sizeof(int), 1, file);
        fwrite(&node->filelog->open_flags, sizeof(int), 1, file);
        fwrite(&node->filelog->offset, sizeof(int), 1, file);

        // 文字列データを保存 (まず文字列の長さを書き込み、次にデータを書き込む)
        int path_len = strlen(node->filelog->path) + 1;
        int perm_len = strlen(node->filelog->permission) + 1;
        fwrite(&path_len, sizeof(int), 1, file);
        fwrite(node->filelog->path, sizeof(char), 20, file);
        fwrite(&perm_len, sizeof(int), 1, file);
        fwrite(node->filelog->permission, sizeof(char), 20, file);
        node = node->next;
    }

    fclose(file);
}