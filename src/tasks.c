#include <aument.h>
#include <stdio.h>
#include <string.h>
#include <uv.h>

#include "tasks.h"
#include "timer.h"

_Thread_local struct tasks_ctx tasks_ctx;

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

struct au_program_data *au_module_load() {
    tasks_ctx = (struct tasks_ctx){0};
    struct au_program_data *data = au_module_new();
    au_module_add_fn(data, "set_interval", tasks_set_interval, 2);
    au_module_add_fn(data, "set_timeout", tasks_set_timeout, 2);
    au_module_add_fn(data, "run", tasks_run, 0);
    return data;
}