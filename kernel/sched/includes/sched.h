#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>
#include <stddef.h>
#include <trapframe.h>
#define NR_TASKS 64
#define HZ 100
#define PRIO_MIN 1
#define PRIO_MAX 40
#define PRIO_DEFAULT 20
#define ALARM_MAX_SECONDS ((int64_t)1 << 40)

#define GDT_USER_CODE_SEGMENT 0x18
#define GDT_USER_DATA_SEGMENT 0x20
#define USER_CS ((GDT_USER_CODE_SEGMENT) | 3)
#define USER_SS ((GDT_USER_DATA_SEGMENT) | 3)

#define TASK_FLAG_USER (1u << 0)

typedef enum {
    TASK_RUNNING = 0,
    TASK_INTERRUPTIBLE,
    TASK_UNINTERRUPTIBLE,
    TASK_ZOMBIE,
    TASK_STOPPED,
    TASK_UNUSED
} task_state_t;

struct task_struct;

struct wait_queue_entry {
    struct task_struct *task;
    struct wait_queue_entry *next;
    struct wait_queue_entry *prev;
};

typedef struct {
    struct wait_queue_entry *head;
    struct wait_queue_entry *tail;
} wait_queue_head_t;

struct task_struct {
    int64_t pid;
    volatile task_state_t state;
    long counter;
    long priority;
    uint64_t rsp;
    void *kstack_base;
    size_t kstack_size;
    uint64_t cr3;
    int64_t alarm;
    uint64_t signal;
    void (*sig_fn[32])(int);
    void (*sig_restorer)(void);
    int64_t father;
    uint32_t uid, euid, gid, egid;
    int is_privileged;
    struct trap_frame *tf;
    uint32_t flags;
    uint64_t kstack_top;
};

extern volatile int64_t jiffies;
extern struct task_struct *current;
extern struct task_struct *task[NR_TASKS];

void scheduler_init(void);
void schedule(void);
void do_timer_tick(int user_mode);

int create_task(struct task_struct *t, void *stack, size_t stack_size,
                void (*entry)(void));
int create_privileged_task(struct task_struct *t, void *stack, size_t stack_size,
                           void (*entry)(void), int is_privileged);
int create_user_task(struct task_struct *t, void *kstack, size_t kstack_size,
                     uint64_t user_rip, uint64_t user_rsp, uint64_t cr3);

void task_set_trap_frame(struct task_struct *t, struct trap_frame *tf);
void return_to_userspace(void);

void sleep_on(wait_queue_head_t *wq);
void interruptible_sleep_on(wait_queue_head_t *wq);
void wake_up(wait_queue_head_t *wq);
void wake_up_one(wait_queue_head_t *wq);

int is_valid_user_code_ptr(struct task_struct *t, void *addr);
int is_user_range_valid(struct task_struct *t, uint64_t addr, uint64_t size);
int can_raise_priority(struct task_struct *t);
int64_t task_fork(struct trap_frame *tf);
#endif 