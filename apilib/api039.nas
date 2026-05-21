[FORMAT "WCOFF"]
[INSTRSET "i486p"]
[BITS 32]
[FILE "api039.nas"]

		GLOBAL	_api_yield

[SECTION .text]

_api_yield:			; void api_yield(void);
		MOV		EDX,39
		INT		0x40
		RET
