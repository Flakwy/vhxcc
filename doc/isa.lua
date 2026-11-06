-- OLD ASSEMBLER ISA REFERENCE

VCOMP.RegisterTable = {}

for i = 0, 31 do
    VCOMP.RegisterTable["x"..i] = i
end
VCOMP.RegisterTable.zer = 0
VCOMP.RegisterTable.ra = 1
VCOMP.RegisterTable.sp = 2
VCOMP.RegisterTable.gp = 3
VCOMP.RegisterTable.tp = 4
for i = 5, 12 do
    VCOMP.RegisterTable["a"..(i-5)] = i
end
for i = 13, 20 do
    VCOMP.RegisterTable["t"..(i-13)] = i
end
for i = 21, 29 do
    VCOMP.RegisterTable["s"..(i-21)] = i
end
VCOMP.RegisterTable.lp = 30
VCOMP.RegisterTable.pc = 31

VCOMP.MachineRegisterTable = {}

VCOMP.MachineRegisterTable.casr = 0x200
VCOMP.MachineRegisterTable.epc  = 0x201
VCOMP.MachineRegisterTable.ecsr = 0x202
VCOMP.MachineRegisterTable.evtp = 0x203
VCOMP.MachineRegisterTable.ipm  = 0x210
VCOMP.MachineRegisterTable.ipm0  = 0x210 + 0
VCOMP.MachineRegisterTable.ipm1  = 0x210 + 1
VCOMP.MachineRegisterTable.ipm2  = 0x210 + 2
VCOMP.MachineRegisterTable.ipm3  = 0x210 + 3
VCOMP.MachineRegisterTable.ipm4  = 0x210 + 4
VCOMP.MachineRegisterTable.ipm5  = 0x210 + 5
VCOMP.MachineRegisterTable.ipm6  = 0x210 + 6
VCOMP.MachineRegisterTable.ipm7  = 0x210 + 7
VCOMP.MachineRegisterTable.ipm8  = 0x210 + 8
VCOMP.MachineRegisterTable.ipm9  = 0x210 + 9
VCOMP.MachineRegisterTable.ipm10  = 0x210 + 10
VCOMP.MachineRegisterTable.ipm11  = 0x210 + 11
VCOMP.MachineRegisterTable.ipm12  = 0x210 + 12
VCOMP.MachineRegisterTable.ipm13  = 0x210 + 13
VCOMP.MachineRegisterTable.ipm14  = 0x210 + 14
VCOMP.MachineRegisterTable.ipm15  = 0x210 + 15
VCOMP.MachineRegisterTable.iem  = 0x220
VCOMP.MachineRegisterTable.iem0  = 0x220 + 0
VCOMP.MachineRegisterTable.iem1  = 0x220 + 1
VCOMP.MachineRegisterTable.iem2  = 0x220 + 2
VCOMP.MachineRegisterTable.iem3  = 0x220 + 3
VCOMP.MachineRegisterTable.iem4  = 0x220 + 4
VCOMP.MachineRegisterTable.iem5  = 0x220 + 5
VCOMP.MachineRegisterTable.iem6  = 0x220 + 6
VCOMP.MachineRegisterTable.iem7  = 0x220 + 7
VCOMP.MachineRegisterTable.iem8  = 0x220 + 8
VCOMP.MachineRegisterTable.iem9  = 0x220 + 9
VCOMP.MachineRegisterTable.iem10  = 0x220 + 10
VCOMP.MachineRegisterTable.iem11  = 0x220 + 11
VCOMP.MachineRegisterTable.iem12  = 0x220 + 12
VCOMP.MachineRegisterTable.iem13  = 0x220 + 13
VCOMP.MachineRegisterTable.iem14  = 0x220 + 14
VCOMP.MachineRegisterTable.iem15  = 0x220 + 15
VCOMP.MachineRegisterTable.tbr  = 0x240
VCOMP.MachineRegisterTable.tscr = 0x241
VCOMP.MachineRegisterTable.tctx = 0x242
VCOMP.MachineRegisterTable.cpid = 0x400
VCOMP.MachineRegisterTable.cidp = 0x401
VCOMP.MachineRegisterTable.vatp = 0x402
VCOMP.MachineRegisterTable.hwclk = 0x410
VCOMP.MachineRegisterTable.swclk = 0x412
VCOMP.MachineRegisterTable.hwtmr = 0x420
VCOMP.MachineRegisterTable.swtmr = 0x422

