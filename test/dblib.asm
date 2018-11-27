SECTION .data
db 0

SECTION .text
global print_int
global _print_int

; Print a 64-bit integer to the console
; rdi: the number to print
print_int:
_print_int:
	push	rsi
	push	r8

	; Allocate space
	mov	rax, rdi
	mov	r8, 10
	mov	rsi, rsp

	; Parse int (do { ... } while (rax != 0))
	mov	dl, 10	; '\n' == 10, but nasm doesn't handle '\n' specially.
	mov	[rsi], dl
.loop:
	dec	rsi

	xor	rdx, rdx
	idiv	r8

	add	dl, '0'
	mov	[rsi], dl

	cmp	rax, 0
	jnz	.loop


	; Print int
	mov	rdx, rsp	; parameter 1
	sub	rdx, rsi
	inc	rdx
	mov	rdi, 1	; stdout
	mov	rax, 0x2000004	; write
	syscall

	pop	r8
	pop	rsi
	ret	
