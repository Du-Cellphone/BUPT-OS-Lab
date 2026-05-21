[FORMAT "WCOFF"]
[INSTRSET "i486p"]
[BITS 32]
[FILE "api031.nas"]

		GLOBAL	_api_mutex_lock

[SECTION .text]

_api_mutex_lock:			; void api_mutex_lock(void *m);
		MOV		EDX,31
		MOV		EAX,[ESP+4]			; m
		INT		0x40
		RET
