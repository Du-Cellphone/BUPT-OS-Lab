[FORMAT "WCOFF"]
[INSTRSET "i486p"]
[BITS 32]
[FILE "api033.nas"]

		GLOBAL	_api_sem_init

[SECTION .text]

_api_sem_init:			; void api_sem_init(void *sem, int initial, int max);
		MOV		EDX,33
		MOV		EAX,[ESP+4]			; sem
		MOV		EBX,[ESP+8]			; initial
		MOV		ECX,[ESP+12]		; max
		INT		0x40
		RET
