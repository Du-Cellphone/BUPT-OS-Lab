[FORMAT "WCOFF"]
[INSTRSET "i486p"]
[BITS 32]
[FILE "api038.nas"]

		GLOBAL	_api_consume

[SECTION .text]

_api_consume:			; int api_consume(void);
		MOV		EDX,38
		INT		0x40
		RET
