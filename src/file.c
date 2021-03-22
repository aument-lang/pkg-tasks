#include <aument.h>
#include <stdio.h>
#include <string.h>
#include <uv.h>

#include "file.h"
#include "tasks.h"

#define MAX_SMALL_PATH 256

static void on_open(uv_fs_t *);
static void on_close(uv_fs_t *);

struct tasks_file;

// * Tasks *
// ** Reading **
struct file_read_task {
    struct task_header header;
    uv_fs_t req;
    struct tasks_file *file;
    char *buffer;
    size_t bufsize;
};

static void file_read_task_del(struct file_read_task *task) {
}

static void file_read_task_on_read(uv_fs_t *read_req) {
    struct file_read_task *task = read_req->data;
    fprintf(stderr, "read! %.*s\n", (int)read_req->result, task->buffer);
}

static struct file_read_task *file_read_task_new(
    struct tasks_file *file, size_t bufsize) {
    struct file_read_task *task = au_data_malloc(sizeof(struct file_read_task));
    task->header.prev = 0;
    task->header.next = 0;
    task->header.del = (task_del_fn_t)file_read_task_del;

    au_value_ref(au_value_struct((struct au_struct*)file));
    task->file = file;
    task->req.data = task;

    if(bufsize > 0) {
        task->buffer = au_data_calloc(bufsize, 1);
        task->bufsize = bufsize;
    } else {
        task->buffer = 0;
        task->bufsize = 0;
    }

    tasks_ctx_append(&task->header);
    return task;
}

// * Operations *
enum file_operation_type {
    FILE_OP_READ,
};

struct file_operation {
    struct au_fn_value *cb;
    enum file_operation_type type;
    union {
        struct {
            int32_t max_size;
        } read;
    } data;
};

AU_ARRAY_STRUCT(struct file_operation, file_operation_array, 1)

// * File *

enum tasks_file_status {
    TASK_FILE_OPENING,
    TASK_FILE_OPENED,
    TASK_FILE_CLOSED,
};

struct tasks_file {
    struct au_struct header;
    enum tasks_file_status status;
    union {
        uv_fs_t open_req;
        uv_file file;
    } data;
    struct file_operation_array ops;
};

static void file_close(struct tasks_file *file) {
    switch(file->status) {
    case TASK_FILE_OPENING: {
        if(file->status == TASK_FILE_OPENING) {
            uv_fs_req_cleanup(&file->data.open_req);
            file->data.open_req = (uv_fs_t){0};
        }
        au_data_free(file->ops.data);
        file->ops.len = 0;
        file->status = TASK_FILE_CLOSED;
        break;
    }
    case TASK_FILE_OPENED: {
        file->status = TASK_FILE_CLOSED;
        break;
    }
    case TASK_FILE_CLOSED: {
        break;
    }
    }
}

// * Queueing *

static void file_operation_process(
    const struct file_operation *op, struct tasks_file *file) {
    switch(op->type) {
    case FILE_OP_READ: {
        struct file_read_task *task = file_read_task_new(
            file,
            op->data.read.max_size
        );
        uv_buf_t iov = uv_buf_init(task->buffer, task->bufsize);
        uv_fs_read(uv_default_loop(), &task->req,
            file->data.file, &iov, 1, -1, file_read_task_on_read);
        break;
    }
    }
}

// * Events *

static void on_open(uv_fs_t *open_req) {
    struct tasks_file *file = (struct tasks_file *)open_req->data;
    if(open_req->result <= 0) {
        file_close(file);
        return;
    }

    switch(file->status) {
    case TASK_FILE_OPENING: {
        const uv_file _libuv_file = (uv_file)open_req->result;
        uv_fs_req_cleanup(&file->data.open_req);
        file->data.open_req = (uv_fs_t){0};
        file->data.file = _libuv_file;
        file->status = TASK_FILE_OPENED;
        // fallthrough
    }
    case TASK_FILE_OPENED: {
        if(file->ops.len > 0) {
            for(size_t i = 0; i < file->ops.len; i++) {
                file_operation_process(&file->ops.data[i], file);
            }
            au_data_free(file->ops.data);
            file->ops.data = 0;
            file->ops.len = 0;
        }
        break;
    }
    case TASK_FILE_CLOSED: {
        break;
    }
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

    file->data.open_req = (uv_fs_t){0};
    file->data.open_req.data = file;
    file->status = TASK_FILE_OPENING;
    file->ops = (struct file_operation_array){0};

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
        fd = uv_fs_open(uv_default_loop(), &file->data.open_req, path_param, flags, mode, on_open);
    } else {
        char *path_param = au_data_malloc(path_str->len + 1);
        memcpy(path_param, path_str->data, path_str->len);
        path_param[path_str->len] = 0;
        fd = uv_fs_open(uv_default_loop(), &file->data.open_req, path_param, flags, mode, on_open);
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
    func_value = _args[2];
    if(au_value_get_type(func_value) != AU_VALUE_FN)
        goto fail;
    struct au_fn_value *func = au_value_get_fn(func_value);

    if(file->status == TASK_FILE_OPENING) {
        au_value_ref(func_value);
        struct file_operation op = {
            .type = FILE_OP_READ,
            .cb = func,
            .data.read.max_size = n_bytes,
        };
        file_operation_array_add(&file->ops, op);
    } else {
        abort(); // TODO
    }

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
