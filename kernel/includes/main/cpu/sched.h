#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>

#define STACK_SIZE 8192
#define MAX_TASKS  16
#define TIME_SLICE 1000

typedef struct task {
    uint64_t rsp;
    uint64_t stack_base;
    uint32_t id;
    struct task *next;
} task_t;

void sched_init(void);
void sched_start(void);
int  sched_spawn(void (*entry)(void));
void sched_yield(void);

#endif
