#include "riscv.hpp"

#include <array>
#include <cctype>
#include <string>

namespace riscv {
namespace {

constexpr uint32_t kOpLui = 0x37;
constexpr uint32_t kOpAuipc = 0x17;
constexpr uint32_t kOpJal = 0x6F;
constexpr uint32_t kOpJalr = 0x67;
constexpr uint32_t kOpBranch = 0x63;
constexpr uint32_t kOpLoad = 0x03;
constexpr uint32_t kOpStore = 0x23;
constexpr uint32_t kOpImm = 0x13;
constexpr uint32_t kOpReg = 0x33;
constexpr uint32_t kOpFence = 0x0F;
constexpr uint32_t kOpSystem = 0x73;

// Extrai o campo [high:low] da palavra.
uint32_t bits(uint32_t word, int high, int low) {
    const int width = high - low + 1;
    const uint32_t mask = (width == 32) ? 0xFFFFFFFFu : ((1u << width) - 1u);
    return (word >> low) & mask;
}

Instruction invalid(uint32_t word, uint32_t pc, std::string reason) {
    Instruction inst;
    inst.pc = pc;
    inst.raw = word;
    inst.format = Format::Invalid;
    inst.cls = Class::Invalid;
    inst.error = std::move(reason);
    return inst;
}

std::string hexByte(uint32_t value) {
    static const char* kDigits = "0123456789abcdef";
    std::string out = "0x";
    out += kDigits[(value >> 4) & 0xF];
    out += kDigits[value & 0xF];
    return out;
}

// --- Decodificacao por formato --------------------------------------------

Instruction decodeR(uint32_t word, uint32_t pc) {
    Instruction inst;
    inst.pc = pc;
    inst.raw = word;
    inst.format = Format::R;
    inst.rd = static_cast<uint8_t>(bits(word, 11, 7));
    inst.funct3 = static_cast<uint8_t>(bits(word, 14, 12));
    inst.rs1 = static_cast<uint8_t>(bits(word, 19, 15));
    inst.rs2 = static_cast<uint8_t>(bits(word, 24, 20));
    inst.funct7 = static_cast<uint8_t>(bits(word, 31, 25));

    const uint32_t f3 = *inst.funct3;
    const uint32_t f7 = *inst.funct7;

    if (f7 == 0x01) {  // extensao M (mul/div)
        inst.cls = Class::MulDiv;
        switch (f3) {
            case 0x0: inst.mnemonic = "mul"; break;
            case 0x1: inst.mnemonic = "mulh"; break;
            case 0x2: inst.mnemonic = "mulhsu"; break;
            case 0x3: inst.mnemonic = "mulhu"; break;
            case 0x4: inst.mnemonic = "div"; break;
            case 0x5: inst.mnemonic = "divu"; break;
            case 0x6: inst.mnemonic = "rem"; break;
            default: inst.mnemonic = "remu"; break;
        }
        return inst;
    }

    inst.cls = Class::Alu;
    switch (f3) {
        case 0x0:
            if (f7 == 0x00) inst.mnemonic = "add";
            else if (f7 == 0x20) inst.mnemonic = "sub";
            break;
        case 0x1:
            if (f7 == 0x00) inst.mnemonic = "sll";
            break;
        case 0x2:
            if (f7 == 0x00) inst.mnemonic = "slt";
            break;
        case 0x3:
            if (f7 == 0x00) inst.mnemonic = "sltu";
            break;
        case 0x4:
            if (f7 == 0x00) inst.mnemonic = "xor";
            break;
        case 0x5:
            // O bit 30 (funct7 = 0x20) e o unico que separa o deslocamento
            // logico do aritmetico.
            if (f7 == 0x00) inst.mnemonic = "srl";
            else if (f7 == 0x20) inst.mnemonic = "sra";
            break;
        case 0x6:
            if (f7 == 0x00) inst.mnemonic = "or";
            break;
        default:
            if (f7 == 0x00) inst.mnemonic = "and";
            break;
    }

    if (inst.mnemonic.empty()) {
        return invalid(word, pc, "combinacao funct3/funct7 desconhecida no opcode 0x33");
    }
    return inst;
}

Instruction decodeLoad(uint32_t word, uint32_t pc) {
    Instruction inst;
    inst.pc = pc;
    inst.raw = word;
    inst.format = Format::I;
    inst.cls = Class::Load;
    inst.rd = static_cast<uint8_t>(bits(word, 11, 7));
    inst.funct3 = static_cast<uint8_t>(bits(word, 14, 12));
    inst.rs1 = static_cast<uint8_t>(bits(word, 19, 15));
    inst.imm = immediateI(word);

    switch (*inst.funct3) {
        case 0x0: inst.mnemonic = "lb"; break;
        case 0x1: inst.mnemonic = "lh"; break;
        case 0x2: inst.mnemonic = "lw"; break;
        case 0x4: inst.mnemonic = "lbu"; break;
        case 0x5: inst.mnemonic = "lhu"; break;
        default: return invalid(word, pc, "funct3 invalido para load (opcode 0x03)");
    }
    return inst;
}

Instruction decodeOpImm(uint32_t word, uint32_t pc) {
    Instruction inst;
    inst.pc = pc;
    inst.raw = word;
    inst.format = Format::I;
    inst.cls = Class::Alu;
    inst.rd = static_cast<uint8_t>(bits(word, 11, 7));
    inst.funct3 = static_cast<uint8_t>(bits(word, 14, 12));
    inst.rs1 = static_cast<uint8_t>(bits(word, 19, 15));
    inst.imm = immediateI(word);

    const uint32_t f3 = *inst.funct3;
    const uint32_t shiftFunct7 = bits(word, 31, 25);
    const uint32_t shamt = bits(word, 24, 20);

    switch (f3) {
        case 0x0: inst.mnemonic = "addi"; break;
        case 0x2: inst.mnemonic = "slti"; break;
        case 0x3: inst.mnemonic = "sltiu"; break;
        case 0x4: inst.mnemonic = "xori"; break;
        case 0x6: inst.mnemonic = "ori"; break;
        case 0x7: inst.mnemonic = "andi"; break;
        case 0x1:
        case 0x5:
            // Nos deslocamentos imediatos o campo de 12 bits e, na verdade,
            // funct7 (7 bits) + shamt (5 bits): o imediato exibido e o shamt.
            if (f3 == 0x1 && shiftFunct7 == 0x00) inst.mnemonic = "slli";
            else if (f3 == 0x5 && shiftFunct7 == 0x00) inst.mnemonic = "srli";
            else if (f3 == 0x5 && shiftFunct7 == 0x20) inst.mnemonic = "srai";
            else return invalid(word, pc, "funct7 invalido em deslocamento imediato");
            inst.funct7 = static_cast<uint8_t>(shiftFunct7);
            inst.imm = static_cast<int32_t>(shamt);
            break;
        default:
            return invalid(word, pc, "funct3 invalido para opcode 0x13");
    }
    return inst;
}

Instruction decodeJalr(uint32_t word, uint32_t pc) {
    if (bits(word, 14, 12) != 0) return invalid(word, pc, "funct3 invalido para jalr");
    Instruction inst;
    inst.pc = pc;
    inst.raw = word;
    inst.format = Format::I;
    inst.cls = Class::Jump;
    inst.mnemonic = "jalr";
    inst.rd = static_cast<uint8_t>(bits(word, 11, 7));
    inst.funct3 = 0;
    inst.rs1 = static_cast<uint8_t>(bits(word, 19, 15));
    inst.imm = immediateI(word);
    return inst;
}

Instruction decodeStore(uint32_t word, uint32_t pc) {
    Instruction inst;
    inst.pc = pc;
    inst.raw = word;
    inst.format = Format::S;
    inst.cls = Class::Store;
    // Tipo S nao possui rd: os bits 11-7 fazem parte do imediato.
    inst.funct3 = static_cast<uint8_t>(bits(word, 14, 12));
    inst.rs1 = static_cast<uint8_t>(bits(word, 19, 15));
    inst.rs2 = static_cast<uint8_t>(bits(word, 24, 20));
    inst.imm = immediateS(word);

    switch (*inst.funct3) {
        case 0x0: inst.mnemonic = "sb"; break;
        case 0x1: inst.mnemonic = "sh"; break;
        case 0x2: inst.mnemonic = "sw"; break;
        default: return invalid(word, pc, "funct3 invalido para store (opcode 0x23)");
    }
    return inst;
}

Instruction decodeBranch(uint32_t word, uint32_t pc) {
    Instruction inst;
    inst.pc = pc;
    inst.raw = word;
    inst.format = Format::B;
    inst.cls = Class::Branch;
    // Tipo B nao possui rd: os bits 11-7 carregam imm[4:1|11].
    inst.funct3 = static_cast<uint8_t>(bits(word, 14, 12));
    inst.rs1 = static_cast<uint8_t>(bits(word, 19, 15));
    inst.rs2 = static_cast<uint8_t>(bits(word, 24, 20));
    inst.imm = immediateB(word);

    switch (*inst.funct3) {
        case 0x0: inst.mnemonic = "beq"; break;
        case 0x1: inst.mnemonic = "bne"; break;
        case 0x4: inst.mnemonic = "blt"; break;
        case 0x5: inst.mnemonic = "bge"; break;
        case 0x6: inst.mnemonic = "bltu"; break;
        case 0x7: inst.mnemonic = "bgeu"; break;
        default: return invalid(word, pc, "funct3 invalido para desvio (opcode 0x63)");
    }
    return inst;
}

Instruction decodeUpper(uint32_t word, uint32_t pc, bool isLui) {
    Instruction inst;
    inst.pc = pc;
    inst.raw = word;
    inst.format = Format::U;
    inst.cls = Class::Upper;
    inst.mnemonic = isLui ? "lui" : "auipc";
    // Tipo U so tem rd e imediato: nao existem rs1, rs2 nem funct3.
    inst.rd = static_cast<uint8_t>(bits(word, 11, 7));
    inst.imm = immediateU(word);
    return inst;
}

Instruction decodeJal(uint32_t word, uint32_t pc) {
    Instruction inst;
    inst.pc = pc;
    inst.raw = word;
    inst.format = Format::J;
    inst.cls = Class::Jump;
    inst.mnemonic = "jal";
    inst.rd = static_cast<uint8_t>(bits(word, 11, 7));
    inst.imm = immediateJ(word);
    return inst;
}

Instruction decodeSystem(uint32_t word, uint32_t pc) {
    Instruction inst;
    inst.pc = pc;
    inst.raw = word;
    inst.format = Format::I;
    inst.cls = Class::System;
    inst.funct3 = static_cast<uint8_t>(bits(word, 14, 12));

    if (*inst.funct3 == 0x0) {
        if (bits(word, 19, 7) != 0) return invalid(word, pc, "campos nao nulos em ecall/ebreak");
        const uint32_t imm = bits(word, 31, 20);
        if (imm == 0) inst.mnemonic = "ecall";
        else if (imm == 1) inst.mnemonic = "ebreak";
        else return invalid(word, pc, "instrucao de sistema desconhecida");
        // ecall/ebreak nao usam registradores: os campos ficam vazios para nao
        // gerar dependencia falsa na analise de hazards da Etapa 2.
        return inst;
    }

    // Acesso a CSR: o imediato e o numero do registrador de controle (sem sinal).
    switch (*inst.funct3) {
        case 0x1: inst.mnemonic = "csrrw"; break;
        case 0x2: inst.mnemonic = "csrrs"; break;
        case 0x3: inst.mnemonic = "csrrc"; break;
        case 0x5: inst.mnemonic = "csrrwi"; break;
        case 0x6: inst.mnemonic = "csrrsi"; break;
        case 0x7: inst.mnemonic = "csrrci"; break;
        default: return invalid(word, pc, "funct3 invalido para opcode 0x73");
    }
    inst.rd = static_cast<uint8_t>(bits(word, 11, 7));
    inst.imm = static_cast<int32_t>(bits(word, 31, 20));
    // As variantes "i" usam um imediato de 5 bits no lugar de rs1.
    if (*inst.funct3 < 0x5) inst.rs1 = static_cast<uint8_t>(bits(word, 19, 15));
    return inst;
}

Instruction decodeFence(uint32_t word, uint32_t pc) {
    const uint32_t f3 = bits(word, 14, 12);
    if (f3 != 0x0 && f3 != 0x1) return invalid(word, pc, "funct3 invalido para opcode 0x0F");
    Instruction inst;
    inst.pc = pc;
    inst.raw = word;
    inst.format = Format::I;
    inst.cls = Class::System;
    inst.funct3 = static_cast<uint8_t>(f3);
    inst.mnemonic = (f3 == 0) ? "fence" : "fence.i";
    return inst;
}

}  // namespace

int32_t signExtend(uint32_t value, int width) {
    const uint32_t signBit = 1u << (width - 1);
    const uint32_t mask = (width == 32) ? 0xFFFFFFFFu : ((1u << width) - 1u);
    value &= mask;
    if (value & signBit) value |= ~mask;
    return static_cast<int32_t>(value);
}

int32_t immediateI(uint32_t word) { return signExtend(bits(word, 31, 20), 12); }

int32_t immediateS(uint32_t word) {
    const uint32_t imm = (bits(word, 31, 25) << 5) | bits(word, 11, 7);
    return signExtend(imm, 12);
}

int32_t immediateB(uint32_t word) {
    // imm[12] = bit 31 | imm[11] = bit 7 | imm[10:5] = bits 30-25 | imm[4:1] = bits 11-8.
    const uint32_t imm = (bits(word, 31, 31) << 12) | (bits(word, 7, 7) << 11) |
                         (bits(word, 30, 25) << 5) | (bits(word, 11, 8) << 1);
    return signExtend(imm, 13);
}

int32_t immediateU(uint32_t word) {
    // Os 20 bits ja ocupam a parte alta da palavra: o valor tem 32 bits.
    return static_cast<int32_t>(word & 0xFFFFF000u);
}

int32_t immediateJ(uint32_t word) {
    // imm[20] = bit 31 | imm[19:12] = bits 19-12 | imm[11] = bit 20 | imm[10:1] = bits 30-21.
    const uint32_t imm = (bits(word, 31, 31) << 20) | (bits(word, 19, 12) << 12) |
                         (bits(word, 20, 20) << 11) | (bits(word, 30, 21) << 1);
    return signExtend(imm, 21);
}

Instruction decode(uint32_t word, uint32_t pc) {
    const uint32_t opcode = bits(word, 6, 0);
    switch (opcode) {
        case kOpReg: return decodeR(word, pc);
        case kOpImm: return decodeOpImm(word, pc);
        case kOpLoad: return decodeLoad(word, pc);
        case kOpJalr: return decodeJalr(word, pc);
        case kOpStore: return decodeStore(word, pc);
        case kOpBranch: return decodeBranch(word, pc);
        case kOpLui: return decodeUpper(word, pc, true);
        case kOpAuipc: return decodeUpper(word, pc, false);
        case kOpJal: return decodeJal(word, pc);
        case kOpSystem: return decodeSystem(word, pc);
        case kOpFence: return decodeFence(word, pc);
        default: return invalid(word, pc, "opcode desconhecido " + hexByte(opcode));
    }
}

// --- Metadados e ganchos da estrutura Instruction --------------------------

const char* toString(Format f) {
    switch (f) {
        case Format::R: return "R";
        case Format::I: return "I";
        case Format::S: return "S";
        case Format::B: return "B";
        case Format::U: return "U";
        case Format::J: return "J";
        case Format::Invalid: return "-";
    }
    return "-";
}

const char* toString(Class c) {
    switch (c) {
        case Class::Alu: return "alu";
        case Class::MulDiv: return "muldiv";
        case Class::Load: return "load";
        case Class::Store: return "store";
        case Class::Branch: return "branch";
        case Class::Jump: return "jump";
        case Class::Upper: return "upper";
        case Class::System: return "system";
        case Class::Invalid: return "invalida";
    }
    return "invalida";
}

std::optional<Class> classFromString(const std::string& name) {
    std::string key;
    for (char ch : name) key += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

    static const std::array<std::pair<const char*, Class>, 8> kTable{{
        {"alu", Class::Alu},
        {"muldiv", Class::MulDiv},
        {"load", Class::Load},
        {"store", Class::Store},
        {"branch", Class::Branch},
        {"jump", Class::Jump},
        {"upper", Class::Upper},
        {"system", Class::System},
    }};
    for (const auto& [text, value] : kTable) {
        if (key == text) return value;
    }
    return std::nullopt;
}

std::vector<uint8_t> Instruction::readsRegisters() const {
    std::vector<uint8_t> regs;
    if (rs1 && *rs1 != 0) regs.push_back(*rs1);
    if (rs2 && *rs2 != 0) regs.push_back(*rs2);
    return regs;
}

std::optional<uint8_t> Instruction::writesRegister() const {
    if (!rd || *rd == 0) return std::nullopt;
    return rd;
}

std::optional<uint32_t> Instruction::branchTarget() const {
    if (!imm) return std::nullopt;
    if (format != Format::B && format != Format::J) return std::nullopt;
    return static_cast<uint32_t>(pc + static_cast<uint32_t>(*imm));
}

}  // namespace riscv
