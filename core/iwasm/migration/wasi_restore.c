#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stddef.h>
#include <errno.h>

#include "../libraries/libc-wasi/libc_wasi_wrapper.h"
#include "../libraries/libc-wasi/sandboxed-system-primitives/src/posix.h"
#include "wasm_runtime_common.h"
#include "wasi_restore.h"

int
wasi_restore(WASMExecEnv *exec_env)
{
    wasm_module_inst_t module_inst = get_module_inst(exec_env);
    wasi_ctx_t wasi_ctx = get_wasi_ctx(module_inst);
    if (wasi_ctx == NULL) {
        return -1;
    }

    FILE *file = fopen("fd_table.img", "rb");
    if (!file) {
        perror("Failed to open file for reading");
        return -1;
    }

    struct fd_table *tf = malloc(sizeof(struct fd_table));
    if (!tf) {
        perror("Failed to allocate memory for fd_table");
        fclose(file);
        return -1;
    }

    // Read the fd_table metadata
    size_t size, used;
    if (fread(&size, sizeof(size_t), 1, file) != 1) {
        perror("Failed to read fd_table size");
        fclose(file);
        return -1;
    }
    if (fread(&used, sizeof(size_t), 1, file) != 1) {
        perror("Failed to read fd_table used");
        fclose(file);
        return -1;
    }
    if (fread(&tf->lock.object, sizeof(korp_rwlock), 1, file) != 1) {
        perror("Failed to read fd_table rwlock");
        fclose(file);
        return -1;
    }

    // Initialize fd_table
    tf->size = size;
    tf->used = used;
    tf->entries = calloc(size, sizeof(struct fd_entry));
    if (tf->entries == NULL) {
        perror("Failed to allocate fd_table entries");
        fclose(file);
        return -1;
    }

    size_t restored = 0;
    while (restored < used) {
        size_t index;

        // エントリのインデックスを読み取る
        if (fread(&index, sizeof(size_t), 1, file) != 1) {
            perror("Failed to read fd_entry index");
            fclose(file);
            return -1;
        }

        if (index >= tf->size) {
            printf("Error: Invalid fd_entry index %zu\n", index);
            fclose(file);
            return -1;
        }

        struct fd_entry *entry = &tf->entries[index];

        // `rights_base`と`rights_inheriting`を保存
        if (fread(&entry->rights_base, sizeof(__wasi_rights_t), 1, file) != 1) {
            perror("Failed to write rights_base");
            fclose(file);
            return -1;
        }
        if (fread(&entry->rights_inheriting, sizeof(__wasi_rights_t), 1, file)
            != 1) {
            perror("Failed to write rights_inheriting");
            fclose(file);
            return -1;
        }

        // 残りのデータを復元 (rights, fd_object, etc.)
        entry->object = malloc(sizeof(struct fd_object));
        if (entry->object == NULL) {
            perror("Failed to allocate fd_object");
            fclose(file);
            return -1;
        }

        struct fd_object *obj = entry->object;
        if (fread(&obj->refcount, sizeof(struct refcount), 1, file) != 1
            || fread(&obj->type, sizeof(__wasi_filetype_t), 1, file) != 1
            || fread(&obj->file_handle, sizeof(os_file_handle), 1, file) != 1
            || fread(&obj->is_stdio, sizeof(bool), 1, file) != 1) {
            perror("Failed to read fd_object data");
            free(obj);
            fclose(file);
            return -1;
        }

        // ディレクトリ固有データの復元
        if (obj->type == __WASI_FILETYPE_DIRECTORY) {
            if (fread(&obj->directory.lock, sizeof(struct mutex), 1, file) != 1
                || fread(&obj->directory.handle, sizeof(os_dir_stream), 1, file)
                       != 1
                || fread(&obj->directory.offset, sizeof(__wasi_dircookie_t), 1,
                         file)
                       != 1) {
                perror("Failed to read directory-specific data");
                free(obj);
                fclose(file);
                return -1;
            }
        }

        restored++;
    }

    fclose(file);
    wasi_ctx->curfds = tf;
    // debug_fd_table(wasi_ctx->curfds);
    return 0;
}

#define MAX_FD 1024 // 最大ファイルディスクリプタ数

