#pragma once
#include <aument.h>

struct task_header;
struct tasks_ctx {
    struct task_header *task_list;
};

typedef void (*task_del_fn_t)(struct task_header *);

struct task_header {
    struct task_header *prev;
    struct task_header *next;
    task_del_fn_t del;
};

extern _Thread_local struct tasks_ctx tasks_ctx;