local function pack32(x)
    return string.pack("<I4", x)
end

local opcodes = {}

local function alu0_encode(info, operands)
    return pack32(
        (info.opc6 << 0) |
        (VCOMP.RegisterTable[operands[1]] << 6) |
        (VCOMP.RegisterTable[operands[2]] << 11) |
        (info.func4 << 16) |
        (VCOMP.RegisterTable[operands[3]] << 20)
    )
end

local function mv_encode(info, operands)
    return pack32(
        (info.opc6 << 0) |
        (VCOMP.RegisterTable[operands[1]] << 6) |
        (VCOMP.RegisterTable[operands[2]] << 11) |
        (info.func4 << 16) |
        (0 << 20)
    )
end

local function bl_encode(info, operands)
    return pack32(
        (info.opc6 << 0) |
        (31 << 6) |
        (VCOMP.RegisterTable[operands[1]] << 11) |
        (info.func4 << 16) |
        (0 << 20)
    )
end

local function nop_encode(info, operands)
    return pack32(
        (info.opc6 << 0) |
        (0 << 6) |
        (0 << 11) |
        (info.func4 << 16) |
        (0 << 20)
    )
end

local function ret_encode(info, operands)
    return pack32(
        (info.opc6 << 0) |
        (31 << 6) |
        (1 << 11) |
        (info.func4 << 16) |
        (0 << 20)
    )
end

opcodes.swp = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register"},
    opc6 = 0x0,
    func4 = 0x0,
    encode = alu0_encode
}

opcodes.add = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register"},
    opc6 = 0x0,
    func4 = 0x1,
    encode = alu0_encode
}

opcodes.mv = {
    size = 4,
    extension = "i",
    operands = {"register", "register"},
    opc6 = 0x0,
    func4 = 0x1,
    encode = mv_encode
}

opcodes.bl = {
    size = 4,
    extension = "i",
    operands = {"register"},
    opc6 = 0x0,
    func4 = 0x1,
    encode = bl_encode
}

opcodes.nop = {
    size = 4,
    extension = "i",
    operands = {"register", "register"},
    opc6 = 0x0,
    func4 = 0x1,
    encode = nop_encode
}

opcodes.brlr = {
    size = 4,
    extension = "i",
    operands = {},
    opc6 = 0x0,
    func4 = 0x1,
    encode = ret_encode
}

opcodes.sub = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register"},
    opc6 = 0x0,
    func4 = 0x2,
    encode = alu0_encode
}

opcodes["or"] = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register"},
    opc6 = 0x0,
    func4 = 0x3,
    encode = alu0_encode
}

opcodes["and"] = {
size = 4,
    extension = "i",
    operands = {"register", "register", "register"},
    opc6 = 0x0,
    func4 = 0x4,
    encode = alu0_encode
}

opcodes.xor = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register"},
    opc6 = 0x0,
    func4 = 0x5,
    encode = alu0_encode
}

opcodes.sll = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register"},
    opc6 = 0x0,
    func4 = 0x6,
    encode = alu0_encode
}

opcodes.srl = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register"},
    opc6 = 0x0,
    func4 = 0x7,
    encode = alu0_encode
}

opcodes.sra = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register"},
    opc6 = 0x0,
    func4 = 0x8,
    encode = alu0_encode
}

local function alu1_encode(info, operands)
    return pack32(
        (info.opc6 << 0) |
        (VCOMP.RegisterTable[operands[1]] << 6) |
        (VCOMP.RegisterTable[operands[2]] << 11) |
        (info.func4 << 16) |
        ((tonumber(operands[3]) & 0xFFF) << 20)
    )
end

opcodes.addi = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "immediate"},
    opc6 = 0x0,
    func4 = 0x9,
    encode = alu1_encode
}

opcodes.ori = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "immediate"},
    opc6 = 0x0,
    func4 = 0xa,
    encode = alu1_encode
}

opcodes.andi = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "immediate"},
    opc6 = 0x0,
    func4 = 0xb,
    encode = alu1_encode
}

