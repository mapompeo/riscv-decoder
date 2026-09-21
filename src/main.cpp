// Decodificador de instrucoes RISC-V (RV32I/M) -- Avaliacao Pratica M1, Etapa 1.
// Le um arquivo de memoria de instrucoes (hexadecimal ou binario), decodifica
// cada palavra e emite a listagem, a desmontagem e o relatorio estatistico.

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "riscv.hpp"

namespace {

struct Options {
    std::string inputPath;
    std::string cpiPath;
    uint32_t baseAddress = 0x00000000;
};

void printUsage(const char* program) {
    std::cout << "Uso: " << program << " <arquivo-rom> [opcoes]\n\n"
              << "Opcoes:\n"
              << "  -b, --base <endereco>   Endereco-base do PC (padrao: 0x00000000)\n"
              << "  -c, --cpi <arquivo>     Tabela de CPI por classe de instrucao\n"
              << "  -h, --help              Mostra esta ajuda\n\n"
              << "O arquivo de entrada tem uma instrucao por linha, em hexadecimal ou\n"
              << "binario; o formato e identificado automaticamente.\n";
}

uint32_t parseAddress(const std::string& text) {
    const int base = (text.rfind("0x", 0) == 0 || text.rfind("0X", 0) == 0) ? 16 : 10;
    return static_cast<uint32_t>(std::stoul(text, nullptr, base));
}

bool parseArguments(int argc, char** argv, Options& options, std::string& error) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool needsValue = (arg == "-b" || arg == "--base" || arg == "-c" || arg == "--cpi");
        if (needsValue && i + 1 >= argc) {
            error = "faltou o valor de " + arg;
            return false;
        }

        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            std::exit(0);
        } else if (arg == "-b" || arg == "--base") {
            try {
                options.baseAddress = parseAddress(argv[++i]);
            } catch (const std::exception&) {
                error = "endereco-base invalido";
                return false;
            }
        } else if (arg == "-c" || arg == "--cpi") {
            options.cpiPath = argv[++i];
        } else if (!arg.empty() && arg[0] == '-') {
            error = "opcao desconhecida: " + arg;
            return false;
        } else if (options.inputPath.empty()) {
            options.inputPath = arg;
        } else {
            error = "apenas um arquivo de entrada e aceito";
            return false;
        }
    }

    if (options.inputPath.empty()) {
        error = "informe o arquivo de entrada";
        return false;
    }
    return true;
}

std::string hex32(uint32_t value) {
    std::ostringstream os;
    os << "0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << value;
    return os.str();
}

void printListing(const std::vector<riscv::Instruction>& program) {
    std::cout << "=== Listagem de instrucoes ===\n";
    std::cout << std::left << std::setw(12) << "ENDERECO" << std::setw(12) << "PALAVRA"
              << std::setw(5) << "FMT" << std::setw(9) << "MNEM" << std::setw(46) << "CAMPOS"
              << "ASSEMBLY\n";
    std::cout << std::string(110, '-') << '\n';

    for (const riscv::Instruction& inst : program) {
        std::cout << std::left << std::setw(12) << hex32(inst.pc) << std::setw(12) << hex32(inst.raw)
                  << std::setw(5) << riscv::toString(inst.format) << std::setw(9)
                  << (inst.valid() ? inst.mnemonic : "???") << std::setw(46)
                  << riscv::fieldsToString(inst);

        if (!inst.valid()) {
            std::cout << "(instrucao invalida)\n";
            continue;
        }

        std::cout << riscv::toAssembly(inst);
        if (const auto pseudo = riscv::toPseudo(inst)) std::cout << "   ; pseudo: " << *pseudo;
        std::cout << '\n';
    }
}

