[FORMAT "WCOFF"]
[INSTRSET "i486p"]
[BITS 32]
[FILE "api032.nas"]

		GLOBAL	_api_mutex_unlock

[SECTION .text]

_api_mutex_unlock:			; void api_mutex_unlock(void *m);
		MOV		EDX,32
		MOV		EAX,[ESP+4]			; m
		INT		0x40
		RET
