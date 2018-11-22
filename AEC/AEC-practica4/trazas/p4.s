
_a:	.space 16384


_valy:	.space 8

_valx:	.space 8

	.align 4
LC0:
	.float 0.000000000000
	.align 4
.global _main
_main:
	add r1,r0,LC0
	lf f5,0(r1)
	addi r6,r0,#0
	addi r3,r0,#63
	sgt	r1,r6,r3
	bnez r1,L11
	nop
	lhi r8,(_a>>16)&0xffff
	addui r8,r8,(_a&0xffff)
L9:
	addi r4,r0,#0
	addi r5,r0,#63
	sgt	r1,r4,r5
	bnez r1,L10
	nop
	slli r7,r6,#2
	addi r5,r0,#63
L8:
	slli r3,r4,#8
	add r3,r8,r3
	add r3,r3,r7
	lf f4,0(r3)
	addf f5,f5,f4
	add r4,r4,#1
	sle	r1,r4,r5
	bnez r1,L8
	nop
L10:
	add r6,r6,#1
	addi r3,r0,#63
	sle	r1,r6,r3
	bnez r1,L9
	nop
L11:
	lhi r3,(_valx>>16)&0xffff
	addui r3,r3,(_valx&0xffff)
	sf 0(r3),f5
	trap #0

