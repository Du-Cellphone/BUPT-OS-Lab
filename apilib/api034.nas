[FORMAT "WCOFF"]
[INSTRSET "i486p"]
[BITS 32]
[FILE "api034.nas"]

		GLOBAL	_api_sem_wait

[SECTION .text]

_api_sem_wait:			; void api_sem_wait(void *sem);
		MOV		EDX,34
		MOV		EAX,[ESP+4]			; sem
		INT		0x40
		RET
