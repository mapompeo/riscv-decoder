#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace riscv {

// Os seis formatos do RISC-V (RV32). "Invalid" marca palavras desconhecidas,
// que sao reportadas mas nao interrompem o processamento do arquivo.
enum class Format { R, I, S, B, U, J, Invalid };

// Classe funcional da instrucao. Serve para a tabela de CPI (R5) e, na Etapa 2,
// para saber quais instrucoes acessam memoria ou alteram o fluxo de controle.
enum class Class { Alu, MulDiv, Load, Store, Branch, Jump, Upper, System, Invalid };

const char* toString(Format f);
const char* toString(Class c);
std::optional<Class> classFromString(const std::string& name);

// Uma instrucao decodificada. Os campos que nao existem no formato ficam
// vazios de verdade (std::nullopt) -- nunca preenchidos com lixo dos bits.
struct Instruction {
    uint32_t pc = 0;
    uint32_t raw = 0;
    Format format = Format::Invalid;
    Class cls = Class::Invalid;
    std::string mnemonic;  // vazio quando a palavra e invalida
    std::string error;     // motivo da invalidez, quando houver

    std::optional<uint8_t> rd;
    std::optional<uint8_t> rs1;
    std::optional<uint8_t> rs2;
    std::optional<uint8_t> funct3;
    std::optional<uint8_t> funct7;
    std::optional<int32_t> imm;  // ja reconstruido e com extensao de sinal

    bool valid() const { return format != Format::Invalid; }

    // --- Ganchos para a Etapa 2 (deteccao de hazards) ---

    // Registradores lidos pela instrucao. x0 e omitido: seu valor e sempre 0,
    // logo nunca participa de um conflito de dados.
    std::vector<uint8_t> readsRegisters() const;

    // Registrador escrito pela instrucao, se houver. Escrita em x0 e descartada
    // pelo hardware, entao tambem retorna vazio.
    std::optional<uint8_t> writesRegister() const;

    // Endereco absoluto de destino de desvios (B) e saltos (J).
    // jalr depende do conteudo de rs1 em tempo de execucao: nao e resolvido aqui.
    std::optional<uint32_t> branchTarget() const;
};

}  // namespace riscv