opcodes.xori = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "immediate"},
    opc6 = 0x0,
    func4 = 0xc,
    encode = alu1_encode
}

opcodes.slli = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "immediate"},
    opc6 = 0x0,
    func4 = 0xd,
    encode = alu1_encode
}

opcodes.srli = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "immediate"},
    opc6 = 0x0,
    func4 = 0xe,
    encode = alu1_encode
}

opcodes.srai = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "immediate"},
    opc6 = 0x0,
    func4 = 0xf,
    encode = alu1_encode
}

local function mem_encode(info, operands)
    return pack32(
        (info.opc6 << 0) |
        (VCOMP.RegisterTable[operands[1]] << 6) |
        (VCOMP.RegisterTable[operands[2].register] << 11) |
        (info.store << 16) |
        ((operands[2].update and 1 or 0) << 17) |
        ((operands[2].update or 0) << 18) |
        (((tonumber(operands[2].offset)) & 0xFFF) << 20)
    )
end

opcodes.lb = {
    size = 4,
    extension = "i",
    operands = {"register", "memory"},
    opc6 = 0x1,
    store = 0,
    encode = mem_encode
}

opcodes.sb = {
    size = 4,
    extension = "i",
    operands = {"register", "memory"},
    opc6 = 0x1,
    store = 1,
    encode = mem_encode
}

opcodes.lh = {
    size = 4,
    extension = "i",
    operands = {"register", "memory"},
    opc6 = 0x2,
    store = 0,
    encode = mem_encode
}

opcodes.sh = {
    size = 4,
    extension = "i",
    operands = {"register", "memory"},
    opc6 = 0x2,
    store = 1,
    encode = mem_encode
}

opcodes.lw = {
    size = 4,
    extension = "i",
    operands = {"register", "memory"},
    opc6 = 0x3,
    store = 0,
    encode = mem_encode
}

opcodes.sw = {
    size = 4,
    extension = "i",
    operands = {"register", "memory"},
    opc6 = 0x3,
    store = 1,
    encode = mem_encode
}

opcodes.ld = {
    size = 4,
    extension = "i",
    operands = {"register", "memory"},
    opc6 = 0x4,
    store = 0,
    encode = mem_encode
}

opcodes.sd = {
    size = 4,
    extension = "i",
    operands = {"register", "memory"},
    opc6 = 0x4,
    store = 1,
    encode = mem_encode
}

opcodes.lq = {
    size = 4,
    extension = "i",
    operands = {"register", "memory"},
    opc6 = 0x5,
    store = 0,
    encode = mem_encode
}

opcodes.sq = {
    size = 4,
    extension = "i",
    operands = {"register", "memory"},
    opc6 = 0x5,
    store = 1,
    encode = mem_encode
}

local function ext_encode(info, operands)
    return pack32(
        (info.opc6 << 0) |
        (VCOMP.RegisterTable[operands[1]] << 6) |
        (VCOMP.RegisterTable[operands[2]] << 11) |
        (info.signed << 17) |
        (info.width << 18)
    )
end

opcodes.sxtb = {
    size = 4,
    extension = "i",
    operands = {"register", "register"},
    opc6 = 0x6,
    signed = 1,
    width = 0,
    encode = ext_encode
}

opcodes.sxth = {
    size = 4,
    extension = "i",
    operands = {"register", "register"},
    opc6 = 0x6,
    signed = 1,
    width = 1,
    encode = ext_encode
}

opcodes.sxtw = {
    size = 4,
    extension = "i",
    operands = {"register", "register"},
    opc6 = 0x6,
    signed = 1,
    width = 2,
    encode = ext_encode
}

opcodes.sxtd = {
    size = 4,
    extension = "i",
    operands = {"register", "register"},
    opc6 = 0x6,
    signed = 1,
    width = 3,
    encode = ext_encode
}

opcodes.zxtb = {
    size = 4,
    extension = "i",
    operands = {"register", "register"},
    opc6 = 0x6,
    signed = 0,
    width = 0,
    encode = ext_encode
}

opcodes.zxth = {
    size = 4,
    extension = "i",
    operands = {"register", "register"},
    opc6 = 0x6,
    signed = 0,
    width = 1,
    encode = ext_encode
}

