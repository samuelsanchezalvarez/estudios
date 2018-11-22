
_a:	.space 256
_aa:	.space 16
_b:	.space 256
_bb:	.space 16
_c:	.space 256

_valy:	.space 8

_valx:	.space 8
	.align 4
LC0:
	.float 0.000000000000
	.align 4
.global _main
_main:
	add r1,r0,LC0
	lf f6,0(r1)
	movf f7,f6
	addi r5,r0,#0
	addi r6,r0,#63
	sgt	r1,r5,r6
	bnez r1,L11
	nop
	lhi r8,(_a>>16)&0xffff
	addui r8,r8,(_a&0xffff)
	lhi r7,(_b>>16)&0xffff
	addui r7,r7,(_b&0xffff)
	addi r6,r0,#63
L5:
	slli r3,r5,#2
	add r4,r8,r3
	lf f4,0(r4)
	multf f4,f6,f4
	add r3,r7,r3
	lf f5,0(r3)
	addf f6,f4,f5
	add r5,r5,#1
	sle	r1,r5,r6
	bnez r1,L5
	nop
L11:
	addi r5,r0,#0
	addi r6,r0,#63
	sgt	r1,r5,r6
	bnez r1,L10
	nop
	lhi r8,(_a>>16)&0xffff
	addui r8,r8,(_a&0xffff)
	lhi r7,(_c>>16)&0xffff
	addui r7,r7,(_c&0xffff)
	addi r6,r0,#63
L9:
	slli r3,r5,#2
	add r4,r8,r3
	lf f4,0(r4)
	multf f4,f7,f4
	add r3,r7,r3
	lf f5,0(r3)
	addf f7,f4,f5
	add r5,r5,#1
	sle	r1,r5,r6
	bnez r1,L9
	nop
L10:
	lhi r3,(_valx>>16)&0xffff
	addui r3,r3,(_valx&0xffff)
	sf 0(r3),f6
	lhi r3,(_valy>>16)&0xffff
	addui r3,r3,(_valy&0xffff)
	sf 0(r3),f7

	trap #0