void
restore_openat_log(FileLogList *list)
{
    // printf("restore_openat_log\n");
    FILE *file = fopen("openat_log.img", "rb");
    if (file == NULL) {
        return;
    }
    printf("Success to open file for reading\n");
    while (1) {
        FileLog *new_log = (FileLog *)malloc(sizeof(FileLog));
        if (!new_log) {
            perror("メモリ確保エラー");
            break;
        }
        printf("Success to allocate memory for new_log\n");

        // 数値データを読み込み
        if (fread(&new_log->fd, sizeof(int), 1, file) != 1) {
            free(new_log);
            break; // データが無い場合は終了
        }
        fread(&new_log->handle, sizeof(int), 1, file);
        fread(&new_log->open_flags, sizeof(int), 1, file);
        fread(&new_log->offset, sizeof(int), 1, file);
        printf("Success to read numeric data\n");
        // 文字列データを読み込み
        int path_len, perm_len;
        fread(&path_len, sizeof(int), 1, file);
        fread(new_log->path, sizeof(char), path_len, file);

        fread(&perm_len, sizeof(int), 1, file);
        fread(new_log->permission, sizeof(char), perm_len, file);
        printf("Success to read string data\n");
        // ノードを作成してリストに追加
        FileLogNode *new_node = (FileLogNode *)malloc(sizeof(FileLogNode));
        if (!new_node) {
            perror("メモリ確保エラー");
            free(new_log->path);
            free(new_log->permission);
            free(new_log);
            break;
        }

        printf("Success to allocate memory for new_node\n");

        new_node->filelog = new_log;
        new_node->next = list->head;
        list->head = new_node;
    }

    fclose(file);

    /*
    // データを確認
    for (size_t i = 0; i < count; i++) {
        // ファイルディスクリプタを復元
        fd = openat(logs[i].handle, logs[i].path, logs[i].open_flags,
                    logs[i].permissions);
        if (fd != logs[i].fd) {
            shift_fd(logs[i].fd);
            int newfd = fcntl(fd, F_DUPFD, logs[i].fd);
            if (newfd == -1) {
                printf("Failed to duplicate fd: %d\n", fd);
                continue;
            }
        }
    }
    */

    FileLogNode *node = list->head; // リストの先頭から開始

    while (node) {
        printf("openat");
        int fd = openat(node->filelog->handle, node->filelog->path,
                        node->filelog->open_flags,
                        node->filelog->permission); // permission を考慮

        if (fd != node->filelog->fd) {
            shift_fd(node->filelog->fd);
            int fd = fcntl(fd, F_DUPFD, node->filelog->fd);
            if (fd == -1) {
                printf("Failed to duplicate fd: %d\n", fd);
                node = node->next; // 次のノードへ
                return;
            }
        }
        printf("lseek");
        if (lseek(fd, node->filelog->offset, SEEK_SET) == -1) {
            perror("lseek error");
            node = node->next; // 次のノードへ
            return;
        }
        node = node->next; // 次のノードへ
    }

    free(node);
    free(list);
};

// 使用中のFDを確認する関数
int
is_fd_in_use(int fd)
{
    return fcntl(fd, F_GETFD) != -1 || errno != EBADF;
}

// 使用されていないFDを探す関数
int
find_unused_fd()
{
    for (int fd = 0; fd < MAX_FD; fd++) {
        if (!is_fd_in_use(fd)) {
            // printf("find_unused_fd: %d\n", fd);
            return fd;
        }
    }
    return -1; // 空きが見つからない場合
}

void
shift_fd(int fd)
{
    int unused_fd = find_unused_fd();
    if (unused_fd == -1) {
        return;
    }
    int newfd = fcntl(fd, F_DUPFD, unused_fd);
    if (newfd == -1) {
        printf("Failed to duplicate fd: %d\n", fd);
        return;
    }
    // printf("newfd: %d\n", newfd);
    // printf("shift_fd: %d -> %d\n", fd, unused_fd);
    close(fd);
}

/*
void
restore_file_pointer()
{
    // printf("restore_file_pointer\n");
    FILE *file = fopen("file_pointer.img", "rb");
    if (file == NULL) {
        return;
    }
    // 保存されているデータのサイズを推定する
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // データの個数を計算
    int size = file_size / (2 * sizeof(int));
    if (size <= 0) {
        printf("No data to restore.\n");
        fclose(file);
        return;
    }

    // 配列を確保
    int *fds = malloc(size * sizeof(int));
    int *offsets = malloc(size * sizeof(int));
    if (fds == NULL || offsets == NULL) {
        perror("malloc");
        fclose(file);
        return;
    }

    // データを読み取る
    if (fread(fds, sizeof(int), size, file) == 0) {
        perror("Failed to read fds");
        fclose(file);
        return;
    }
    if (fread(offsets, sizeof(int), size, file) == 0) {
        perror("Failed to read offsets");
        fclose(file);
        return;
    }
    fclose(file);

    // ファイルディスクリプタとオフセットを復元
    for (int i = 0; i < size; i++) {
        int fd = fds[i];
        int offset = offsets[i];

        // ファイルディスクリプタを `lseek` で設定
        if (lseek(fd, offset, SEEK_SET) == -1) {
            perror("lseek error");
            continue;
        }

        // printf("Restored fd: %d, offset: %d\n", fd, offset);

        // 必要に応じてファイルポインタを操作
        // fclose(fp); // ファイルポインタを閉じる場合に実行
    }

    // メモリを解放
    free(fds);
    free(offsets);
}
*/