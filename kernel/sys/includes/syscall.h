#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <stddef.h>
#include <trapframe.h>

#define SYSCALL_VECTOR 0x80
#define NR_SYSCALLS    512

struct task_struct;

//syscall defs
#define SYSCALL_LIST \
    X(1,  write)    \
    X(24, yield)    \
    X(34, pause)    \
    X(37, alarm)    \
    X(39, getpid)   \
    X(57, fork)     \
    X(110, getppid)

#define X(num, name) extern int64_t sys_##name(void);
SYSCALL_LIST
#undef X

void syscall_handler(struct trap_frame *tf);

int64_t arg_int(int n);
int     arg_ptr(int n, void **out, size_t size);
int64_t arg_str(int n, char **out);

int     is_user_range_valid(struct task_struct *t, uint64_t addr, uint64_t size);
int64_t is_user_string_valid(struct task_struct *t, uint64_t addr);

#endif