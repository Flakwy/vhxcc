# VHX ISA Encoding
Opcode(Func) | Mneumonic | Behaviour | Extension
-|-|-|-
00(0)|swp rd, rm rn|t = rm; rm = rn; rd = t|`i`
00(1)|add rd, rm, rn|rd = rm + rn|`i`
00(2)|sub rd, rm, rn|rd = rm - rn|`i`
00(3)|or rd, rm, rn|rd = rm \| rn  |`i`
00(4)|and rd, rm, rn|rd = rm & rn  |`i`
00(5)|xor rd, rm, rn|rd = rm ^ rn  |`i`
00(6)|sll rd, rm, rn|rd = rm << rn|`i`
00(7)|srl rd, rm, rn|rd = rm >> rn|`i`
00(8)|sra rd, rm, rn|rd = (signed)rm >> rn|`i`
00(9)|addi rd, rm, i12|rd = rm + i12  |`i`
00(A)|ori rd, rm, i12| rd = rm \| i12|`i`
00(B)|andi rd, rm, i12| rd = rm & i12|`i`
00(C)|xori rd, rm, i12|rd = rm ^ i12|`i`
00(D)|slli rd, rm, u7|rd = rm << u7|`i`,(shimm masked to XLEN shift width)
00(E)|srli rd, rm, u7|rd = rm >> u7|`i`,(shimm masked to XLEN shift width)
00(F)|srai rd, rm, u7| rd = (signed)rm >> u7|`i`,(shimm masked to XLEN shift width)
01(0)|lb rd, i12(rm)|rd = *(u8)(rm+i12)|`i`
01(1)|sb rd, i12(rm)|*(u8)(rm+i12) = rd|`i`
02(0)|lh rd, i12(rm)|rd = *(u16)(rm+i12)|`i`
02(1)|sh rd, i12(rm)|*(u16)(rm+i12) = rd|`i`
03(0)|lw rd, i12(rm)|rd = *(u32)(rm+i12)|`i`
03(1)|sw rd, i12(rm)|*(u32)(rm+i12) = rd|`i`
04(0)|ld rd, i12(rm)|rd = *(u64)(rm+i12)|`i`,`vhx64`,`vhx128`
04(1)|sd rd, i12(rm)|*(u64)(rm+i12) = rd|`i`,`vhx64`,`vhx128`
05(0)|lq rd, i12(rm)|rd = *(u128)(rm+i12)|`i`,`vhx128`
05(1)|sq rd, i12(rm)|*(u128)(rm+i12) = rd |`i`,`vhx128`
06(0)|zxtb rd, rm|rd = (u8)rm|`i`
06(1)|zxth rd, rm|rd = (u16)rm|`i`
06(2)|zxtw rd, rm|rd = (u32)rm|`i`
06(3)|zxtd rd, rm|rd = (u64)rm|`i`
06(4)|sxtb rd, rm|rd = (i8)rm|`i`
06(5)|sxth rd, rm|rd = (i16)rm|`i`
06(6)|sxtw rd, rm|rd = (i32)rm|`i`,(nop on `)
06(7)|sxtd rd, rm|rd = (i64)rm|`i`,
07(0)|mveq rd, rn, rl, rm|rd = rn == rl ? rm : rd|`i`
07(1)|mvne rd, rn, rl, rm|rd = rn != rl ? rm : rd|`i`
07(2)|mvlt rd, rn, rl, rm|rd = (signed)rn \< (signed)rl ? rm : rd|`i`
07(3)|mvle rd, rn, rl, rm|rd = (signed)rn \<= (signed)rl ? rm : rd|`i`
07(4)|mvlo rd, rn, rl, rm|rd = rn \< rl ? rm : rd|`i`
07(5)|mvls rd, rn, rl, rm|rd = rn \<= rl ? rm : rd|`i`
07(6)|mvtz rd, rn, rl, rm|rd = (rn & rl) == 0 ? rm : rd|`i`
07(7)|mvnt rd, rn, rl, rm|rd = (rn & rl) 1= 0 ? rm : rd|`i`
08(0)|addieq rd, rn, rl, u8|rd = rn == rl ? rd + u8 + 1: rd|`i`
08(1)|addine rd, rn, rl, u8|rd = rn != rl ? rd + u8 + 1: rd|`i`
08(2)|addilt rd, rn, rl, u8|rd = (signed)rn \< (signed)rl ? rd + u8 + 1: rd|`i`
08(3)|addile rd, rn, rl, u8|rd = (signed)rn \<= (signed)rl ? rd + u8 + 1: rd|`i`
08(4)|addilo rd, rn, rl, u8|rd = rn \< rl ? rd + u8 + 1: rd|`i`
08(5)|addils rd, rn, rl, u8|rd = rn \<= rl ? rd + u8 + 1: rd|`i`
08(6)|additz rd, rn, rl, u8|rd = (rn & rl) == 0 ? rd + u8 + 1: rd|`i`
08(7)|addint rd, rn, rl, u8|rd = (rn & rl) 1= 0 ? rd + u8 + 1: rd|`i`
09(0)|subieq rd, rn, rl, u8|rd = rn == rl ? rd + u8 - 1: rd|`i`
09(1)|subine rd, rn, rl, u8|rd = rn != rl ? rd + u8 - 1: rd|`i`
09(2)|subilt rd, rn, rl, u8|rd = (signed)rn \< (signed)rl ? rd + u8 - 1: rd|`i`
09(3)|subile rd, rn, rl, u8|rd = (signed)rn \<= (signed)rl ? rd + u8 - 1: rd|`i`
09(4)|subilo rd, rn, rl, u8|rd = rn \< rl ? rd + u8 - 1: rd|`i`
09(5)|subils rd, rn, rl, u8|rd = rn \<= rl ? rd + u8 - 1: rd|`i`
09(6)|subitz rd, rn, rl, u8|rd = (rn & rl) == 0 ? rd + u8 - 1: rd|`i`
09(7)|subint rd, rn, rl, u8|rd = (rn & rl) 1= 0 ? rd + u8 - 1: rd|`i`
0A(0)|lwmr rd, mr, rm|t = mr; mr = rm; rd = t|`i`,`zicsr`
0A(1)|lsmr rd, mr, rm|t = mr; mr \|= rm; rd = t|`i`,`zicsr`
0A(2)|lcmr rd, mr, rm|t = mr; mr &= ~rm; rd = t|`i`,`zicsr`
0A(3)|lxmr rd, mr, rm|t = mr; mr ^= rm; rd = t|`i`,`zicsr`
0A(4)|lwmri rd, mr, i5|t = mr; mr = i5; rd = t|`i`,`zicsr`
0A(5)|lsmri rd, mr, i5|t = mr; mr \|= i5; rd = t|`i`,`zicsr`
0A(6)|lcmri rd, mr, i5|t = mr; mr &= ~i5; rd = t|`i`,`zicsr`
0A(7)|lxmri rd, mr, i5|t = mr; mr ^= i5; rd = t|`i`,`zicsr`
0A(8)|wfi|wait for interrupt|`i`,`zicsr`
0A(9)|epcr|return from exception|`s`,`zicsr`
0A(A)|sfwi|software interrupt|`s`,`zicsr`
0A(B)|brkp|software breakpoint|`s`,`zicsr`
0B|adsi rd, imm|rd += u16 \<\< (u4 * 4)|`i`
0C(0)|mul rd, rm, rn, rl|r = rn * rl; rm = hi(r); rd = lo(r)|`m`
0C(1)|muls rd, rm, rn, rl|r = (signed)rn * rl; rm = hi(r); rd = lo(r)|`m`
0C(2)|mulsu rd, rm, rn, rl|r = (signed)rn * (signed)rl; rm = hi(r); rd = lo(r)|`m`
0C(3)|div rd, rm, rn, rl|rm = rn % rl; rd = rn / rl|`m`
0C(4)|div rd, rm, rn, rl|rm = (signed)rn % (signed)rl; rd = (signed)rn / (signed)rl|`m`

Register conflicts are for multi-write instructions will over the `rm` result with the `rd` result.
---

# Register Aliases
Register|Aliases|Description
-|-|-
x0|zer|Hardwired to zero
x1|ra|Return address (Caller Saved)
x2|sp|Stack pointer (Callee Saved)
x3|gp|Global pointer
x4|tp|Thread pointer
x5|a0|Argument 0/Return value (Caller Saved)
x6|a1|Argument 1/Return value (Caller Saved)
x7|a2|Argument 2 (Caller Saved)
x8|a3|Argument 3 (Caller Saved)
x9|a4|Argument 4 (Caller Saved)
x10|a5|Argument 5 (Caller Saved)
x11|a6|Argument 6 (Caller Saved)
x12|a7|Argument 7 (Caller Saved)
x13|t0|Temporary Register (Caller Saved)
x14|t1|Temporary Register (Caller Saved)
x15|t2|Temporary Register (Caller Saved)
x16|t3|Temporary Register (Caller Saved)
x17|t4|Temporary Register (Caller Saved)
x18|t5|Temporary Register (Caller Saved)
x19|t6|Temporary Register (Caller Saved)
x20|t7|Temporary Register (Caller Saved)
x21|s0|Saved Register (Callee Saved)
x22|s1|Saved Register (Callee Saved)
x23|s2|Saved Register (Callee Saved)
x24|s3|Saved Register (Callee Saved)
x25|s4|Saved Register (Callee Saved)
x26|s5|Saved Register (Callee Saved)
x27|s6|Saved Register (Callee Saved)
x28|s7|Saved Register (Callee Saved)
x29|s8|Saved Register (Callee Saved)
x30|t8, lp|Temporary Register/Link Pointer (Caller Saved)
x31|pc|Program Counter
---

## Memory Instructions
Each `l*` and `s*` instruction supports pre and post increment/decrementing of the base register.
The size of increment/decrement matches the size of the accessed memory
```vhxasm
strcpy:
    mv  t0, a0
0:  lb  t1, 0(a1++)
    sb  t1, 0(t0++)
    brnez t1, 0b
    brlr
```
---

# VHX Pseudo Ops
Mneumonic|Expansion
-|-
mv rd, rm|addi rd, rm, 0
nop|addi zer, zer, 0
li rd, imm|(syntax available, but implementations not yet finished)
la rd, label|(syntax available, but implementations not yet finished)
bl rd|mv pc, rd (name subject to change)
br label|la lp, label; mv pc, lp
br label|addi pc, pc, pc_rel(label)
breq rn, rl, label|la lp, label; mveq pc, rn, rl, lp
brne rn, rl, label|la lp, label; mvne pc, rn, rl, lp
brlt rn, rl, label|la lp, label; mvlt pc, rn, rl, lp
brle rn, rl, label|la lp, label; mvle pc, rn, rl, lp
brgt rn, rl, label|la lp, label; mvlt pc, rl, rn, lp
brge rn, rl, label|la lp, label; mvle pc, rn, rl, lp
brlo rn, rl, label|la lp, label; mvlo pc, rn, rl, lp
brls rn, rl, label|la lp, label; mvls pc, rn, rl, lp
brhi rn, rl, label|la lp, label; mvlo pc, rl, rn, lp
brhs rn, rl, label|la lp, label; mvls pc, rl, rn, lp
brtz rn, rl, label|la lp, label; mvtz pc, rn, rl, lp
brnt rn, rl, label|la lp, label; mvnt pc, rn, rl, lp
breqz rn, label|la lp, label; mveq pc, rn, zer, lp
brnez rn, label|la lp, label; mvne pc, rn, zer, lp
brltz rn, label|la lp, label; mvlt pc, rn, zer, lp
brlez rn, label|la lp, label; mvle pc, rn, zer, lp
brlr|mv pc, ra (name subject to change to ret)
brl label|la ra, label; swp pc, ra, pc (loads the return address into ra)
---