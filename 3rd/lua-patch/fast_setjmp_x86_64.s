.code

fast_setjmp proc
    mov rax, [rsp]
    mov [rcx+050h], rax ; rip
    lea rax, [rsp+008h]
    mov [rcx+000h], rax
    mov [rcx+008h], rbx
    mov [rcx+010h], rax ; rsp
    mov [rcx+018h], rbp
    mov [rcx+020h], rsi
    mov [rcx+028h], rdi
    mov [rcx+030h], r12
    mov [rcx+038h], r13
    mov [rcx+040h], r14
    mov [rcx+048h], r15
    stmxcsr [rcx+058h]
    fnstcw  [rcx+05Ch]
    movaps [rcx+060h], xmm6
    movaps [rcx+070h], xmm7
    movaps [rcx+080h], xmm8
    movaps [rcx+090h], xmm9
    movaps [rcx+0A0h], xmm10
    movaps [rcx+0B0h], xmm11
    movaps [rcx+0C0h], xmm12
    movaps [rcx+0D0h], xmm13
    movaps [rcx+0E0h], xmm14
    movaps [rcx+0F0h], xmm15
    xor eax, eax
    ret
fast_setjmp endp

fast_longjmp proc
    mov rbx, [rcx+008h]
    mov rsp, [rcx+010h]
    mov rbp, [rcx+018h]
    mov rsi, [rcx+020h]
    mov rdi, [rcx+028h]
    mov r12, [rcx+030h]
    mov r13, [rcx+038h]
    mov r14, [rcx+040h]
    mov r15, [rcx+048h]
    ldmxcsr [rcx+058h]
    fldcw   [rcx+05Ch]
    movaps xmm6,  [rcx+060h]
    movaps xmm7,  [rcx+070h]
    movaps xmm8,  [rcx+080h]
    movaps xmm9,  [rcx+090h]
    movaps xmm10, [rcx+0A0h]
    movaps xmm11, [rcx+0B0h]
    movaps xmm12, [rcx+0C0h]
    movaps xmm13, [rcx+0D0h]
    movaps xmm14, [rcx+0E0h]
    movaps xmm15, [rcx+0F0h]
    mov eax, edx
    jmp qword ptr [rcx+050h]
fast_longjmp endp

end
