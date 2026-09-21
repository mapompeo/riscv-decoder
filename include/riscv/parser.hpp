#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace riscv {

// Base numerica detectada no arquivo de entrada (R1).
enum class Radix { Hex, Binary, Unknown };

const char* toString(Radix r);

struct RawWord {
    uint32_t pc = 0;
    uint32_t value = 0;
    std::size_t line = 0;  // linha do arquivo, para mensagens de erro
};

struct LineError {
    std::size_t line = 0;
    std::string text;
    std::string reason;
};

struct ParseResult {
    Radix radix = Radix::Unknown;
    std::vector<RawWord> words;
    std::vector<LineError> errors;
};

// Le uma instrucao por linha, em hexadecimal ou binario, detectando o formato
// automaticamente. Ignora linhas vazias, comentarios (#, //, ;) e o prefixo 0x.
// O PC de cada palavra vem de baseAddress, incrementado de 4 em 4 bytes.
ParseResult parseStream(std::istream& in, uint32_t baseAddress);

}  // namespace riscv