opcodes.zxtw = {
    size = 4,
    extension = "i",
    operands = {"register", "register"},
    opc6 = 0x6,
    signed = 0,
    width = 2,
    encode = ext_encode
}

opcodes.zxtd = {
    size = 4,
    extension = "i",
    operands = {"register", "register"},
    opc6 = 0x6,
    signed = 0,
    width = 3,
    encode = ext_encode
}

    -- 0 | equal
    -- 1 | not equal
    -- 2 | less than
    -- 3 | less than or equal
    -- 4 | lower than
    -- 5 | lower than or equal
    -- 6 | masked zero
    -- 7 | masked not zero

local function ccop_encode(info, operands)
    return pack32(
        (info.opc6 << 0) |
        (VCOMP.RegisterTable[operands[1]] << 6) |
        (VCOMP.RegisterTable[operands[4]] << 11) |
        (info.func3 << 16) |
        (VCOMP.RegisterTable[operands[2]] << 20) |
        (VCOMP.RegisterTable[operands[3]] << 25)
    )
end

local function nccop_encode(info, operands)
    return pack32(
        (info.opc6 << 0) |
        (VCOMP.RegisterTable[operands[1]] << 6) |
        (VCOMP.RegisterTable[operands[4]] << 11) |
        (info.func3 << 16) |
        (VCOMP.RegisterTable[operands[3]] << 20) |
        (VCOMP.RegisterTable[operands[2]] << 25)
    )
end

opcodes.mveq = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register", "register"},
    opc6 = 0x7,
    func3 = 0x0,
    encode = ccop_encode
}

opcodes.mvne = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register", "register"},
    opc6 = 0x7,
    func3 = 0x1,
    encode = ccop_encode
}

opcodes.mvlt = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register", "register"},
    opc6 = 0x7,
    func3 = 0x2,
    encode = ccop_encode
}

opcodes.mvle = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register", "register"},
    opc6 = 0x7,
    func3 = 0x3,
    encode = ccop_encode
}

opcodes.mvgt = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register", "register"},
    opc6 = 0x7,
    func3 = 0x3,
    encode = nccop_encode
}

opcodes.mvge = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register", "register"},
    opc6 = 0x7,
    func3 = 0x2,
    encode = nccop_encode
}

opcodes.mvlo = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register", "register"},
    opc6 = 0x7,
    func3 = 0x4,
    encode = ccop_encode
}

opcodes.mvls = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register", "register"},
    opc6 = 0x7,
    func3 = 0x5,
    encode = ccop_encode
}

opcodes.mvhi = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register", "register"},
    opc6 = 0x7,
    func3 = 0x5,
    encode = nccop_encode
}

opcodes.mvhs = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register", "register"},
    opc6 = 0x7,
    func3 = 0x4,
    encode = nccop_encode
}

opcodes.mvtz = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register", "register"},
    opc6 = 0x7,
    func3 = 0x6,
    encode = ccop_encode
}

opcodes.mvnt = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register", "register"},
    opc6 = 0x7,
    func3 = 0x7,
    encode = ccop_encode
}

local function ccopimm_encode(info, operands)
    local opc6 = info.opc6
    if tonumber(operands[4]) < 0 then
        operands[4] = -tonumber(operands[4])
        opc6 = (opc6 == 0x8) and 0x9 or 0x8
    end
    return pack32(
        (opc6 << 0) |
        (VCOMP.RegisterTable[operands[1]] << 6) |
        (((tonumber(operands[4]) - 1) & 0x1f) << 11) |
        (info.func3 << 16) |
        ((((tonumber(operands[4]) - 1) >> 7) & 0x1) << 19) |
        (VCOMP.RegisterTable[operands[2]] << 20) |
        (VCOMP.RegisterTable[operands[3]] << 25) |
        ((((tonumber(operands[4]) - 1) >> 5) & 0x3) << 30)
    )
end

