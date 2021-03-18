#include <aument.h>
#include <stdio.h>
#include <string.h>
#include <uv.h>

struct tasks_ctx {
    uv_timer_t timer;
};

static _Thread_local struct tasks_ctx ctx = {0};

// * Callbacks *
static void tasks_internal_timer_cb(uv_timer_t *handle) {
    struct au_vm_thread_local *tl = au_vm_thread_local_get();
    struct au_fn_value *func = (struct au_fn_value *)handle->data;
    au_fn_value_call_vm(func, tl, 0, 0, 0);
}

// * Implementation *

AU_EXTERN_FUNC_DECL(tasks_run) {
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    return au_value_none();
}

AU_EXTERN_FUNC_DECL(tasks_set_interval) {
    // Function argument
    au_value_t func_value = _args[0];
    if(au_value_get_type(func_value) != AU_VALUE_FN)
        return au_value_op_error();
    struct au_fn_value *func = au_value_get_fn(func_value);

    // Interval argument
    au_value_t ms_value = _args[1];
    if(au_value_get_type(ms_value) != AU_VALUE_INT)
        return au_value_op_error();
    int32_t ms = au_value_get_int(ms_value);

    fprintf(stderr, "Setting up timer\n");

    // Timer
    au_value_ref(func_value);
    ctx.timer.data = func;
    uv_timer_init(uv_default_loop(), &ctx.timer);
    uv_timer_start(&ctx.timer, tasks_internal_timer_cb, 0, (uint64_t)ms);

    return au_value_none();
}

struct au_program_data *au_module_load() {
    ctx = (struct tasks_ctx){0};
    struct au_program_data *data = au_module_new();
    au_module_add_fn(data, "set_interval", tasks_set_interval, 2);
    au_module_add_fn(data, "run", tasks_run, 0);
    return data;
}