#include <sched.h>

//syscalls and whatnot
int64_t sys_getpid(void) { //syscall 39
    return current->pid; 
}

int64_t sys_getppid(void){ //syscall 110
    return current->father; 
}

int64_t sys_pause(void) { //syscall 34
    current->state = TASK_INTERRUPTIBLE;
    schedule();
    return 0;
}

//canadian "syscall"
int sys_nice(long increment) {
    //basically: sys_nice dictates how nice a task should be to other tasks
    //meaning that if a task is nice, it allows other tasks to go ahead infront of it
    //but if it's not nice, it increases its priority and goes first instead

    //P.S: this isnt a syscall btw.
    long new_priority = current->priority - increment;

    if (increment < 0 && !can_raise_priority(current))
        return -1;

    if (new_priority < PRIO_MIN) new_priority = PRIO_MIN;
    if (new_priority > PRIO_MAX) new_priority = PRIO_MAX;
    current->priority = new_priority;
    return 0;
}

//wee woo wee woo
int64_t sys_alarm(int64_t seconds) { //syscall 37
    if (seconds <= 0) { current->alarm = 0; return 0; }
    if (seconds > ALARM_MAX_SECONDS) seconds = ALARM_MAX_SECONDS;
    current->alarm = jiffies + (int64_t)HZ * seconds;
    return seconds;
}

int64_t sys_signal(int signum, void (*handler)(int), void (*restorer)(void)) { //also not a syscall
    if (signum < 1 || signum > 32) return -1;
    if (handler  && !is_valid_user_code_ptr(current, (void *)handler))  return -1;
    if (restorer && !is_valid_user_code_ptr(current, (void *)restorer)) return -1;
    current->sig_fn[signum - 1] = handler;
    current->sig_restorer = restorer;
    return 0;
}

//FORRRKKKKKK!
int64_t sys_fork(void) {
    return task_fork(current->tf);  
}

int64_t sys_yield(void){
    schedule();
    return 0;
}