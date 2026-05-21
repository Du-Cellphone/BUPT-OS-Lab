[FORMAT "WCOFF"]
[INSTRSET "i486p"]
[BITS 32]
[FILE "api030.nas"]

		GLOBAL	_api_mutex_init

[SECTION .text]

_api_mutex_init:			; void api_mutex_init(void *m);
		MOV		EDX,30
		MOV		EAX,[ESP+4]			; m
		INT		0x40
		RET
