#include <stdint.h>
#include <stddef.h>
#include <flanterm.h>
#include <kprintf.h>
#include <syscall.h>
#include <serial.h>
#include <sys_proc.h>
#include <trapframe.h>

extern struct flanterm_context *global_flanterm;

int64_t is_user_string_valid(struct task_struct *t, uint64_t addr) {
    (void)t;
    const char *s = (const char *)(uintptr_t)addr;
    int64_t len = 0;
    while (s[len] != '\0') len++;
    return len;
}


//i wrote arg_* while i had a fever so yknow i dont remember what they do exactly
static uint64_t argraw(int n) {
    struct trap_frame *tf = current->tf;
    switch (n) {
        case 0: return tf->rdi;
        case 1: return tf->rsi;
        case 2: return tf->rdx;
        case 3: return tf->r10;
        case 4: return tf->r8;
        case 5: return tf->r9;
        default: return 0;
    }
}

int64_t arg_int(int n) { return (int64_t)argraw(n); }

int arg_ptr(int n, void **out, size_t size) {
    uint64_t addr = argraw(n);
    if (!is_user_range_valid(current, addr, size)) return -1;
    *out = (void *)(uintptr_t)addr;
    return 0;
}

int64_t arg_str(int n, char **out) {
    uint64_t addr = argraw(n);
    int64_t len = is_user_string_valid(current, addr);
    if (len < 0) return -1;
    *out = (char *)(uintptr_t)addr;
    return len;
}

//no comment needed
int64_t sys_write(void) {
    int fd = (int)arg_int(0);
    size_t count = (size_t)arg_int(2);
    void *buf;

    if (fd != 1 && fd != 2) return -1;
    if (arg_ptr(1, &buf, count) < 0) return -1;

    flanterm_write(global_flanterm, (const char *)buf, count);
    return (int64_t)count;
}

static int64_t (*syscalls[NR_SYSCALLS])(void) = {
#define X(num, name) [num] = sys_##name,
    SYSCALL_LIST
#undef X
};

//the syscall handler handles the syscalls that get handled by the syscall handler 
void syscall_handler(struct trap_frame *tf) {
    current->tf = tf;
    serial_printf("syscall entered: num=%llu\n", tf->rax);
    int64_t num = (int64_t)tf->rax;

    if (num >= 0 && num < NR_SYSCALLS && syscalls[num])
        tf->rax = (uint64_t)syscalls[num]();
    else
        tf->rax = (uint64_t)-1;
}