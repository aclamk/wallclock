
	.data
.globl R_init_agent
.globl R_create_sampling_context
.type agent_interface, STT_COMMON
.globl agent_interface
	agent_interface:
	.quad 0 ;//version
	.quad R_init_agent
	.quad R_create_sampling_context
	.quad R_print_peek
	.quad _wrapper
	.quad _wrapper_regs_provided
	.quad _wrapper_to_func

.text

.globl _get_backtrace
.globl _test_do_print

.align 16
.globl _wrapper
_wrapper:
.type _wrapper, STT_FUNC
.stabs "_wrapper:F1",36,0,0,_wrapper
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

	//arg1 - rdi, rip
	//arg2 - rsi, rbp
	//arg3 - rdx, rsp
	//arg4 - rcx, context*

	mov 8*17(%rsp), %rdi ;//this is rip
	mov 8*16(%rsp), %rsi ;//this is rbp
	mov %rsp, %rdx
	add $8*18+128, %rdx  ;//this is rsp
	mov 8*18(%rsp), %rcx ;//this is context
	call _get_backtrace@plt

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
	ret $128+8


.align 16
.globl _wrapper_regs_provided
_wrapper_regs_provided:
.stabs "wrapper:F1",36,0,0,_wrapper_regs_provided
.type _wrapper_regs_provided, STT_FUNC

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
	//arg1 - rdi, rip
	//arg2 - rsi, rbp
	//arg3 - rdx, rsp
	//arg4 - rcx, context*

	//call *%rax
	mov 8*(17+4)(%rsp), %rdi
	mov 8*(17+3)(%rsp), %rsi
	mov 8*(17+2)(%rsp), %rdx
	mov 8*(17+1)(%rsp), %rcx
	call _get_backtrace@plt
	//call _wrapper2

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
.globl _wrapper_to_func
.type _wrapper_to_func, STT_FUNC
_wrapper_to_func:
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
