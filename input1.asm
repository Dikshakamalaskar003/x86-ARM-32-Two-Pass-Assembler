section .data
    msg db 'Hello, World!', 0
    count dd 10
    array dd 1, 2, 3, 4, 5

section .text
    global main
    extern printf
    extern exit
    
main:
    ; Register to register instructions
    mov eax, ebx
    add ecx, edx
    sub esi, edi
    
    ; Register to immediate instructions
    mov eax, 42
    add ebx, 100
    sub ecx, 50
    
    ; Register to memory instructions
    mov eax, [ebx]
    mov ecx, [edx+8]
    mov edx, [esi*2+16]
    mov ebx, [eax+ebx*4+32]
    
    ; Memory to register instructions
    mov [eax], ebx
    mov [ecx+12], edx
    mov [esi*2+20], eax
    mov [eax+edi*8+40], ecx
    
    ; Complex addressing modes
    mov eax, [ebx+ecx*2]
    mov [esi+edi*4+100], edx
    mov eax, [ebp-8]
    mov [esp+20], ebx
    
    ; More instructions
    push eax
    pop ebx
    cmp eax, ebx
    jne label1
    
label1:
    call printf
    mov eax, 0
    ret
    
_exit:
    call exit
