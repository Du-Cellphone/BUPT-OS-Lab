[FORMAT "WCOFF"]
[INSTRSET "i486p"]
[BITS 32]
[FILE "api035.nas"]

		GLOBAL	_api_sem_signal

[SECTION .text]

_api_sem_signal:			; void api_sem_signal(void *sem);
		MOV		EDX,35
		MOV		EAX,[ESP+4]			; sem
		INT		0x40
		RET
