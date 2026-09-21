#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "riscv/instruction.hpp"

namespace riscv {

// Nome ABI do registrador (x8 -> s0, x2 -> sp, ...).
const char* abiName(uint8_t reg);

// Instrucao em assembly, com nomes ABI. Para desvios e saltos o operando
// exibido e o endereco absoluto de destino, e nao o deslocamento.
std::string toAssembly(const Instruction& inst);

// Pseudo-instrucao equivalente, quando existe (nop, ret, mv, li, j, ...).
std::optional<std::string> toPseudo(const Instruction& inst);

// Campos validos do formato, no estilo "rd=8 rs1=0 f3=0x0 imm=5".
std::string fieldsToString(const Instruction& inst);

}  // namespace riscv
