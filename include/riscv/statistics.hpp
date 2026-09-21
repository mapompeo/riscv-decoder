#pragma once

#include <cstddef>
#include <iosfwd>
#include <map>
#include <string>
#include <vector>

#include "riscv/instruction.hpp"

namespace riscv {

// Ciclos por instrucao de cada classe (R5). Vem de um arquivo de entrada;
// na falta dele, valem os valores didaticos de um pipeline de cinco estagios.
struct CpiTable {
    std::map<Class, double> values;
    std::vector<std::string> warnings;
};

CpiTable defaultCpiTable();

// Le linhas "classe = ciclos" (ex.: "load = 2.0"), ignorando comentarios.
// Classes ausentes mantem o valor padrao.
CpiTable loadCpiTable(std::istream& in);

struct Statistics {
    std::map<Format, std::size_t> byFormat;
    std::map<Class, std::size_t> byClass;
    std::size_t valid = 0;
    std::size_t invalid = 0;
    double averageCpi = 0.0;  // media aritmetica ponderada pela quantidade de cada classe
};

Statistics computeStatistics(const std::vector<Instruction>& program, const CpiTable& cpi);

}  // namespace riscv
