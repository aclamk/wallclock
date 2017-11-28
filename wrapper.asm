	.data
.globl _funkcja
_funkcja:	
	.quad 0


#define ORC_REG_SP			5
#define ORC_TYPE_CALL			0


.macro UNWIND_HINT sp_reg=ORC_REG_SP sp_offset=0 type=ORC_TYPE_CALL
#ifdef CONFIG_STACK_VALIDATION
.Lunwind_hint_ip_\@:
	.pushsection .discard.unwind_hints
		/* struct unwind_hint */
		.long .Lunwind_hint_ip_\@ - .
		.short \sp_offset
		.byte \sp_reg
		.byte \type
	.popsection
#endif
.endm

.macro UNWIND_HINT_FUNC sp_offset=8
	UNWIND_HINT sp_offset=\sp_offset
.endm


	.text
.globl _my_backtrace


.align 16
.globl _wrapper2
_wrapper2:
	push %rbp
	mov %rsp,%rbp
	call my_backtrace
	pop %rbp
	ret

.align 16
.globl _wrapper
_wrapper:
.stabs "wrapper:F1",36,0,0,_wrapper
	push %rbp
	//mov %rsp,%rbp
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
	//arg1 - rdi
	//arg2 - rsi
	//arg3 - rdx
	//call *%rax

	mov 8*17(%rsp), %rdi ;//this is rip
	mov 8*16(%rsp), %rsi ;//this is rbp
	mov %rsp, %rdx
	add $8*18+128, %rdx       ;//this is rsp

	call my_backtrace
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
	ret $128
	add $160,%rsp
	jmp *-160(%rsp)
	//pop %rax
	ret


.align 16
.globl _wrapper_regs_provided
_wrapper_regs_provided:
.stabs "wrapper:F1",36,0,0,_wrapper_regs_provided

	push %rbp
	//mov %rsp,%rbp
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
	//call *%rax
	mov 8*(17+3)(%rsp), %rdi
	mov 8*(17+2)(%rsp), %rsi
	mov 8*(17+1)(%rsp), %rdx

	call my_backtrace
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
	ret $128+(8*3)
	add $160,%rsp
	jmp *-160(%rsp)
	//pop %rax
	ret

	

	.text
.align 16
.globl _rax_test
_rax_test:
	mov $0,%rax
	mov $0,%rcx
	1:
	inc %rax
	inc %rcx
	dec %rdi
	jnz 1b
	sub %rcx,%rax
	ret