void printStatistics(const riscv::Statistics& stats, const riscv::CpiTable& cpi) {
    std::cout << "\n=== Relatorio estatistico ===\n";
    std::cout << "Instrucoes validas: " << stats.valid << "   invalidas: " << stats.invalid << "\n\n";

    std::cout << "Distribuicao por formato:\n";
    const riscv::Format formats[] = {riscv::Format::R, riscv::Format::I, riscv::Format::S,
                                     riscv::Format::B, riscv::Format::U, riscv::Format::J};
    for (riscv::Format format : formats) {
        const auto it = stats.byFormat.find(format);
        const std::size_t count = (it != stats.byFormat.end()) ? it->second : 0;
        const double percent =
            stats.valid > 0 ? (100.0 * static_cast<double>(count) / static_cast<double>(stats.valid)) : 0.0;
        std::cout << "  Tipo " << riscv::toString(format) << ": " << std::setw(4) << std::right
                  << count << "   " << std::fixed << std::setprecision(2) << std::setw(6) << percent
                  << "%\n"
                  << std::left;
    }

    std::cout << "\nDistribuicao por classe e CPI:\n";
    double totalCycles = 0.0;
    for (const auto& [cls, count] : stats.byClass) {
        const auto it = cpi.values.find(cls);
        const double value = (it != cpi.values.end()) ? it->second : 1.0;
        const double cycles = value * static_cast<double>(count);
        totalCycles += cycles;
        std::cout << "  " << std::left << std::setw(10) << riscv::toString(cls) << std::right
                  << std::setw(4) << count << " x CPI " << std::fixed << std::setprecision(2)
                  << value << " = " << std::setw(8) << cycles << " ciclos\n";
    }

    std::cout << "\nTotal de ciclos estimados: " << std::fixed << std::setprecision(2)
              << totalCycles << '\n';
    std::cout << "CPI medio (media ponderada): " << std::fixed << std::setprecision(3)
              << stats.averageCpi << '\n';
}

}  // namespace

int main(int argc, char** argv) {
    Options options;
    std::string error;
    if (!parseArguments(argc, argv, options, error)) {
        std::cerr << "erro: " << error << "\n\n";
        printUsage(argv[0]);
        return 1;
    }

    std::ifstream input(options.inputPath);
    if (!input) {
        std::cerr << "erro: nao foi possivel abrir '" << options.inputPath << "'\n";
        return 1;
    }

    riscv::CpiTable cpi = riscv::defaultCpiTable();
    if (!options.cpiPath.empty()) {
        std::ifstream cpiFile(options.cpiPath);
        if (!cpiFile) {
            std::cerr << "erro: nao foi possivel abrir a tabela de CPI '" << options.cpiPath << "'\n";
            return 1;
        }
        cpi = riscv::loadCpiTable(cpiFile);
        for (const std::string& warning : cpi.warnings) {
            std::cerr << "aviso (tabela de CPI): " << warning << '\n';
        }
    }

    const riscv::ParseResult parsed = riscv::parseStream(input, options.baseAddress);

    std::cout << "Arquivo: " << options.inputPath << '\n';
    std::cout << "Formato detectado: " << riscv::toString(parsed.radix) << '\n';
    std::cout << "Endereco-base: " << hex32(options.baseAddress) << "\n\n";

    if (!parsed.errors.empty()) {
        std::cout << "=== Linhas ignoradas ===\n";
        for (const riscv::LineError& e : parsed.errors) {
            std::cout << "  linha " << e.line << ": '" << e.text << "' -> " << e.reason << '\n';
        }
        std::cout << '\n';
    }

    std::vector<riscv::Instruction> program;
    program.reserve(parsed.words.size());
    for (const riscv::RawWord& word : parsed.words) {
        program.push_back(riscv::decode(word.value, word.pc));
    }

    printListing(program);

    // Palavras invalidas sao listadas de novo, juntas, com o endereco em que ocorrem.
    bool anyInvalid = false;
    for (const riscv::Instruction& inst : program) {
        if (inst.valid()) continue;
        if (!anyInvalid) {
            std::cout << "\n=== Instrucoes invalidas ===\n";
            anyInvalid = true;
        }
        std::cout << "  " << hex32(inst.pc) << ": " << hex32(inst.raw) << " -> " << inst.error
                  << '\n';
    }

    printStatistics(riscv::computeStatistics(program, cpi), cpi);
    return 0;
}
