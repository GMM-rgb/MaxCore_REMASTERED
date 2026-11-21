%macro MATH_ADD 2
    mov rax, %1          ; first # into RAX
    add rax, %2          ; add second into RAX
%endmacro

extern printf            ; Windows C runtime print function

section .data
    fmt db "%lld", 10, 0 ; Format string for 64-bit integer + newline

section .text
    global main          ; Windows/GCC entry point (replaces _start)

main:
    sub rsp, 40          ; 32-byte Windows shadow space + 8-byte stack alignment

    MATH_ADD 5, 4        ; RAX is now 4

    ; Print RAX (replaces Linux sys_write)
    lea rcx, [rel fmt]   ; 1st argument: format string (RIP-relative)
    mov rdx, rax         ; 2nd argument: the result in RAX (4)
    call printf

    ; Exit cleanly (replaces Linux sys_exit)
    add rsp, 40          ; Restore stack
    xor eax, eax         ; Return 0
    ret
