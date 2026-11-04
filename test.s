    .file   "test.c"
    .align  1
    .globl  test
test:
    addi    sp,sp,-16
    sd  ra, 8(sp)
    sd  s0, 0(sp)
    addi    s0,sp,16
    lui a5, hi(a)
    lbu a5,lo(a)(a5)
    addiw   a5,a5,-3
    andi    a4,a5,0xff
    lui a5,hi(a)
    sb  a4,lo(a)(a5)
    nop
    mv  a0,a5
    ld  ra,8(sp)
    ld  s0,0(sp)
    addi    sp,sp,16
    jr  ra
