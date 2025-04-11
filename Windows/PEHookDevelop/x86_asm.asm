.386
.model flat

includelib kernel32.lib

ExitProcess proto STDCALL :DWORD
GetStdHandle proto STDCALL :DWORD
WriteConsoleA proto STDCALL :DWORD, :DWORD, :DWORD, :DWORD, :DWORD

ASSUME fs:fs

.code

log proc
	push ebp ; save previous stack frame
	mov ebp, esp ; flush current stack frame
	push -11 ; -11 for STD_OUTPUT_HANDLE
	call GetStdHandle
	mov ecx, eax
	mov edx, [ebp + dword * 2]
	push 10 ; force output 10 byte
	push edx
	push ecx
	call WriteConsoleA
	mov esp, ebp
	pop ebp ; recover previous stack frame
	ret
log endp

strcmp proc
; ecx, ebx, edx stored, ptr size
stored equ dword * 3
str1 equ [ebp - dword]
str2 equ [ebp - dword * 2]
len equ [ebp - dword * 3]
i equ edx
	; stdcall cc
	; proto *str1, *str2, size
	; if eq, ret 0, or ret 1
	push ebp
	; may break ecx, ebx, store it for safe
	push ecx
	push ebx
	push edx
	mov ebp, esp
	mov ecx, [ebp + dword * 2 + stored]
	push ecx ; str1
	mov ecx, [ebp + dword * 3 + stored]
	push ecx ; str2
	mov ecx, [ebp + dword * 4 + stored]
	push ecx ; len
	mov i, 0
	mov eax, 0
	; prologue end
check_len:
	cmp i, len
	jnb short epilo
check_str:
	mov ecx, str1
	mov cl, BYTE PTR [ecx + i]
	mov ebx, str2
	cmp cl, BYTE PTR [ebx + i]
	jnz short not_eq
	; current str is same
	inc i
	jmp short check_len
not_eq:
	mov eax, 1
epilo:
	mov esp, ebp
	pop edx
	pop ebx
	pop ecx
	pop ebp
	ret
strcmp endp

walkPEB proc
; break ecx, rbx
stored_walkPEB equ dword * 2
dllname equ [ebp - dword]
len equ [ebp - dword * 2]
flink equ [ebp - dword * 3]
	; stdcall cc
	; proto *dllname, len
	; ret DllBase addr
	push ebp
	push ecx
	push ebx
	mov ebp, esp
	mov eax, [ebp + dword * 2 + stored_walkPEB] ; dllname
	push eax ; dllname
	mov eax, [ebp + dword * 3 + stored_walkPEB] ; len
	push eax ; len
	; access TEB, x86 access by fs
	mov eax, fs:[18h] ; _TEB
	mov eax, [eax + 30h] ; _PEB
	mov eax, [eax + 0ch] ; _PEB_LDR_DATA
	mov ebx, [eax + 0ch] ; InLoadOrderModuleList
	; struct _LIST_ENTRY
	;	*Flink ptr size;
	;	*Blink ptr size;
	push ebx ; first flink
	; _LDR_DATA_TABLE_ENTRY
	; +0x02c BaseDllName
	; +0x018 DllBase
parse_ldr:
	lea ecx, [ebx + 2ch]
	mov cx, [ecx] ; Length
	cmp len, cx
	jnz next_flink
	mov ecx, [ebx + 2ch + 4h] ; *Buffer
	push len
	push ecx
	push dllname
	call strcmp
	test eax, eax
	jnz next_flink
	; when match dll
	mov eax, [ebx + 18h]
	jmp epilo
next_flink:
	mov ebx, [ebx]
	cmp ebx, flink
	jnz parse_ldr ; check reach end of flink
	mov eax, 0 ; found nothing
	; epilogue
epilo:
	mov esp, ebp
	pop ebx
	pop ecx
	pop ebp
	ret
walkPEB endp

