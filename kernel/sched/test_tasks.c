#include "test_tasks.h"
#include <kprintf.h>
#include <string.h>
#include <sys_proc.h>

static int tasks_completed = 0;

static void privileged_function(void) {
    int count = 0;
    kprintf("Privileged task started! PID=%d, UID=%d, EUID=%d\n", 
            current->pid, current->uid, current->euid);

    while (count < 10) {
        kprintf("Privileged task running: iteration %d\n", count++);
        
        if (sys_nice(-5) == 0) {
            kprintf("Privileged task raised priority to %ld\n", current->priority);
        }
        
        schedule();
        for (volatile int i = 0; i < 200000; i++);
    }
    
    kprintf("Privileged task completed!\n");
    tasks_completed++;
    current->state = TASK_ZOMBIE;
    schedule();
}

static void unprivileged_function(void) {
    int count = 0;
    kprintf("Unprivileged task started! PID=%d, UID=%d, EUID=%d\n", 
            current->pid, current->uid, current->euid);
    
    while (count < 10) {
        kprintf("Unprivileged task running: iteration %d\n", count++);
        
        if (sys_nice(-5) == -1) {
            kprintf("Unprivileged task FAILED to raise priority (expected)\n");
        }
        
        schedule();
        for (volatile int i = 0; i < 200000; i++);
    }
    
    kprintf("Unprivileged task completed!\n");
    tasks_completed++;
    current->state = TASK_ZOMBIE;
    schedule();
}

int create_test_task(const test_task_config_t *config, int task_slot) {
    if (!config || task_slot < 1 || task_slot >= NR_TASKS) {
        return -1;
    }
    
    static struct task_struct tasks[NR_TASKS];
    static uint8_t stacks[NR_TASKS][4096];
    static int next_task = 1;
    
    if (next_task >= NR_TASKS) {
        kprintf("Too many tasks!\n");
        return -1;
    }
    
    struct task_struct *t = &tasks[next_task];
    void *stack = stacks[next_task];
    
    void (*func)(void) = config->is_privileged ? privileged_function : unprivileged_function;
    
    if (create_task(t, stack, 4096, func) != 0) {
        kprintf("Failed to create task\n");
        return -1;
    }
    
    t->priority = config->priority > 0 ? config->priority : 20;
    t->counter = t->priority;
    
    if (config->is_privileged) {
        t->is_privileged = 1;
        t->uid = 0;
        t->euid = 0;
        t->gid = 0;
        t->egid = 0;
    }
    
    task[task_slot] = t;
    
    kprintf("Created %s task %d (priority=%ld)\n", 
            config->is_privileged ? "PRIVILEGED" : "UNPRIVILEGED",
            t->pid, t->priority);
    
    next_task++;
    return 0;
}

int create_default_test_tasks(void) {
    int count = 0;
    
    test_task_config_t configs[] = {
        //priv = 1, unpriv = 0
        {"Privileged", 25, 1},    
        {"Unpriv1", 20, 0},      
        {"Unpriv2", 20, 0},      
        {"Unpriv3", 15, 0}        
    };
    
    for (int i = 0; i < 4; i++) {
        if (create_test_task(&configs[i], i + 1) == 0) count++;
    }
    
    kprintf("Created %d tasks (1 privileged, 3 unprivileged)\n", count);
    return count;
}

int create_privileged_task_test(void) {
    int count = 0;
    
    test_task_config_t priv = {"Privileged", 35, 1};
    test_task_config_t unpriv = {"Unprivileged", 20, 0};
    
    if (create_test_task(&priv, 1) == 0) count++;
    if (create_test_task(&unpriv, 2) == 0) count++;
    
    kprintf("Created %d tasks for privilege test\n", count);
    return count;
}

int create_priority_demo_tasks(void) {
    int count = 0;
    
   test_task_config_t low = {"Low", 10, 0};
    test_task_config_t normal = {"Normal", 20, 0};
    test_task_config_t high = {"High", 35, 0};
    
    if (create_test_task(&low, 1) == 0) count++;
    if (create_test_task(&normal, 2) == 0) count++;
    if (create_test_task(&high, 3) == 0) count++;
    
    kprintf("Created %d priority tasks\n", count);
    return count;
}

int get_tasks_completed(void) {
    return tasks_completed;
}

void print_task_info(void) {
    kprintf("\nTask information:\n");
    kprintf("Slot\tPID\tState\tPriority\tPrivileged\tUID\n");
    
    for (int i = 0; i < NR_TASKS; i++) {
        struct task_struct *t = task[i];
        if (!t) continue;
        
        const char *states[] = {"RUNNING", "INT", "UNINT", "ZOMBIE", "STOPPED", "UNUSED"};
        kprintf("%d\t%d\t%s\t%ld\t\t%s\t\t%d\n", 
                i, t->pid, states[t->state], t->priority,
                t->is_privileged ? "YES" : "NO",
                t->uid);
    }
    kprintf("Completed: %d\n", tasks_completed);
}