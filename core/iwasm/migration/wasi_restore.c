#include <stdio.h>

#include "../libraries/libc-wasi/libc_wasi_wrapper.h"
#include "../libraries/libc-wasi/sandboxed-system-primitives/src/posix.h"
#include "wasm_runtime_common.h"

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
    debug_fd_table(wasi_ctx->curfds);
    return 0;
}