walkEAT proc
; break ebx, ecx
stored_walkEAT equ dword * 2
dllbase equ [ebp - dword]
funcname equ [ebp - dword * 2]
namelen equ [ebp - dword * 3]
eatbase equ [ebp - dword * 4]
funcsize equ [ebp - dword * 5]
	; stdcall cc
	; proto *dllbase, *funcname, namelen
	; ret funcaddress
	push ebp ; store previous frame pointer
	push ebx
	push ecx
	mov ebp, esp ; open new frame
	mov eax, [ebp + dword * 2 + stored_walkEAT] ; get dllbase
	push eax
	mov eax, [ebp + dword * 3 + stored_walkEAT] ; get funcname
	push eax
	mov eax, [ebp + dword * 4 + stored_walkEAT] ; get funcname length
	push eax
	; Try access EAT
	mov ebx, [dllbase] ; store dllbase
	mov eax, DWORD PTR [ebx + 3ch] ; get IMAGE_NT_HEADERS RVA
	lea eax, [ebx + eax] ; access IMAGE_NT_HEADERS
	lea eax, [eax + 18h] ; access IMAGE_OPTIONAL_HEADER
	; IMAGE_OPTIONAL_HEADER->DataDirectory offset diff from x86/x64
	; x86 offset 0x60
	lea eax, [eax + 60h] ; access IMAGE_DATA_DIRECTORY[EAT]
	mov eax, DWORD PTR [eax] ; access VA
	lea eax, [ebx + eax] ; access BASE OFFSET EAT VA
	push eax ; store to eatbase
	; get entries number of func table
	mov ecx, [eax + dword * 6] ; _IMAGE_EXPORT_DIRECTORY->NumberOfFunctions
	push ecx ; funcsize
	mov ecx, 0
iterateNames:
	mov eax, [eatbase]
	cmp ecx, [funcsize]
	ja nofound ; over size means found nothing, just ret 0
	mov ebx, [namelen]
	push ebx ; push namelen for strcmp
	mov ebx, [funcname]
	push ebx ; push funcname for strcmp
	mov ebx, [dllbase]
	add ebx, [eax + dword * 8] ; _IMAGE_EXPORT_DIRECTORY->AddressOfNames
	mov ebx, [ebx + ecx * dword] ; pointer to name of table, get RVA
	add ebx, [dllbase] ; access name string
	push ebx
	call strcmp
	test eax, eax
	jz found ; ret 0 means funcname matched
	inc ecx
	jmp iterateNames
found:
	; access AddressOfFunctions by ecx
	mov eax, [eatbase]
	mov eax, [eax + dword * 9] ; get AddressOfNameOrdinals RVA
	mov ebx, [dllbase]
	add eax, ebx ; access AddressOfNameOrdinals
	mov cx, WORD PTR [eax + ecx * word] ; get Target Function Index
	mov eax, [eatbase]
	mov eax, [eax + dword * 7] ; get AddressOfFunctions RVA
	add eax, ebx ; access AddressOfFunctions
	mov eax, [eax + ecx * dword] ; get Target Function by Index
	add eax, ebx
	jmp epilo
nofound:
	mov eax, 0
epilo:
	mov esp, ebp ; close stack
	pop ecx
	pop ebx
	pop ebp ; restore previous frame pointer
	ret
walkEAT endp

; test unit
main proc
	lea eax, [msg]
	push eax
	call log
	push 3
	lea eax, [teststr2]
	push eax
	lea eax, [teststr1]
	push eax
	call strcmp
	push 28
	lea eax, [kerneldll]
	push eax
	call walkPEB
	push 12
	lea ebx, [loadLibraryA]
	push ebx
	push eax ; KERNELBASE.dll BaseAddr
	call walkEAT
	lea ebx, [dllsample]
	push ebx
	call eax
	lea eax, [msgend]
	push eax
	call log
	push 0
	call ExitProcess
	msg byte "MASM Code", 0Ah
	msgend byte "MASM Endz", 0Ah
	teststr1 byte "123"
	teststr2 byte "123"
	; 28 len
	kerneldll byte "K", 0h, "E", 0h, "R", 0h, "N", 0h, "E", 0h, "L", 0h, "B", 0h, "A", 0h, "S", 0h, "E", 0h, ".", 0h, "d", 0h, "l", 0h, "l", 0h
	loadLibraryA byte "LoadLibraryA"
	dllsample byte "DllSample.dll", 0h
main endp
end
