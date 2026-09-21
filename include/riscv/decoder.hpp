#pragma once

#include <cstdint>

#include "riscv/instruction.hpp"

namespace riscv {

// Decodifica uma palavra de 32 bits. Palavras desconhecidas voltam com
// format == Format::Invalid e o motivo preenchido em .error.
Instruction decode(uint32_t word, uint32_t pc);

// Auxiliares expostos para os testes.
int32_t signExtend(uint32_t value, int width);
int32_t immediateI(uint32_t word);
int32_t immediateS(uint32_t word);
int32_t immediateB(uint32_t word);
int32_t immediateU(uint32_t word);
int32_t immediateJ(uint32_t word);

}  // namespace riscv
