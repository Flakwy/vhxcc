.text
Lcc_0_fib:
    li x30, 144
    sub x2, x2, x30
    sd x1, 136(x2)
    sw x5, 0(x2)
    li x5, 0
    lw x13, 0(x2)
    sxtw x13, x13
    li x14, 0
    li x15, 0
    li x30, 1
    mveq x15, x13, x14, x30
    la x30, Lcc_Lcc_0_fib_return_else_0
    mveq x31, x15, x0, x30
    li x13, 1
    sxtw x13, x13
    mv x5, x13
    la x30, Lcc_0_fib_return
    bl x30
Lcc_Lcc_0_fib_return_else_0:
    lw x13, 0(x2)
    sxtw x13, x13
    li x14, 1
    sub x15, x13, x14
    sd x15, 8(x2)
    ld x5, 8(x2)
    brl Lcc_0_fib
    mv x13, x5
    lw x14, 0(x2)
    sxtw x14, x14
    add x15, x13, x14
    sxtw x15, x15
    mv x5, x15
    la x30, Lcc_0_fib_return
    bl x30
Lcc_0_fib_return:
    ld x1, 136(x2)
    li x30, 144
    add x2, x2, x30
    brlr
.global main
main:
    li x30, 144
    sub x2, x2, x30
    sd x1, 128(x2)
    li x5, 0
    li x13, 0
    sd x13, 0(x2)
    ld x5, 0(x2)
    brl Lcc_0_fib
    mv x13, x5
    sd x13, 64(x2)
    brl test
    ld x13, 64(x2)
    mv x14, x5
    add x15, x13, x14
    sxtw x15, x15
    mv x5, x15
    la x30, main_return
    bl x30
main_return:
    ld x1, 128(x2)
    li x30, 144
    add x2, x2, x30
    brlr
