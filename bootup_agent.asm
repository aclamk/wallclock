//.section .mysection
.text
.globl init_agent
.align 16
entry_point:
	//if called by injection from tracer then rax == 0xfee1fab
	//  and stack contains:
	//rsp + 8 + 128 ... rsp + 8 = scratch space
	//rsp + 8 = original rax
	//rsp:    = return address
	//
	//if called (testing) by directly invoking this, rax == 0
	//  and stack contains:
	//rsp:    = return address
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

	call init_agent

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
	cmp $0xfee1fab,%rax
	je 1f
	ret
1:
	mov 8(%rsp), %rax
	ret $128+8

