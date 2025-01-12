#include "wasi_restore.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stddef.h>
#include <errno.h>

#define MAX_FD 1024 // 最大ファイルディスクリプタ数

void
restore_openat_log()
{
    printf("restore_openat_log\n");
    FILE *file = fopen("openat_log.img", "rb");
    int fd;
    if (file == NULL) {
        return;
    }

    OpenatLog *logs = NULL; // 　OpenatLogの配列
    size_t count = 0;       // ログの数
    OpenatLog log;          // ログを読み込むための変数

    while (fread(&log, sizeof(OpenatLog), 1, file) == 1) {
        // 配列を1つ拡張
        OpenatLog *new_logs = realloc(logs, (count + 1) * sizeof(OpenatLog));
        if (new_logs == NULL) {
            perror("Failed to reallocate memory");
            free(logs); // 既存のメモリを解放
            fclose(file);
            return;
        }

        logs = new_logs;     // 新しい配列を反映
        logs[count++] = log; // 新しい要素を追加
    }
    fclose(file);

    // データを確認
    printf("Read %zu logs from the file:\n", count);
    for (size_t i = 0; i < count; i++) {
        printf("Log %zu: %s %d %s %d %d %d\n", i + 1, logs[i].func_name,
               logs[i].handle, logs[i].path, logs[i].open_flags,
               logs[i].permissions, logs[i].fd);

        // ファイルディスクリプタを復元
        fd = openat(logs[i].handle, logs[i].path, logs[i].open_flags,
                    logs[i].permissions);
        printf("oldfd: %d\n", fd);
        if (fd != logs[i].fd) {
            shift_fd(logs[i].fd);
            int newfd = fcntl(fd, F_DUPFD, logs[i].fd);
            if (newfd == -1) {
                printf("Failed to duplicate fd: %d\n", fd);
                continue;
            }
            printf("newfd: %d\n", newfd);
        }
        printf("newfd: %d\n", fd);
        printf("getfd: %d\n", fcntl(fd, F_GETFD));
    }

    // メモリを解放
    free(logs);
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
    // printf("newfd: %d\n", newfd);
    // printf("shift_fd: %d -> %d\n", fd, unused_fd);
    close(fd);
}