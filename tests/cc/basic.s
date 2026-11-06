.text
.global sum
sum:
    add x13, x5, x6
    addi x7, x13, 0
    addi x5, x7, 0
    brlr
.global wide_constant
wide_constant:
    addi x13, x0, 0
    adsi x13, 65535
    adsi x13, 4294901760
    adsi x13, 281470681743360
    adsi x13, -281474976710656
    addi x5, x13, 0
    brlr
Lcc_0_negate:
    sub x13, x0, x5
    addi x5, x13, 0
    brlr
