
.data

.type agent_interface, STT_COMMON
.globl agent_interface
agent_interface:
	.ascii "AGENTAPI" ;//sanity-check marker
	.quad _init_agent
	.quad _wc_inject

.text
.align 16
.globl _wc_inject
.type _wc_inject, STT_FUNC
_wc_inject:
	nop
	nop
	push %rbp
	push %rax
	pushf
	push %rbx
	push %rcx
	push %rdx

	push %rbp
	push %rsi
	push %rdi

	push %r8
	push %r9
	push %r10
	push %r11

	push %r12
	push %r13
	push %r14
	push %r15

	//arg1 - func to call
	//arg2 - rdi, arg1 of func
	//arg3 - rsi, arg2 of func
	//arg4 - rdx, arg3 of func
	mov 8*(17+4)(%rsp), %rax
	mov 8*(17+3)(%rsp), %rdi
	mov 8*(17+2)(%rsp), %rsi
	mov 8*(17+1)(%rsp), %rdx
	call *%rax

	pop %r15
	pop %r14
	pop %r13
	pop %r12

	pop %r11
	pop %r10
	pop %r9
	pop %r8

	pop %rdi
	pop %rsi
	pop %rbp

	pop %rdx
	pop %rcx
	pop %rbx
	popf
	pop %rax
	pop %rbp
	ret $128+(8*4)



.align 16
.globl _remote_return
_remote_return:
	mov $-1, %rax
	syscall
	ret
