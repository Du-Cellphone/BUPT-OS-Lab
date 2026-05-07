[FORMAT "WCOFF"]
[INSTRSET "i486p"]
[BITS 32]
[FILE "api029.nas"]

		GLOBAL	_api_log2phy

[SECTION .text]

_api_log2phy:		; int api_log2phy(int logical_addr);
		MOV		EDX,29
		MOV		EAX,[ESP+4]
		INT		0x40
		RET
