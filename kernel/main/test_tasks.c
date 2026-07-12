#include "includes/main/cpu/sched.h"
#include <kprintf.h>
#include <log.h>

static volatile int shared_counter = 0;

static void task_a(void) {
    while (1) {
        LOG_INFO("Task A: shared=%d\n", shared_counter++);
        for (volatile int i = 0; i < 5000000; i++);
    }
}

static void task_b(void) {
    while (1) {
        LOG_INFO("Task B: shared=%d\n", shared_counter++);
        for (volatile int i = 0; i < 5000000; i++);
    }
}

static void task_c(void) {
    while (1) {
        LOG_INFO("Task C: shared=%d\n", shared_counter++);
        for (volatile int i = 0; i < 5000000; i++);
    }
}

void test_tasks_init(void) {
    sched_spawn(task_a);
    sched_spawn(task_b);
    sched_spawn(task_c);
    LOG_OK("3 test tasks spawned\n");
}
