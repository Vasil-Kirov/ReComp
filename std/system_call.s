

section .text
	global _system_call

_system_call:

	mov rax, rdi
	mov rdi, rsi
	mov rsi, rdx
	mov rdx, rcx
	mov r10, r8
	mov r8, r9
	pop r9
	syscall

	ret

