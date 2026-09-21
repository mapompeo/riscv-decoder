// Decodificador de instrucoes RISC-V (RV32I/M) -- interface publica.
//
// O fluxo e: parseStream (le a ROM) -> decode (interpreta os bits) ->
// toAssembly / computeStatistics (saida e relatorio).

#pragma once

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace riscv {

// ---------------------------------------------------------------------------
// Instrucao decodificada
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// R1 -- leitura da entrada
// ---------------------------------------------------------------------------

// Base numerica detectada no arquivo de entrada.
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

// ---------------------------------------------------------------------------
// R2 e R3 -- classificacao, mnemonico, campos e imediatos
// ---------------------------------------------------------------------------

// Decodifica uma palavra de 32 bits. Palavras desconhecidas voltam com
// format == Format::Invalid e o motivo preenchido em .error.
Instruction decode(uint32_t word, uint32_t pc);

int32_t signExtend(uint32_t value, int width);
int32_t immediateI(uint32_t word);
int32_t immediateS(uint32_t word);
int32_t immediateB(uint32_t word);
int32_t immediateU(uint32_t word);
int32_t immediateJ(uint32_t word);

// ---------------------------------------------------------------------------
// R4 -- desmontagem
// ---------------------------------------------------------------------------

// Nome ABI do registrador (x8 -> s0, x2 -> sp, ...).
const char* abiName(uint8_t reg);

// Instrucao em assembly, com nomes ABI. Para desvios e saltos o operando
// exibido e o endereco absoluto de destino, e nao o deslocamento.
std::string toAssembly(const Instruction& inst);

// Pseudo-instrucao equivalente, quando existe (nop, ret, mv, li, j, ...).
std::optional<std::string> toPseudo(const Instruction& inst);

// Campos validos do formato, no estilo "rd=8 rs1=0 f3=0x0 imm=5".
std::string fieldsToString(const Instruction& inst);

// ---------------------------------------------------------------------------
// R5 -- relatorio estatistico
// ---------------------------------------------------------------------------

// Ciclos por instrucao de cada classe. Vem de um arquivo de entrada; na falta
// dele, valem os valores didaticos de um pipeline de cinco estagios.
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
