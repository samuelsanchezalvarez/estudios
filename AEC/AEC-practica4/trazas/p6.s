_a:	.space 256
_aa:	.space 16

_b:	.space 256

_y:	.space 8

_x:	.space 8

	.align 4
.global _main
_main:
	addi r5,r0,#0
	addi r6,r0,#63
	sgt	r1,r5,r6
	bnez r1,L11
	nop
	lhi r7,(_x>>16)&0xffff
	addui r7,r7,(_x&0xffff)
	lhi r9,(_a>>16)&0xffff
	addui r9,r9,(_a&0xffff)
	lhi r8,(_b>>16)&0xffff
	addui r8,r8,(_b&0xffff)
	addi r6,r0,#63
L5:
	slli r3,r5,#2
	add r4,r9,r3
	lf f4,0(r7)
	lf f5,0(r4)
	multf f4,f4,f5
	add r3,r8,r3
	lf f5,0(r3)
	addf f4,f4,f5
	sf 0(r7),f4
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
	lhi r7,(_y>>16)&0xffff
	addui r7,r7,(_y&0xffff)
	lhi r9,(_a>>16)&0xffff
	addui r9,r9,(_a&0xffff)
	lhi r8,(_b>>16)&0xffff
	addui r8,r8,(_b&0xffff)
	addi r6,r0,#63
L9:
	slli r3,r5,#2
	add r4,r9,r3
	lf f4,0(r7)
	lf f5,0(r4)
	multf f4,f4,f5
	add r3,r8,r3
	lf f5,0(r3)
	addf f4,f4,f5
	sf 0(r7),f4
	add r5,r5,#1
	sle	r1,r5,r6
	bnez r1,L9
	nop
L10:
	trap #0

