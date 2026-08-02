extern syscall_handler

global syscall_entry
syscall_entry:
    push qword 0        ; err_code (unused, int 0x80 has no real one)
    push qword 0x80      ; int_no

    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp         ; arg0 = struct trap_frame *tf
    call syscall_handler

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 16          ; discard int_no, err_code
    iretq