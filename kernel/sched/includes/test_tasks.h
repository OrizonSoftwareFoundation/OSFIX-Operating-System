#ifndef _TEST_TASKS_H
#define _TEST_TASKS_H

#include <stdint.h>
#include <sched.h>

typedef struct {
    const char *name;
    long priority;
    int is_privileged; 
} test_task_config_t;

int create_test_task(const test_task_config_t *config, int task_slot);
int create_default_test_tasks(void);
int create_privileged_task_test(void);
int create_priority_demo_tasks(void);
int get_tasks_completed(void);
void print_task_info(void);

#endif