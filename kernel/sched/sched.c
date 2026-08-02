#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <kprintf.h>
#include <gdt.h>
#include <trapframe.h>
#include <vmm.h>
#include <stdlib.h>

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

volatile int64_t jiffies = 0;
struct task_struct *current = NULL;
struct task_struct *task[NR_TASKS];
static struct task_struct idle_task;
extern struct tss_entry tss;

static void wq_add(wait_queue_head_t *wq, struct wait_queue_entry *e){
    e->next = NULL;
    e->prev = wq->tail;
    if (wq->tail) wq->tail->next = e; else wq->head = e;
    wq->tail = e;
}

static void wq_remove(wait_queue_head_t *wq, struct wait_queue_entry *e){
    if (e->prev) e->prev->next = e->next; else if (wq->head == e) wq->head = e->next;
    if (e->next) e->next->prev = e->prev; else if (wq->tail == e) wq->tail = e->prev;
    e->next = NULL;
    e->prev = NULL;
}

static inline void irq_disable(void) { __asm__ volatile ("cli"); }
static inline void irq_enable(void) { __asm__ volatile ("sti"); }

extern void swtch(uint64_t *old_rsp_slot, uint64_t new_rsp);
__asm__(
".text\n"
".globl swtch\n"
"swtch:\n"
" pushq %rbx\n"
" pushq %rbp\n"
" pushq %r12\n"
" pushq %r13\n"
" pushq %r14\n"
" pushq %r15\n"
" movq %rsp, (%rdi)\n"
" movq %rsi, %rsp\n"
" popq %r15\n"
" popq %r14\n"
" popq %r13\n"
" popq %r12\n"
" popq %rbp\n"
" popq %rbx\n"
" ret\n"
);

extern void task_trampoline(void);
__asm__(
".text\n"
".globl task_trampoline\n"
"task_trampoline:\n"
" popq %rax\n"
" xor %rbp, %rbp\n"
" call *%rax\n"
"1: cli\n"
" hlt\n"
" jmp 1b\n"
);

static void enter_userspace_c(struct trap_frame *tf){
    uint64_t user_ss  = tf->ss;
    uint64_t user_rsp = tf->rsp;
    uint64_t user_cs  = tf->cs;
    uint64_t user_rip = tf->rip;

    __asm__ volatile (
        "push %0\n"
        "push %1\n"
        "pushf\n"
        "push %2\n"
        "push %3\n"
        "iretq\n"
        :
        : "r"(user_ss), "r"(user_rsp), "r"(user_cs), "r"(user_rip)
        : "memory"
    );
}

extern void enter_userspace(struct trap_frame *tf);

__asm__(
".text\n"
".global enter_userspace\n"
"enter_userspace:\n"

"    mov %rdi, %rsp\n"

"    pop %r15\n"
"    pop %r14\n"
"    pop %r13\n"
"    pop %r12\n"
"    pop %r11\n"
"    pop %r10\n"
"    pop %r9\n"
"    pop %r8\n"

"    pop %rbp\n"
"    pop %rdi\n"
"    pop %rsi\n"
"    pop %rdx\n"
"    pop %rcx\n"
"    pop %rbx\n"
"    pop %rax\n"

"    add $16, %rsp\n"        

"    iretq\n"
);

static void set_kernel_stack(struct task_struct *t) {
    if (t && t->kstack_top)
        tss.rsp0 = t->kstack_top;
}

static void context_switch(struct task_struct *prev, struct task_struct *next) {
    if (next->cr3)
        __asm__ volatile ("mov %0, %%cr3" :: "r"(next->cr3) : "memory");
    set_kernel_stack(next);
    swtch(&prev->rsp, next->rsp);
}

struct task_trampoline_frame {
    uint64_t r15, r14, r13, r12, rbp, rbx;
    uint64_t return_addr;
    uint64_t entry_point;
};

static int64_t next_pid = 1;

int create_task(struct task_struct *t, void *stack, size_t stack_size, void (*entry)(void)) {
    if (!t || !stack || stack_size < sizeof(struct task_trampoline_frame))
        return -1;
    memset(t, 0, sizeof(*t));
    t->pid = next_pid++;
    t->state = TASK_RUNNING;
    t->counter = PRIO_DEFAULT;
    t->priority = PRIO_DEFAULT;
    t->father = -1;
    t->kstack_base = stack;
    t->kstack_size = stack_size;
    t->kstack_top = (uint64_t)(uintptr_t)stack + stack_size;
    t->kstack_top &= ~0xFULL;

    uint8_t *top = (uint8_t *)(uintptr_t)t->kstack_top;
    struct task_trampoline_frame *frame =
        (struct task_trampoline_frame *)(top - sizeof(*frame));
    memset(frame, 0, sizeof(*frame));
    frame->return_addr = (uint64_t)(uintptr_t)task_trampoline;
    frame->entry_point = (uint64_t)(uintptr_t)entry;
    t->rsp = (uint64_t)(uintptr_t)frame;
    return 0;
}

