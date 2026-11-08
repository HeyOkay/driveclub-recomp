.intel_syntax noprefix
.text
.global run_harness
.global harness_resume
run_harness:
 push rbp
 push r15
 push r14
 push r13
 push r12
 push rbx
 sub rsp, 0x48
 mov [rsp+0x30], rdi
 mov [rsp+0x38], rcx
 mov rbp, rsi
 mov r10, rdx
 mov rax, [r10+48]
 mov [rsp], rax
 mov rax, [r10+56]
 mov [rsp+8], rax
 mov rax, [r10+64]
 mov [rsp+16], rax
 mov rax, [r10+72]
 mov [rsp+24], rax
 mov rax, [r10+80]
 mov [rsp+32], rax
 mov rax, [r10+88]
 mov [rsp+40], rax
 mov rdi, [r10]
 mov rsi, [r10+8]
 mov rdx, [r10+16]
 mov rcx, [r10+24]
 mov r8, [r10+32]
 mov r9, [r10+40]
 mov rbx, 0xb0b0b0b0
 mov r12, 0xc0c0c0c0
 mov r13, 0xd0d0d0d0
 mov r14, 0xe0e0e0e0
 mov r15, 0xf0f0f0f0
 mov r11, [rsp+0x30]
 push 0x246
 popfq
 jmp r11
harness_resume:
 mov r11, [rsp+0x38]
 mov [r11], rbx
 mov [r11+8], r12
 mov [r11+16], r13
 mov [r11+24], r14
 mov [r11+32], r15
 mov [r11+40], rbp
 mov [r11+48], rsp
 pushfq
 pop rax
 mov [r11+56], rax
 add rsp, 0x48
 pop rbx
 pop r12
 pop r13
 pop r14
 pop r15
 pop rbp
 ret
