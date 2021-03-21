#include <aument.h>
#include <stdio.h>
#include <string.h>
#include <uv.h>

#include "tasks.h"

struct task_timer {
    struct task_header header;
    uv_timer_t timer;
};

static void task_timer_del(struct task_timer *timer) {
    struct au_fn_value *fn = (struct au_fn_value *)timer->timer.data;
    au_value_deref(au_value_fn(fn));
    timer->timer.data = 0;
}

static struct task_timer *task_timer_new(struct au_fn_value *fn) {
    struct task_timer *timer = au_data_malloc(sizeof(struct task_timer));

    timer->header.prev = 0;
    timer->header.next = tasks_ctx.task_list;
    if(tasks_ctx.task_list != 0)
        tasks_ctx.task_list->prev = (struct task_header *)timer;

    timer->header.del = (task_del_fn_t)task_timer_del;

    au_value_ref(au_value_fn(fn));
    timer->timer.data = fn;

    if (uv_timer_init(uv_default_loop(), &timer->timer) != 0) {
        au_data_free(timer);
        return 0;
    }

    return timer;
}

// * Callbacks *
static void tasks_internal_timer_cb(uv_timer_t *handle) {
    struct au_vm_thread_local *tl = au_vm_thread_local_get();
    struct au_fn_value *func = (struct au_fn_value *)handle->data;
    au_fn_value_call_vm(func, tl, 0, 0, 0);
}

// * Implementation *

AU_EXTERN_FUNC_DECL(tasks_set_interval) {
    au_value_t func_value = au_value_none();
    au_value_t ms_value = au_value_none();

    // Function argument
    func_value = _args[0];
    if(au_value_get_type(func_value) != AU_VALUE_FN)
        goto fail;
    struct au_fn_value *func = au_value_get_fn(func_value);

    // Interval argument
    ms_value = _args[1];
    if(au_value_get_type(ms_value) != AU_VALUE_INT)
        goto fail;
    const int32_t ms = au_value_get_int(ms_value);

    // Timer
    struct task_timer *timer = task_timer_new(func);
    uv_timer_start(&timer->timer, tasks_internal_timer_cb, 0, (uint64_t)ms);

    au_value_deref(func_value);
    au_value_deref(ms_value);
    return au_value_none();

fail:
    au_value_deref(func_value);
    au_value_deref(ms_value);
    return au_value_op_error();
}

AU_EXTERN_FUNC_DECL(tasks_set_timeout) {
    au_value_t func_value = au_value_none();
    au_value_t ms_value = au_value_none();

    // Function argument
    func_value = _args[0];
    if(au_value_get_type(func_value) != AU_VALUE_FN)
        goto fail;
    struct au_fn_value *func = au_value_get_fn(func_value);

    // Interval argument
    ms_value = _args[1];
    if(au_value_get_type(ms_value) != AU_VALUE_INT)
        goto fail;
    const int32_t ms = au_value_get_int(ms_value);

    // Timer
    struct task_timer *timer = task_timer_new(func);
    uv_timer_start(&timer->timer, tasks_internal_timer_cb, (uint64_t)ms, 0);

    au_value_deref(func_value);
    au_value_deref(ms_value);
    return au_value_none();

fail:
    au_value_deref(func_value);
    au_value_deref(ms_value);
    return au_value_op_error();
}