local function nccopimm_encode(info, operands)
    local opc6 = info.opc6
    if tonumber(operands[4]) < 0 then
        operands[4] = -tonumber(operands[4])
        opc6 = (opc6 == 0x8) and 0x9 or 0x8
    end
    return pack32(
        (opc6 << 0) |
        (VCOMP.RegisterTable[operands[1]] << 6) |
        (((tonumber(operands[4]) - 1) & 0x1f) << 11) |
        (info.func3 << 16) |
        ((((tonumber(operands[4]) - 1) >> 7) & 0x1) << 19) |
        (VCOMP.RegisterTable[operands[3]] << 20) |
        (VCOMP.RegisterTable[operands[2]] << 25) |
        ((((tonumber(operands[4]) - 1) >> 5) & 0x3) << 30)
    )
end

opcodes.addieq = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register", "immediate"},
    opc6 = 0x8,
    func3 = 0x0,
    encode = ccopimm_encode
}

opcodes.addine = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register", "immediate"},
    opc6 = 0x8,
    func3 = 0x1,
    encode = ccopimm_encode
}

opcodes.addilt = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register", "immediate"},
    opc6 = 0x8,
    func3 = 0x2,
    encode = ccopimm_encode
}

opcodes.addile = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register", "immediate"},
    opc6 = 0x8,
    func3 = 0x3,
    encode = ccopimm_encode
}

opcodes.addigt = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register", "immediate"},
    opc6 = 0x8,
    func3 = 0x3,
    encode = nccopimm_encode
}

opcodes.addige = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register", "immediate"},
    opc6 = 0x8,
    func3 = 0x2,
    encode = nccopimm_encode
}

opcodes.addilo = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register", "immediate"},
    opc6 = 0x8,
    func3 = 0x4,
    encode = ccopimm_encode
}

opcodes.addils = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register", "immediate"},
    opc6 = 0x8,
    func3 = 0x5,
    encode = ccopimm_encode
}

opcodes.addihi = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register", "immediate"},
    opc6 = 0x8,
    func3 = 0x5,
    encode = nccopimm_encode
}

opcodes.addihs = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register", "immediate"},
    opc6 = 0x8,
    func3 = 0x4,
    encode = nccopimm_encode
}

opcodes.additz = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register", "immediate"},
    opc6 = 0x8,
    func3 = 0x6,
    encode = ccopimm_encode
}

opcodes.addint = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register", "immediate"},
    opc6 = 0x8,
    func3 = 0x7,
    encode = ccopimm_encode
}

opcodes.subieq = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register", "immediate"},
    opc6 = 0x9,
    func3 = 0x0,
    encode = ccopimm_encode
}

opcodes.subine = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register", "immediate"},
    opc6 = 0x9,
    func3 = 0x1,
    encode = ccopimm_encode
}

opcodes.subilt = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register", "immediate"},
    opc6 = 0x9,
    func3 = 0x2,
    encode = ccopimm_encode
}

opcodes.subile = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register", "immediate"},
    opc6 = 0x9,
    func3 = 0x3,
    encode = ccopimm_encode
}

opcodes.subigt = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register", "immediate"},
    opc6 = 0x9,
    func3 = 0x3,
    encode = nccopimm_encode
}

opcodes.subige = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register", "immediate"},
    opc6 = 0x9,
    func3 = 0x2,
    encode = nccopimm_encode
}

opcodes.subilo = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register", "immediate"},
    opc6 = 0x9,
    func3 = 0x4,
    encode = ccopimm_encode
}

opcodes.subils = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register", "immediate"},
    opc6 = 0x9,
    func3 = 0x5,
    encode = ccopimm_encode
}

opcodes.subihi = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register", "immediate"},
    opc6 = 0x9,
    func3 = 0x5,
    encode = nccopimm_encode
}

opcodes.subihs = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register", "immediate"},
    opc6 = 0x9,
    func3 = 0x4,
    encode = nccopimm_encode
}

opcodes.subitz = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register", "immediate"},
    opc6 = 0x9,
    func3 = 0x6,
    encode = ccopimm_encode
}

opcodes.subint = {
    size = 4,
    extension = "i",
    operands = {"register", "register", "register", "immediate"},
    opc6 = 0x9,
    func3 = 0x7,
    encode = ccopimm_encode
}

local function lwmr_encode(info, operands)
    return pack32(
        (info.opc6 << 0) |
        (VCOMP.RegisterTable[operands[1]] << 6) |
        (VCOMP.RegisterTable[operands[3]] << 11) |
        (info.func4 << 16) |
        ((tonumber(operands[2]) & 0xFFF) << 20)
    )
