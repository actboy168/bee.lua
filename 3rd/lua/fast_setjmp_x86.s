.386
.xmm
.model flat, syscall
.code

@fast_setjmp@4 proc
    mov eax, [esp]
    mov [ecx+014h], eax ; eip
    lea eax, [esp+004h]
    mov [ecx+000h], ebp
    mov [ecx+004h], ebx
    mov [ecx+008h], edi
    mov [ecx+00Ch], esi
    mov [ecx+010h], eax ;esp
    stmxcsr [ecx+018h]
    fnstcw  [ecx+01Ch]
    xor eax, eax
    ret
@fast_setjmp@4 endp

@fast_longjmp@8 proc
    mov ebp, [ecx+000h]
    mov ebx, [ecx+004h]
    mov edi, [ecx+008h]
    mov esi, [ecx+00Ch]
    mov esp, [ecx+010h]
    ldmxcsr  [ecx+018h]
    fldcw    [ecx+01Ch]
    mov eax, edx
    jmp dword ptr [ecx+014h]
@fast_longjmp@8 endp

end
