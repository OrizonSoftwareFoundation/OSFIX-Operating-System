#ifndef SYS_PROC_H
#define SYS_PROC_H
#include <sched.h>

int64_t sys_getpid(void);
int64_t sys_getppid(void);
int64_t sys_pause(void);

//these two are not syscalls in the traditional sense
//see sys_proc.c for more info
int sys_nice(long increment);
int64_t sys_signal(int signum, void (*handler)(int), void (*restorer)(void));

int64_t sys_alarm(void);
int64_t sys_fork(void);
int64_t sys_yield(void);
#endif