end
local function lwmri_encode(info, operands)
    return pack32(
        (info.opc6 << 0) |
        (VCOMP.RegisterTable[operands[1]] << 6) |
        ((tonumber(operands[3]) & 0x1F) << 11) |
        (info.func4 << 16) |
        ((tonumber(operands[2]) & 0xFFF) << 20)
    )
end


local function lmr_encode(info, operands)
    return pack32(
        (info.opc6 << 0) |
        (VCOMP.RegisterTable[operands[1]] << 6) |
        (0 << 11) |
        (info.func4 << 16) |
        ((tonumber(operands[2]) & 0xFFF) << 20)
    )
end
local function wmr_encode(info, operands)
    return pack32(
        (info.opc6 << 0) |
        (0 << 6) |
        (VCOMP.RegisterTable[operands[2]] << 11) |
        (info.func4 << 16) |
        ((tonumber(operands[1]) & 0xFFF) << 20)
    )
end
local function wmri_encode(info, operands)
    return pack32(
        (info.opc6 << 0) |
        (0 << 6) |
        ((tonumber(operands[2]) & 0x1F) << 11) |
        (info.func4 << 16) |
        ((tonumber(operands[1]) & 0xFFF) << 20)
    )
end

opcodes.lwmr = {
    size = 4,
    extension = "i",
    operands = {"register", "machine register", "register"},
    opc6 = 0xa,
    func4 = 0x0,
    encode = lwmr_encode
}

opcodes.lsmr = {
    size = 4,
    extension = "i",
    operands = {"register", "machine register", "register"},
    opc6 = 0xa,
    func4 = 0x1,
    encode = lwmr_encode
}

opcodes.lcmr = {
    size = 4,
    extension = "i",
    operands = {"register", "machine register", "register"},
    opc6 = 0xa,
    func4 = 0x2,
    encode = lwmr_encode
}

opcodes.lxmr = {
    size = 4,
    extension = "i",
    operands = {"register", "machine register", "register"},
    opc6 = 0xa,
    func4 = 0x3,
    encode = lwmr_encode
}

opcodes.lwmri = {
    size = 4,
    extension = "i",
    operands = {"register", "machine register", "immediate"},
    opc6 = 0xa,
    func4 = 0x4,
    encode = lwmri_encode
}

opcodes.lsmri = {
    size = 4,
    extension = "i",
    operands = {"register", "machine register", "immediate"},
    opc6 = 0xa,
    func4 = 0x5,
    encode = lwmri_encode
}

opcodes.lcmri = {
    size = 4,
    extension = "i",
    operands = {"register", "machine register", "immediate"},
    opc6 = 0xa,
    func4 = 0x6,
    encode = lwmri_encode
}

opcodes.lxmri = {
    size = 4,
    extension = "i",
    operands = {"register", "machine register", "immediate"},
    opc6 = 0xa,
    func4 = 0x7,
    encode = lwmri_encode
}

opcodes.lmr = {
    size = 4,
    extension = "i",
    operands = {"register", "machine register"},
    opc6 = 0xa,
    func4 = 0x0,
    encode = lmr_encode
}

opcodes.wmr = {
    size = 4,
    extension = "i",
    operands = {"machine register", "register"},
    opc6 = 0xa,
    func4 = 0x0,
    encode = wmr_encode
}

opcodes.smr = {
    size = 4,
    extension = "i",
    operands = {"machine register", "register"},
    opc6 = 0xa,
    func4 = 0x1,
    encode = wmr_encode
}

opcodes.cmr = {
    size = 4,
    extension = "i",
    operands = {"machine register", "register"},
    opc6 = 0xa,
    func4 = 0x2,
    encode = wmr_encode
}

opcodes.xmr = {
    size = 4,
    extension = "i",
    operands = {"machine register", "register"},
    opc6 = 0xa,
    func4 = 0x3,
    encode = wmr_encode
}

opcodes.wmri = {
    size = 4,
    extension = "i",
    operands = {"machine register", "immediate"},
    opc6 = 0xa,
    func4 = 0x4,
    encode = wmri_encode
}

