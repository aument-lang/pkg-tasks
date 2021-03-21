#include <aument.h>
#include <stdio.h>
#include <string.h>
#include <uv.h>

#include "tasks.h"
#include "timer.h"
#include "file.h"

_Thread_local struct tasks_ctx tasks_ctx;
static _Thread_local int tasks_ctx_init = 0;

void tasks_ctx_append(struct task_header *header) {
    header->prev = 0;
    header->next = tasks_ctx.task_list;
    if(tasks_ctx.task_list != 0)
        tasks_ctx.task_list->prev = header;
}


static void tasks_ctx_del(struct tasks_ctx *ctx) {
    struct task_header *cur = ctx->task_list;
    while(cur != 0) {
        struct task_header *next = cur->next;
        cur->del(cur);
        au_data_free(cur);
        cur = next;
    }
    *ctx = (struct tasks_ctx){0};
}

AU_EXTERN_FUNC_DECL(tasks_run) {
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    tasks_ctx_del(&tasks_ctx);
    return au_value_none();
}

au_extern_module_t au_extern_module_load(struct au_extern_module_options *options) {
    if(!tasks_ctx_init) {
        tasks_ctx = (struct tasks_ctx){0};
        tasks_ctx_init = 1;
    }
    if(options->subpath == 0) {
        au_extern_module_t data = au_extern_module_new();
        // * timer.c *
        au_extern_module_add_fn(data, "set_interval", tasks_set_interval, 2);
        au_extern_module_add_fn(data, "set_timeout", tasks_set_timeout, 2);
        au_extern_module_add_fn(data, "run", tasks_run, 0);
        return data;
    } else if(strcmp(options->subpath, "file") == 0) {
        au_extern_module_t data = au_extern_module_new();
        // * file.c *
        au_extern_module_add_fn(data, "open", tasks_file_open, 2);
        au_extern_module_add_fn(data, "close", tasks_file_close, 1);
        au_extern_module_add_fn(data, "read", tasks_file_read, 1);
        au_extern_module_add_fn(data, "write", tasks_file_write, 1);
        return data;
    } else {
        return 0;
    }
}