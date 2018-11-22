.data
        ; z = a + y
        ; Tamaño de los vectores: 64 palabras
        ; Vector y
y:      .word 0,1,2,3,4,5,6,7,8,9
        .word 10,11,12,13,14,15,16,17,18,19
        .word 20,21,22,23,24,25,26,27,28,29
        .word 30,31,32,33,34,35,36,37,38,39
        .word 40,41,42,43,44,45,46,47,48,49
        .word 50,51,52,53,54,55,56,57,58,59
        .word 60,61,62,63

        ; Vector z
	;   64 elementos son 256 bytes
z:      .space 256

        ; Escalar a
a:      .word 1

        ; El código
        .align 4
	.text
start:
        add r1,r0,y     ; r1 contiene la direccion de y
        add r2,r0,z     ; r2 contiene la direccion de z
        lw r3,a(r0)     ; r3 contiene a
        add r10,r1,#256 ; 64 elementos son 256 bytes
loop:
        lw r4,0(r1)
        add r5,r3,r4
        sw 0(r2),r5
        add r1,r1,#4
        add r2,r2,#4
        sub r11,r10,r1
        bnez r11,loop
        nop             ; "delay slot"

        trap #0         ; Fin de programa



        


