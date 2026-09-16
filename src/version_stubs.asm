; Windows x64 ABI-preserving version.dll forwarding stubs.
; Stack parameters remain in place. Volatile argument GPRs and XMM0..3 survive lazy resolution.
EXTERN rgResolveVersion:PROC
.code
rgVersionDispatch PROC FRAME
    sub rsp, 0A8h
    .allocstack 0A8h
    .endprolog
    mov [rsp+20h], rcx
    mov [rsp+28h], rdx
    mov [rsp+30h], r8
    mov [rsp+38h], r9
    movdqu [rsp+40h], xmm0
    movdqu [rsp+50h], xmm1
    movdqu [rsp+60h], xmm2
    movdqu [rsp+70h], xmm3
    mov ecx, r10d
    call rgResolveVersion
    mov rcx, [rsp+20h]
    mov rdx, [rsp+28h]
    mov r8, [rsp+30h]
    mov r9, [rsp+38h]
    movdqu xmm0, [rsp+40h]
    movdqu xmm1, [rsp+50h]
    movdqu xmm2, [rsp+60h]
    movdqu xmm3, [rsp+70h]
    add rsp, 0A8h
    jmp rax
rgVersionDispatch ENDP
rg_GetFileVersionInfoA PROC
    mov r10d, 0
    jmp rgVersionDispatch
rg_GetFileVersionInfoA ENDP
rg_GetFileVersionInfoByHandle PROC
    mov r10d, 1
    jmp rgVersionDispatch
rg_GetFileVersionInfoByHandle ENDP
rg_GetFileVersionInfoExA PROC
    mov r10d, 2
    jmp rgVersionDispatch
rg_GetFileVersionInfoExA ENDP
rg_GetFileVersionInfoExW PROC
    mov r10d, 3
    jmp rgVersionDispatch
rg_GetFileVersionInfoExW ENDP
rg_GetFileVersionInfoSizeA PROC
    mov r10d, 4
    jmp rgVersionDispatch
rg_GetFileVersionInfoSizeA ENDP
rg_GetFileVersionInfoSizeExA PROC
    mov r10d, 5
    jmp rgVersionDispatch
rg_GetFileVersionInfoSizeExA ENDP
rg_GetFileVersionInfoSizeExW PROC
    mov r10d, 6
    jmp rgVersionDispatch
rg_GetFileVersionInfoSizeExW ENDP
rg_GetFileVersionInfoSizeW PROC
    mov r10d, 7
    jmp rgVersionDispatch
rg_GetFileVersionInfoSizeW ENDP
rg_GetFileVersionInfoW PROC
    mov r10d, 8
    jmp rgVersionDispatch
rg_GetFileVersionInfoW ENDP
rg_VerFindFileA PROC
    mov r10d, 9
    jmp rgVersionDispatch
rg_VerFindFileA ENDP
rg_VerFindFileW PROC
    mov r10d, 10
    jmp rgVersionDispatch
rg_VerFindFileW ENDP
rg_VerInstallFileA PROC
    mov r10d, 11
    jmp rgVersionDispatch
rg_VerInstallFileA ENDP
rg_VerInstallFileW PROC
    mov r10d, 12
    jmp rgVersionDispatch
rg_VerInstallFileW ENDP
rg_VerLanguageNameA PROC
    mov r10d, 13
    jmp rgVersionDispatch
rg_VerLanguageNameA ENDP
rg_VerLanguageNameW PROC
    mov r10d, 14
    jmp rgVersionDispatch
rg_VerLanguageNameW ENDP
rg_VerQueryValueA PROC
    mov r10d, 15
    jmp rgVersionDispatch
rg_VerQueryValueA ENDP
rg_VerQueryValueW PROC
    mov r10d, 16
    jmp rgVersionDispatch
rg_VerQueryValueW ENDP
END
