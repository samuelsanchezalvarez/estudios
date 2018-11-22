; z = a + x + y
; Tamaño de los vectores: 16 palabras
; Vector x
   .data
x: .word 0,1,2,3,4,5,6,7,8,9
   .word 10,11,12,13,14,15

; Vector y
y: .word 100,100,100,100,100,100,100,100
   .word 100,100,100,100,100,100,100,100

; Vector z
; 16 elementos son 64 bytes.
z: .space 64

; escalar a
a: .word -10

; El código
   .text

start:
   add r1,r0,x
   add r4,r1,#64 ; 16*4
   add r2,r0,y
   add r3,r0,z
   lw r10,a(r0)

loop:
   lw r12,0(r1)
   add r12,r10,r12
   lw r14,0(r2)
   add r14,r12,r14
   sw 0(r3),r14
   add r1,r1,#4
   add r2,r2,#4
   add r3,r3,#4
   seq r5,r4,r1
   beqz r5,loop
   trap #0   ; Fin de programa
