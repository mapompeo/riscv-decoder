#include "riscv.hpp"

#include <cctype>
#include <istream>
#include <stdexcept>
#include <string>

namespace riscv {
namespace {

std::string trim(const std::string& text) {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;
    return text.substr(begin, end - begin);
}

std::string stripComment(const std::string& line) {
    const std::size_t hash = line.find('#');
    return hash == std::string::npos ? line : line.substr(0, hash);
}

}  // namespace

CpiTable defaultCpiTable() {
    CpiTable table;
    table.values = {
        {Class::Alu, 1.0},   {Class::MulDiv, 3.0}, {Class::Load, 2.0},   {Class::Store, 1.0},
        {Class::Branch, 2.0}, {Class::Jump, 2.0},  {Class::Upper, 1.0}, {Class::System, 1.0},
    };
    return table;
}

CpiTable loadCpiTable(std::istream& in) {
    CpiTable table = defaultCpiTable();

    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(in, line)) {
        ++lineNumber;
        const std::string body = trim(stripComment(line));
        if (body.empty()) continue;

        const std::size_t separator = body.find_first_of("=:");
        if (separator == std::string::npos) {
            table.warnings.push_back("linha " + std::to_string(lineNumber) + ": formato esperado 'classe = ciclos'");
            continue;
        }

        const std::string name = trim(body.substr(0, separator));
        const std::string value = trim(body.substr(separator + 1));
        const auto cls = classFromString(name);
        if (!cls) {
            table.warnings.push_back("linha " + std::to_string(lineNumber) + ": classe desconhecida '" + name + "'");
            continue;
        }
        try {
            table.values[*cls] = std::stod(value);
        } catch (const std::exception&) {
            table.warnings.push_back("linha " + std::to_string(lineNumber) + ": valor invalido '" + value + "'");
        }
    }

    return table;
}

Statistics computeStatistics(const std::vector<Instruction>& program, const CpiTable& cpi) {
    Statistics stats;

    double cycles = 0.0;
    for (const Instruction& inst : program) {
        if (!inst.valid()) {
            ++stats.invalid;
            continue;
        }
        ++stats.valid;
        ++stats.byFormat[inst.format];
        ++stats.byClass[inst.cls];

        const auto it = cpi.values.find(inst.cls);
        cycles += (it != cpi.values.end()) ? it->second : 1.0;
    }

    // Media aritmetica ponderada: soma(qtd_classe * cpi_classe) / total de instrucoes.
    if (stats.valid > 0) cycles /= static_cast<double>(stats.valid);
    stats.averageCpi = cycles;
    return stats;
}

}  // namespace riscv
