.intel_syntax noprefix
.text
.global rear_native_stub
rear_native_stub:
 # Entered by JMP from 5B92D6, with original 5EDE70 ABI intact.
 lea r11, [rip + return_5b95c3]
 cmp qword ptr [rbp+8], r11
 jne original_path
 mov r10, [rbp]
 lea r11, [rip + return_5c720f]
 cmp qword ptr [r10+8], r11
 jne original_path
 mov r10, [r10]
 lea r11, [rip + return_fbd4d9]
 cmp qword ptr [r10+8], r11
 jne original_path
 # Additional width/height bound: current output descriptor, initialized before view.
 mov r10, [rsp+0x28]
 test r10, r10
 je original_path
 mov r10, [r10]
 test r10, r10
 je original_path
 mov r10, [r10+0x530]
 test r10, r10
 je original_path
 mov r11d, [r10+0x2c]
 cmp r11d, 384
 jne test_256
 cmp dword ptr [r10+0x30], 136
 je native_path
 jmp original_path
test_256:
 cmp r11d, 256
 jne test_1152
 cmp dword ptr [r10+0x30], 128
 je native_path
 jmp original_path
test_1152:
 cmp r11d, 1152
 jne test_768
 cmp dword ptr [r10+0x30], 408
 je native_path
 jmp original_path
test_768:
 cmp r11d, 768
 jne original_path
 cmp dword ptr [r10+0x30], 384
 jne original_path
native_path:
 sub rsp, 0xc0
 mov [rsp+0x40], rdi
 mov [rsp+0x48], rsi
 mov [rsp+0x50], rdx
 mov [rsp+0x58], rcx
 mov [rsp+0x60], r8
 # Output refs must begin null:739F30 atomically replaces/release old output.
 mov qword ptr [rsp+0xa0], 0
 mov qword ptr [rsp+0xa8], 0
 mov qword ptr [rsp+0xb0], 0
 mov qword ptr [rsp+0x08], 0
 mov qword ptr [rsp+0x10], 1
 mov qword ptr [rsp+0x18], 0
 mov qword ptr [rsp+0x20], 6
 mov qword ptr [rsp+0x28], 0
 mov qword ptr [rsp+0x30], 0
 .macro make_desc slot, width, height, format
 mov qword ptr [rsp], \format
 lea rdi, [rip + image_factory_slot]
 mov rdi, [rdi]
 lea rsi, [rsp+\slot]
 mov edx, \width
 mov ecx, \height
 mov r8d, 1
 xor r9d, r9d
 call create_description
 .endm
 make_desc 0xa0, 2048, 4096, 7
 make_desc 0xa8, 512, 512, 37
 make_desc 0xb0, 512, 512, 7
 # Restore original ABI; replace exactly the three description arguments.
 mov rax, [rsp+0xa8]
 mov [rsp], rax
 mov rax, [rsp+0xb0]
 mov [rsp+8], rax
 .macro copy_stack offset
 mov rax, [rsp+0xc0+\offset]
 mov [rsp+\offset], rax
 .endm
 copy_stack 0x10
 copy_stack 0x18
 copy_stack 0x20
 copy_stack 0x28
 mov rdi, [rsp+0x40]
 mov rsi, [rsp+0x48]
 mov rdx, [rsp+0x50]
 mov rcx, [rsp+0x58]
 mov r8, [rsp+0x60]
 mov r9, [rsp+0xa0]
 call construct_view
 # Actual targets retained each description at749A72..749A7F.
 # Drop local output ownership with the original reference counting convention.
 .macro release_desc slot
 mov rdi, [rsp+\slot]
 test rdi, rdi
 je .Ldone\@
 mov eax, -1
 lock xadd dword ptr [rdi+8], eax
 cmp eax, 1
 jne .Ldone\@
 mov rax, [rdi]
 call qword ptr [rax+8]
 .Ldone\@:
 .endm
 release_desc 0xa0
 release_desc 0xa8
 release_desc 0xb0
 add rsp, 0xc0
 jmp resume_view
original_path:
 call construct_view
 jmp resume_view