int create_privileged_task(struct task_struct *t, void *stack, size_t stack_size, void (*entry)(void), int is_privileged) {
    if (create_task(t, stack, stack_size, entry) != 0)
        return -1;
    if (is_privileged) {
        t->is_privileged = 1;
        t->uid = t->euid = 0;
        t->gid = t->egid = 0;
    }
    return 0;
}

static void user_task_bootstrap(void){
    if (!current || !current->tf) {
        log(Fatal, "user bootstrap: current=%p tf=%p\n",
            current, current ? current->tf : NULL);
        for (;;)
            __asm__ volatile ("cli; hlt");
    }

    serial_printf("sched.c:241:user bootstrap pid=%d, rax=%x, rip=%x\n",
        current->pid,
        current->tf->rax,
        current->tf->rip);

    enter_userspace(current->tf);

    __builtin_unreachable();
}
int create_user_task(struct task_struct *t, void *kstack, size_t kstack_size, uint64_t user_rip, uint64_t user_rsp, uint64_t cr3){
    if (create_task(t, kstack, kstack_size, user_task_bootstrap) != 0)
        return -1;

    t->flags |= TASK_FLAG_USER;
    t->is_privileged = 0;
    t->cr3 = cr3;
    struct trap_frame *tf = malloc(sizeof(struct trap_frame));
    if (!tf)
        return -1;
    memset(tf, 0, sizeof(*tf));
    tf->rip = user_rip;
    tf->cs  = 0x1B;
    tf->rsp = user_rsp;
    tf->ss  = 0x23;
    t->tf = tf;
    return 0;
}

void task_set_trap_frame(struct task_struct *t, struct trap_frame *tf) {
    if (t)
        t->tf = tf;
}

void return_to_userspace(void) {
    if (!current || !current->tf) {
        log(Fatal, "return_to_userspace with no frame\n");
        for (;;)
            __asm__ volatile ("cli; hlt");
    }
    set_kernel_stack(current);
    enter_userspace_c(current->tf);
}

int is_valid_user_code_ptr(struct task_struct *t, void *addr) {
    uintptr_t ptr = (uintptr_t)addr;
    (void)t;
    if (ptr > 0x0000800000000000ULL) {
        log(Fatal, "User code was found in kernelspace, please investigate");
        return 0;
    }
    if (ptr < 0x1000) {
        log(Fatal, "User code was found to be null, please investigate");
        return 0;
    }
    return 1;
}

int is_user_range_valid(struct task_struct *t, uint64_t addr, uint64_t size) {
    (void)t;
    if (size == 0)
        return 0;
    uint64_t end = addr + size;
    if (end < addr)
        return 0;
    if (end > 0x0000800000000000ULL) {
        log(Fatal, "User range was found in kernelspace, please investigate");
        return 0;
    }
    if (addr < 0x1000) {
        log(Fatal, "User range was found to be null, please investigate");
        return 0;
    }
    return 1;
}

int can_raise_priority(struct task_struct *t) {
    if (t->is_privileged) return 1;
    if (t->euid == 0 || t->uid == 0) return 1;
    return 0;
}

void scheduler_init(void) {
    memset(task, 0, sizeof(task));
    memset(&idle_task, 0, sizeof(idle_task));
    idle_task.pid = 0;
    idle_task.state = TASK_RUNNING;
    idle_task.priority = PRIO_DEFAULT;
    idle_task.father = -1;
    idle_task.is_privileged = 1;
    task[0] = &idle_task;
    current = &idle_task;
    for (int i = 1; i < NR_TASKS; i++)
        task[i] = NULL;
    log(Ok, "Initialized scheduler, ready for tasks\n");
}

