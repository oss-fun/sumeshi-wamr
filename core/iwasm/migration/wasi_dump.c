#include "../libraries/libc-wasi/libc_wasi_wrapper.h"
#include "../libraries/libc-wasi/sandboxed-system-primitives/src/posix.h"
#include "wasm_runtime_common.h"

#include <stdio.h>
#include <stdlib.h>

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
    debug_fd_table(ft);
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