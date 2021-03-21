#include <aument.h>
#include <stdio.h>
#include <string.h>
#include <uv.h>

#include "file.h"

#define MAX_SMALL_PATH 256

// * Task *

// * File *

struct tasks_file {
    struct au_struct header;
    uv_fs_t open_req;
    int is_open_req_free;
};

static void file_close(struct tasks_file *file) {
    if(!file->is_open_req_free) {
        file->is_open_req_free = 1;
        uv_fs_req_cleanup(&file->open_req);
        file->open_req = (uv_fs_t){0};
    }
}

// * Events *

static void on_open(uv_fs_t *open_req) {
    struct tasks_file *file = (struct tasks_file *)open_req->data;
    if(!file->is_open_req_free) {
        file->is_open_req_free = 1;
        uv_fs_req_cleanup(open_req);
        *open_req = (uv_fs_t){0};
    }
}

// * Objects & functions *

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

    struct tasks_file *file = 
        au_obj_malloc(sizeof(struct tasks_file), (au_obj_del_fn_t)file_close);
    file_vdata_init();
    file->header = (struct au_struct){
        .rc = 1,
        .vdata = &file_vdata,
    };

    file->open_req = (uv_fs_t){0};
    file->open_req.data = file;
    file->is_open_req_free = 0;

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

    int fd = -1;

    // Set up path parameter & open the file
    if (path_str->len < MAX_SMALL_PATH) {
        // Path parameter will usually be smaller than or equal to this
        // size. This is an optimization to reduce heap allocation
        char path_param[MAX_SMALL_PATH] = {0};
        memcpy(path_param, path_str->data, path_str->len);
        path_param[path_str->len] = 0;
        fd = uv_fs_open(uv_default_loop(), &file->open_req, path_param, flags, mode, on_open);
    } else {
        char *path_param = au_data_malloc(path_str->len + 1);
        memcpy(path_param, path_str->data, path_str->len);
        path_param[path_str->len] = 0;
        fd = uv_fs_open(uv_default_loop(), &file->open_req, path_param, flags, mode, on_open);
        au_data_free(path_param);
    }

    if(fd < 0)
        goto fail;

    fd = -1;

    au_value_deref(path_val);
    au_value_deref(mode_val);
    return au_value_struct((struct au_struct *)file);

fail:
    au_value_deref(path_val);
    au_value_deref(mode_val);
    au_obj_free(file);
    return au_value_none();
}

AU_EXTERN_FUNC_DECL(tasks_file_close) {
    const au_value_t file_value = _args[0];
    struct au_struct *file_struct = au_struct_coerce(file_value);
    if (file_struct == NULL || file_struct->vdata != &file_vdata) {
        au_value_deref(file_value);
        return au_value_op_error();
    }
    file_close((struct tasks_file *)file_struct);
    au_value_deref(file_value);
    return au_value_none();
}

AU_EXTERN_FUNC_DECL(tasks_file_read) {
    au_value_t file_value = au_value_none();
    au_value_t func_value = au_value_none();

    // File argument
    file_value = _args[0];
    struct au_struct *file_struct = au_struct_coerce(file_value);
    if (file_struct == NULL || file_struct->vdata != &file_vdata)
        goto fail;
    struct tasks_file *file = (struct tasks_file *)file_struct;

    // Function argument
    func_value = _args[1];
    if(au_value_get_type(func_value) != AU_VALUE_FN)
        goto fail;
    struct au_fn_value *func = au_value_get_fn(func_value);

    // TODO

    au_value_deref(file_value);
    au_value_deref(func_value);
    return au_value_none();

fail:
    au_value_deref(file_value);
    au_value_deref(func_value);
    return au_value_op_error();
}

AU_EXTERN_FUNC_DECL(tasks_file_read_up_to) {
    au_value_t file_value = au_value_none();
    au_value_t n_value = au_value_none();
    au_value_t func_value = au_value_none();

    // File argument
    file_value = _args[0];
    struct au_struct *file_struct = au_struct_coerce(file_value);
    if (file_struct == NULL || file_struct->vdata != &file_vdata)
        goto fail;
    struct tasks_file *file = (struct tasks_file *)file_struct;

    // number of bytes argument
    n_value = _args[1];
    if(au_value_get_type(n_value) != AU_VALUE_INT)
        goto fail;
    const int32_t n_bytes = au_value_get_int(n_value);

    // Function argument
    func_value = _args[1];
    if(au_value_get_type(func_value) != AU_VALUE_FN)
        goto fail;
    struct au_fn_value *func = au_value_get_fn(func_value);

    // TODO

    au_value_deref(file_value);
    au_value_deref(n_value);
    au_value_deref(func_value);
    return au_value_none();

fail:
    au_value_deref(file_value);
    au_value_deref(n_value);
    au_value_deref(func_value);
    return au_value_op_error();
}

AU_EXTERN_FUNC_DECL(tasks_file_write) {
    au_value_t file_value = au_value_none();
    au_value_t out_value = au_value_none();
    au_value_t func_value = au_value_none();

    // File argument
    file_value = _args[0];
    struct au_struct *file_struct = au_struct_coerce(file_value);
    if (file_struct == NULL || file_struct->vdata != &file_vdata)
        goto fail;
    struct tasks_file *file = (struct tasks_file *)file_struct;

    // Output argument
    out_value = _args[1];
    if(au_value_get_type(out_value) != AU_VALUE_STR)
        goto fail;
    const struct au_string *out = au_value_get_string(out_value);

    // Function argument
    func_value = _args[2];
    if(au_value_get_type(func_value) != AU_VALUE_FN)
        goto fail;
    struct au_fn_value *func = au_value_get_fn(func_value);

    // TODO

    au_value_deref(file_value);
    au_value_deref(out_value);
    au_value_deref(func_value);
    return au_value_none();

fail:
    au_value_deref(file_value);
    au_value_deref(out_value);
    au_value_deref(func_value);
    return au_value_op_error();
}
