#include <aument.h>
#include <stdio.h>
#include <string.h>
#include <uv.h>

#include "file.h"

#define MAX_SMALL_PATH 256

struct tasks_file {
    struct au_struct header;
    uv_fs_t req;
};

static void file_close(struct tasks_file *file) {
    // TODO
    (void)file;
}

/* Events */

static void on_open(uv_fs_t *req) {
    struct tasks_file *file = (struct tasks_file *)req->data;
    fprintf(stderr, "opened file %s\n", file->req.path);
}

/* Objects & functions */

struct _Thread_local au_struct_vdata file_vdata;
static _Thread_local int file_vdata_inited = 0;
static void file_vdata_init() {
    if (!file_vdata_inited) {
        file_vdata = (struct au_struct_vdata){
            .del_fn = (au_obj_del_fn_t)file_close,
            .idx_get_fn = 0,
            .idx_set_fn = 0,
            .len_fn = 0,
        };
        file_vdata_inited = 1;
    }
}

AU_EXTERN_FUNC_DECL(tasks_file_open) {
    au_value_t path_val = au_value_none();
    au_value_t mode_val = au_value_none();

    path_val = _args[0];
    if (au_value_get_type(path_val) != AU_VALUE_STR)
        goto fail;
    const struct au_string *path_str = au_value_get_string(path_val);

    mode_val = _args[1];
    if (au_value_get_type(mode_val) != AU_VALUE_STR)
        goto fail;
    const struct au_string *mode_str = au_value_get_string(mode_val);

    int flags = 0;
    int mode = 0;
    if (au_string_cmp_cstr(mode_str, "r") == 0)
        flags |= UV_FS_O_RDONLY;
    else if (au_string_cmp_cstr(mode_str, "w") == 0)
        flags |= UV_FS_O_WRONLY;
    else if (au_string_cmp_cstr(mode_str, "a") == 0)
        flags |= UV_FS_O_APPEND;
    else
        goto fail;

    struct tasks_file *file = 0;
    int fd = -1;
    uv_fs_t req;

    // Set up path parameter & open the file
    if (path_str->len < MAX_SMALL_PATH) {
        // Path parameter will usually be smaller than or equal to this
        // size. This is an optimization to reduce heap allocation
        char path_param[MAX_SMALL_PATH] = {0};
        memcpy(path_param, path_str->data, path_str->len);
        path_param[path_str->len] = 0;
        fd = uv_fs_open(uv_default_loop(), &req, path_param, flags, mode, on_open);
    } else {
        char *path_param = au_data_malloc(path_str->len + 1);
        memcpy(path_param, path_str->data, path_str->len);
        path_param[path_str->len] = 0;
        fd = uv_fs_open(uv_default_loop(), &req, path_param, flags, mode, on_open);
        au_data_free(path_param);
    }

    if(fd < 0)
        goto fail;

    file =
        au_obj_malloc(sizeof(struct tasks_file), (au_obj_del_fn_t)file_close);
    file_vdata_init();
    file->header = (struct au_struct){
        .rc = 1,
        .vdata = &file_vdata,
    };

    req.data = file;
    memcpy(&file->req, &req, sizeof(uv_fs_t));

    fd = -1;

    au_value_deref(path_val);
    au_value_deref(mode_val);
    return au_value_struct((struct au_struct *)file);

fail:
    au_value_deref(path_val);
    au_value_deref(mode_val);
    if(fd >= 0)
        uv_fs_req_cleanup(&req);
    if(file != 0)
        au_obj_free(file);
    return au_value_none();
}

AU_EXTERN_FUNC_DECL(tasks_file_close) {
    const au_value_t file_val = _args[0];
    struct au_struct *file_struct = au_struct_coerce(file_val);
    if (file_struct == NULL || file_struct->vdata != &file_vdata) {
        au_value_deref(file_val);
        return au_value_op_error();
    }
    file_close((struct tasks_file *)file_struct);
    au_value_deref(file_val);
    return au_value_none();
}

AU_EXTERN_FUNC_DECL(tasks_file_read) {}

AU_EXTERN_FUNC_DECL(tasks_file_write) {}
