[FORMAT "WCOFF"]
[INSTRSET "i486p"]
[BITS 32]
[FILE "api037.nas"]

		GLOBAL	_api_produce

[SECTION .text]

_api_produce:			; void api_produce(int item);
		MOV		EDX,37
		MOV		EAX,[ESP+4]			; item
		INT		0x40
		RET
