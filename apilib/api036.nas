[FORMAT "WCOFF"]
[INSTRSET "i486p"]
[BITS 32]
[FILE "api036.nas"]

		GLOBAL	_api_buffer_init

[SECTION .text]

_api_buffer_init:			; int api_buffer_init(void);
		MOV		EDX,36
		INT		0x40
		RET
