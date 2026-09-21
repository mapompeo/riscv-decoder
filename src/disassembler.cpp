#include "riscv.hpp"

#include <array>
#include <sstream>

namespace riscv {
namespace {

const std::array<const char*, 32> kAbiNames{
    "zero", "ra", "sp", "gp", "tp",  "t0",  "t1", "t2", "s0", "s1", "a0",
    "a1",   "a2", "a3", "a4", "a5",  "a6",  "a7", "s2", "s3", "s4", "s5",
    "s6",   "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"};

std::string hex32(uint32_t value) {
    std::ostringstream os;
    os << "0x" << std::hex << std::uppercase;
    os.width(8);
    os.fill('0');
    os << value;
    return os.str();
}

std::string hexValue(uint32_t value) {
    std::ostringstream os;
    os << "0x" << std::hex << value;
    return os.str();
}

// Destino de um desvio/salto, ja como endereco absoluto.
std::string target(const Instruction& inst) {
    const auto address = inst.branchTarget();
    return address ? hex32(*address) : std::to_string(inst.imm.value_or(0));
}

bool isShiftImmediate(const Instruction& inst) {
    return inst.mnemonic == "slli" || inst.mnemonic == "srli" || inst.mnemonic == "srai";
}

bool isLoad(const Instruction& inst) { return inst.cls == Class::Load; }

bool isCsr(const Instruction& inst) { return inst.mnemonic.rfind("csrr", 0) == 0; }

}  // namespace

const char* abiName(uint8_t reg) { return reg < kAbiNames.size() ? kAbiNames[reg] : "x?"; }

std::string toAssembly(const Instruction& inst) {
    if (!inst.valid()) return "(instrucao invalida)";

    std::ostringstream os;
    os << inst.mnemonic;

    switch (inst.format) {
        case Format::R:
            os << ' ' << abiName(*inst.rd) << ", " << abiName(*inst.rs1) << ", "
               << abiName(*inst.rs2);
            break;

        case Format::I:
            if (inst.mnemonic == "ecall" || inst.mnemonic == "ebreak" ||
                inst.mnemonic == "fence" || inst.mnemonic == "fence.i") {
                break;  // sem operandos
            }
            if (isCsr(inst)) {
                os << ' ' << abiName(*inst.rd) << ", " << hexValue(static_cast<uint32_t>(*inst.imm))
                   << ", ";
                if (inst.rs1) os << abiName(*inst.rs1);
                else os << (inst.raw >> 15 & 0x1F);  // imediato de 5 bits das variantes "i"
                break;
            }
            if (isLoad(inst) || inst.mnemonic == "jalr") {
                os << ' ' << abiName(*inst.rd) << ", " << *inst.imm << '(' << abiName(*inst.rs1)
                   << ')';
                break;
            }
            os << ' ' << abiName(*inst.rd) << ", " << abiName(*inst.rs1) << ", " << *inst.imm;
            break;

        case Format::S:
            os << ' ' << abiName(*inst.rs2) << ", " << *inst.imm << '(' << abiName(*inst.rs1) << ')';
            break;

        case Format::B:
            os << ' ' << abiName(*inst.rs1) << ", " << abiName(*inst.rs2) << ", " << target(inst);
            break;

        case Format::U:
            // Por convencao, o operando escrito de lui/auipc e o campo de 20 bits.
            os << ' ' << abiName(*inst.rd) << ", "
               << hexValue(static_cast<uint32_t>(*inst.imm) >> 12);
            break;

        case Format::J:
            os << ' ' << abiName(*inst.rd) << ", " << target(inst);
            break;

        case Format::Invalid:
            break;
    }

    return os.str();
}

std::optional<std::string> toPseudo(const Instruction& inst) {
    if (!inst.valid()) return std::nullopt;

    const auto rd = inst.rd.value_or(0xFF);
    const auto rs1 = inst.rs1.value_or(0xFF);
    const auto rs2 = inst.rs2.value_or(0xFF);
    const auto imm = inst.imm.value_or(0);
    const std::string& m = inst.mnemonic;

    std::ostringstream os;

    if (m == "addi") {
        if (rd == 0 && rs1 == 0 && imm == 0) return std::string("nop");
        if (imm == 0 && rs1 != 0) {
            os << "mv " << abiName(rd) << ", " << abiName(rs1);
            return os.str();
        }
        if (rs1 == 0) {
            os << "li " << abiName(rd) << ", " << imm;
            return os.str();
        }
        return std::nullopt;
    }

    if (m == "jalr" && imm == 0) {
        if (rd == 0 && rs1 == 1) return std::string("ret");
        if (rd == 0) {
            os << "jr " << abiName(rs1);
            return os.str();
        }
        if (rd == 1) {
            os << "jalr " << abiName(rs1);
            return os.str();
        }
        return std::nullopt;
    }

    if (m == "jal") {
        if (rd == 0) {
            os << "j " << target(inst);
            return os.str();
        }
        if (rd == 1) {
            os << "jal " << target(inst);
            return os.str();
        }
        return std::nullopt;
    }

    if (m == "xori" && imm == -1) {
        os << "not " << abiName(rd) << ", " << abiName(rs1);
        return os.str();
    }
    if (m == "sub" && rs1 == 0) {
        os << "neg " << abiName(rd) << ", " << abiName(rs2);
        return os.str();
    }
    if (m == "sltiu" && imm == 1) {
        os << "seqz " << abiName(rd) << ", " << abiName(rs1);
        return os.str();
    }
    if (m == "sltu" && rs1 == 0) {
        os << "snez " << abiName(rd) << ", " << abiName(rs2);
        return os.str();
    }
    if (m == "slt" && rs2 == 0) {
        os << "sltz " << abiName(rd) << ", " << abiName(rs1);
        return os.str();
    }
    if (m == "slt" && rs1 == 0) {
        os << "sgtz " << abiName(rd) << ", " << abiName(rs2);
        return os.str();
    }

    if (inst.format == Format::B) {
        if (rs2 == 0 && (m == "beq" || m == "bne" || m == "bge" || m == "blt")) {
            const char* name = m == "beq"   ? "beqz"
                               : m == "bne" ? "bnez"
                               : m == "bge" ? "bgez"
                                            : "bltz";
            os << name << ' ' << abiName(rs1) << ", " << target(inst);
            return os.str();
        }
        if (rs1 == 0 && (m == "beq" || m == "bne" || m == "bge" || m == "blt")) {
            const char* name = m == "beq"   ? "beqz"
                               : m == "bne" ? "bnez"
                               : m == "bge" ? "blez"  // bge zero, rs2  ==  blez rs2
                                            : "bgtz";  // blt zero, rs2  ==  bgtz rs2
            os << name << ' ' << abiName(rs2) << ", " << target(inst);
            return os.str();
        }
    }

    return std::nullopt;
}

std::string fieldsToString(const Instruction& inst) {
    if (!inst.valid()) return inst.error;

    std::ostringstream os;
    bool first = true;
    const auto add = [&](const std::string& text) {
        if (!first) os << ' ';
        os << text;
        first = false;
    };

    if (inst.rd) add("rd=" + std::to_string(*inst.rd));
    if (inst.rs1) add("rs1=" + std::to_string(*inst.rs1));
    if (inst.rs2) add("rs2=" + std::to_string(*inst.rs2));
    if (inst.funct3) add("f3=" + hexValue(*inst.funct3));
    if (inst.funct7) add("f7=" + hexValue(*inst.funct7));
    if (inst.imm) {
        if (inst.format == Format::U) {
            add("imm=" + hex32(static_cast<uint32_t>(*inst.imm)));
        } else if (isShiftImmediate(inst)) {
            add("shamt=" + std::to_string(*inst.imm));
        } else {
            add("imm=" + std::to_string(*inst.imm));
        }
    }
    if (const auto address = inst.branchTarget()) add("alvo=" + hex32(*address));

    return os.str();
}

}  // namespace riscv