void schedule(void) {
    irq_disable();
    for (int i = 0; i < NR_TASKS; i++) {
        struct task_struct *t = task[i];
        if (!t) continue;
        if (t->alarm && t->alarm < jiffies) {
            t->signal |= (1u << (14 - 1));
            t->alarm = 0;
            if (t->state == TASK_INTERRUPTIBLE)
                t->state = TASK_RUNNING;
        }
    }

    struct task_struct *next = NULL;
    for (;;) {
        long best = -1;
        next = NULL;
        for (int i = 0; i < NR_TASKS; i++) {
            struct task_struct *t = task[i];
            if (!t) continue;
            if (t->state == TASK_RUNNING && t->counter >= best) {
                best = t->counter;
                next = t;
            }
        }
        if (best != 0)
            break;
        if (next == NULL) {
            next = &idle_task;
            break;
        }
        for (int i = 0; i < NR_TASKS; i++) {
            struct task_struct *t = task[i];
            if (!t) continue;
            t->counter = (t->counter >> 1) + t->priority;
        }
    }

    struct task_struct *prev = current;
    current = next;
    set_kernel_stack(next);
    irq_enable();
    serial_printf("sched.c:377: switch %d > %d, rsp=%x\n",
    prev->pid,
    next->pid,
    next->rsp);
    if (prev != next)
        context_switch(prev, next);
}
void do_timer_tick(int user_mode) {
    (void)user_mode;

    if (current->counter > 0)
        current->counter--;

    if (current->counter == 0) {
        serial_printf("sched.c:391: timeslice expired pid=%d\n", current->pid);
        schedule();
    }
}
void sleep_on(wait_queue_head_t *wq) {
    struct wait_queue_entry entry = { current, NULL, NULL };
    irq_disable();
    wq_add(wq, &entry);
    current->state = TASK_UNINTERRUPTIBLE;
    irq_enable();
    schedule();
    irq_disable();
    wq_remove(wq, &entry);
    irq_enable();
}

void interruptible_sleep_on(wait_queue_head_t *wq) {
    struct wait_queue_entry entry = { current, NULL, NULL };
    irq_disable();
    wq_add(wq, &entry);
    current->state = TASK_INTERRUPTIBLE;
    irq_enable();
    schedule();
    irq_disable();
    wq_remove(wq, &entry);
    irq_enable();
}

void wake_up(wait_queue_head_t *wq) {
    irq_disable();
    for (struct wait_queue_entry *e = wq->head; e; e = e->next) {
        if (e->task->state == TASK_INTERRUPTIBLE ||
            e->task->state == TASK_UNINTERRUPTIBLE)
            e->task->state = TASK_RUNNING;
    }
    irq_enable();
}

void wake_up_one(wait_queue_head_t *wq) {
    irq_disable();
    if (wq->head) {
        struct wait_queue_entry *e = wq->head;
        if (e->task->state == TASK_INTERRUPTIBLE ||
            e->task->state == TASK_UNINTERRUPTIBLE)
            e->task->state = TASK_RUNNING;
        wq_remove(wq, e);
    }
    irq_enable();
}

//FUCK YEAAHHH! FORKKKKK
int64_t task_fork(struct trap_frame *tf){
    struct task_struct *parent = current;
    int slot;
    struct task_struct *child;
    void *kstack;
    uint64_t new_cr3;

    if (!parent || !tf)
        return -1;

    slot = -1;
    for (int i = 1; i < NR_TASKS; i++) {
        if (task[i] == NULL) {
            slot = i;
            break;
        }
    }
    if (slot < 0)
        return -1;

    child = malloc(sizeof(*child));
    if (!child)
        return -1;

    kstack = malloc(0x4000);
    if (!kstack) {
        free(child);
        return -1;
    }

    new_cr3 = vmm_clone_address_space();
    if (!new_cr3) {
        free(kstack);
        free(child);
        return -1;
    }

    if (create_user_task(child, kstack, 0x4000, tf->rip, tf->rsp, new_cr3) != 0) {
        free(kstack);
        free(child);
        return -1;
    }

    if (child->tf) {
        *child->tf = *tf;
        child->tf->rax = 0;
    }

    child->father = parent->pid;
    child->uid = parent->uid;
    child->euid = parent->euid;
    child->gid = parent->gid;
    child->egid = parent->egid;
    child->priority = parent->priority;
    child->counter = parent->priority;
    child->state = TASK_RUNNING;
    child->flags |= TASK_FLAG_USER;

    serial_printf("sched.c:502: fork: parent=%d, child=%d, child_tf=%p, rax=%x, rip=%x\n",
    parent->pid,
    child->pid,
    child->tf,
    child->tf->rax,
    child->tf->rip);

    task[slot] = child;
    serial_printf("sched.c:510: child created pid=%d, rsp=%x, kstack=%x, state=%d\n",
    child->pid,
    child->rsp,
    child->kstack_top,
    child->state);
    //schedule();
    return child->pid;
}