opcodes.smri = {
    size = 4,
    extension = "i",
    operands = {"machine register", "immediate"},
    opc6 = 0xa,
    func4 = 0x5,
    encode = wmri_encode
}

opcodes.cmri = {
    size = 4,
    extension = "i",
    operands = {"machine register", "immediate"},
    opc6 = 0xa,
    func4 = 0x6,
    encode = wmri_encode
}

opcodes.xmri = {
    size = 4,
    extension = "i",
    operands = {"machine register", "immediate"},
    opc6 = 0xa,
    func4 = 0x7,
    encode = wmri_encode
}

local function special_encode(info, operands)
    return pack32(
        (info.opc6 << 0) |
        (0 << 6) |
        (0 << 11) |
        (info.func4 << 16) |
        (0 << 20)
    )
end

opcodes.wfi = {
    size = 4,
    extension = "i",
    operands = {},
    opc6 = 0xa,
    func4 = 0x8,
    encode = special_encode
}

opcodes.epcr = {
    size = 4,
    extension = "i",
    operands = {},
    opc6 = 0xa,
    func4 = 0x9,
    encode = special_encode
}

opcodes.swfi = {
    size = 4,
    extension = "i",
    operands = {},
    opc6 = 0xa,
    func4 = 0xa,
    encode = special_encode
}

opcodes.brkp = {
    size = 4,
    extension = "i",
    operands = {},
    opc6 = 0xa,
    func4 = 0xb,
    encode = special_encode
}

local function adsi_encode(info, operands)
    return pack32(
        (info.opc6 << 0) |
        (VCOMP.RegisterTable[operands[1]] << 6)
    )
end

opcodes.adsi = {
    size = 4,
    extension = "i",
    operands = {"register", "immediate"},
    opc6 = 0xb,
    encode = adsi_encode
}

local function mul_encode(info, operands)
    return pack32(
        (info.opc6 << 0) |
        (VCOMP.RegisterTable[operands[1]] << 6) |
        (VCOMP.RegisterTable[operands[2]] << 11) |
        (info.func3 << 16) |
        (VCOMP.RegisterTable[operands[3]] << 20) |
        (VCOMP.RegisterTable[operands[4]] << 25)
    )
end

opcodes.mul = {
    size = 4,
    extension = "m",
    operands = {"register", "register", "register", "register"},
    opc6 = 0xc,
    func3 = 0x0,
    encode = mul_encode
}

opcodes.muls = {
    size = 4,
    extension = "m",
    operands = {"register", "register", "register", "register"},
    opc6 = 0xc,
    func3 = 0x1,
    encode = mul_encode
}

opcodes.mulsu = {
    size = 4,
    extension = "m",
    operands = {"register", "register", "register", "register"},
    opc6 = 0xc,
    func3 = 0x2,
    encode = mul_encode
}

opcodes.div = {
    size = 4,
    extension = "m",
    operands = {"register", "register", "register", "register"},
    opc6 = 0xc,
    func3 = 0x3,
    encode = mul_encode
}

opcodes.divs = {
    size = 4,
    extension = "m",
    operands = {"register", "register", "register", "register"},
    opc6 = 0xc,
    func3 = 0x4,
    encode = mul_encode
}

VCOMP.OpcodeTable = {}
for mneumonic, info in pairs(opcodes) do
    VCOMP.OpcodeTable[mneumonic] = info
end

local update_modes = {}
update_modes["nil"] = nil
update_modes["pre_increment"] = 0
update_modes["pre_decrement"] = 1
update_modes["post_increment"] = 2
update_modes["post_decrement"] = 3
function VCOMP.Encode(ast)
    local operands = {}
    for i = 1, #ast.branches do
        local node = ast.branches[i]
        if node.type == "constant" or node.type == "register" then
            operands[i] = node.value
        elseif node.type == "memory" then
            local operand = {}
            operand.offset = node.branches[1].value
            operand.register = node.branches[2].value
            operand.update = update_modes[node.mode or "nil"]
            operands[i] = operand
        elseif node.type == "machine register" then
            operands[i] = VCOMP.MachineRegisterTable[node.value]
        end
    end
    return ast.info.encode(ast.info, operands)
end