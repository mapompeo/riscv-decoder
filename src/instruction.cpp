#include "riscv/instruction.hpp"

#include <algorithm>
#include <array>
#include <cctype>

namespace riscv {

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
