//.section .mysection
.text
.globl init_agent
.align 16
entry_point:
	jmp init_agent

.globl raw_syscall
.text
.align 16
raw_syscall:
	mov    %rdi,%rax
	mov    %rsi,%rdi
	mov    %rdx,%rsi
	mov    %rcx,%rdx
	mov    %r8,%r10
	mov    %r9,%r8
	mov    0x8(%rsp),%r9
	syscall
	ret
