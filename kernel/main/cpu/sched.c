#include "includes/main/cpu/sched.h"
#include "includes/main/cpu/isr.h"
#include "includes/main/mm/heapalloc/tlsf.h"
#include <log.h>
#include <string.h>

extern tlsf_t kernel_tlsf_pool;

#define LAPIC_BASE      0xFEE00000ULL
#define APIC_EOI        0x0B0
#define APIC_LVT_TIMER  0x320
#define APIC_TIMER_INIT 0x380
#define APIC_TIMER_DIV  0x3E0

static void lapic_write(uint32_t reg, uint32_t val) {
    *(volatile uint32_t *)(LAPIC_BASE + reg) = val;
}

static void lapic_eoi(void) {
    lapic_write(APIC_EOI, 0);
}

static task_t tasks[MAX_TASKS];
static task_t *volatile current;
static int task_count;

static void idle_task(void) {
    while (1)
        __asm__ volatile("hlt");
}

static void setup_frame(uint64_t stack_base, void (*entry)(void), task_t *t) {
    uint64_t frame_addr = stack_base + STACK_SIZE - sizeof(Registers_t) - 8;
    Registers_t *frame = (Registers_t *)frame_addr;
    memset(frame, 0, sizeof(Registers_t));
    frame->rip = (uint64_t)entry;
    frame->cs  = 0x08;
    frame->rflags = 0x202;
    t->rsp = frame_addr;
}

int sched_spawn(void (*entry)(void)) {
    if (task_count >= MAX_TASKS) return -1;
    if (!current) return -1;

    int id = task_count;
    task_t *t = &tasks[id];
    t->stack_base = (uint64_t)tlsf_malloc(kernel_tlsf_pool, STACK_SIZE);
    if (!t->stack_base) return -1;
    t->id = id;
    setup_frame(t->stack_base, entry, t);

    t->next = current->next;
    current->next = t;
    task_count++;

    LOG_OK("Spawned task %d at %p\n", id, entry);
    return id;
}

void sched_yield(void) {
    __asm__ volatile("pause");
}

static void sched_tick(Registers_t *regs) {
    task_t *old = current;
    current = old->next;
    old->rsp = (uint64_t)regs;

    __asm__ volatile(
        "movq %0, %%rsp\n"
        "pop %%r15\n"
        "pop %%r14\n"
        "pop %%r13\n"
        "pop %%r12\n"
        "pop %%r11\n"
        "pop %%r10\n"
        "pop %%r9\n"
        "pop %%r8\n"
        "pop %%rbp\n"
        "pop %%rdi\n"
        "pop %%rsi\n"
        "pop %%rdx\n"
        "pop %%rcx\n"
        "pop %%rbx\n"
        "pop %%rax\n"
        "add $16, %%rsp\n"
        "iretq\n"
        : : "r"(current->rsp) : "memory"
    );

    __builtin_unreachable();
}

static void timer_handler(Registers_t *regs) {
    (void)regs;
    lapic_eoi();
    sched_tick(regs);
}

static void apic_timer_init(void) {
    uint32_t lvt = 0x20 | (1u << 17);
    lapic_write(APIC_LVT_TIMER, lvt);
    lapic_write(APIC_TIMER_DIV, 3);
    lapic_write(APIC_TIMER_INIT, 0x100000);
}

void sched_init(void) {
    memset(tasks, 0, sizeof(tasks));
    task_count = 0;

    tasks[0].stack_base = (uint64_t)tlsf_malloc(kernel_tlsf_pool, STACK_SIZE);
    tasks[0].id = 0;
    setup_frame(tasks[0].stack_base, idle_task, &tasks[0]);

    tasks[1].id = 1;
    tasks[1].stack_base = 0;
    tasks[1].rsp = 0;

    tasks[0].next = &tasks[1];
    tasks[1].next = &tasks[0];

    current = &tasks[1];
    task_count = 2;

    ISR_RegisterHandler(0x20, timer_handler);

    LOG_OK("RR scheduler ready\n");
}

void sched_start(void) {
    apic_timer_init();
    LOG_OK("Scheduler timer started (%d tasks)\n", task_count);
}
