.globl call_start
.data
L0:
	.asciz "A=1"
L1:
	.asciz "program"

.text
.align 16


call_start:
	//arg3 - rdx, auxv
	//arg4 - rcx, auxv_size
	sub %rcx, %rsp
	add %rcx, %rdx
1:
	sub $8, %rdx
	mov (%rdx), %rax
	pushq %rax
	sub $8, %rcx
	jnz 1b

	mov %rdi, %rax
	mov %rsi, %rdx

	pushq $0
	lea    L0(%rip),%rcx
	pushq %rcx
//	pushq $L0
	pushq $0
	lea    L1(%rip),%rcx
	pushq %rcx
//	pushq $L1
	pushq $1
	jmp *%rax

