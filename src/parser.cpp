#include "riscv.hpp"

#include <algorithm>
#include <cctype>
#include <istream>
#include <stdexcept>

namespace riscv {
namespace {

// Uma linha util do arquivo, ja sem comentarios nem espacos.
struct Token {
    std::size_t line;
    std::string original;
    std::string digits;  // sem prefixo 0x / 0b
    bool forcedHex = false;
    bool forcedBinary = false;
};

std::string stripComment(const std::string& line) {
    std::size_t cut = line.size();
    for (std::size_t i = 0; i < line.size(); ++i) {
        const bool doubleSlash = line[i] == '/' && i + 1 < line.size() && line[i + 1] == '/';
        if (line[i] == '#' || line[i] == ';' || doubleSlash) {
            cut = i;
            break;
        }
    }
    return line.substr(0, cut);
}

std::string compact(const std::string& text) {
    std::string out;
    for (char ch : text) {
        // Espacos e underscores sao ignorados: aceita "0000 0000 0101 ..." e 0000_0101.
        if (std::isspace(static_cast<unsigned char>(ch)) || ch == '_') continue;
        out += ch;
    }
    return out;
}

bool isHexDigit(char ch) { return std::isxdigit(static_cast<unsigned char>(ch)) != 0; }
bool isBinaryDigit(char ch) { return ch == '0' || ch == '1'; }

// Decide a base do arquivo inteiro a partir do conjunto de linhas lidas.
Radix detectRadix(const std::vector<Token>& tokens) {
    bool sawForcedHex = false;
    bool sawForcedBinary = false;
    bool sawNonBinaryDigit = false;
    bool sawWordOf32Binary = false;

    for (const Token& t : tokens) {
        if (t.forcedHex) sawForcedHex = true;
        if (t.forcedBinary) sawForcedBinary = true;
        if (!std::all_of(t.digits.begin(), t.digits.end(), isBinaryDigit)) {
            sawNonBinaryDigit = true;  // tem a-f: so pode ser hexadecimal
        } else if (t.digits.size() > 8) {
            sawWordOf32Binary = true;  // 0/1 com mais de 8 digitos: so pode ser binario
        }
    }

    if (sawNonBinaryDigit || sawForcedHex) return Radix::Hex;
    if (sawWordOf32Binary || sawForcedBinary) return Radix::Binary;
    if (tokens.empty()) return Radix::Unknown;
    // Restou o caso ambiguo (tudo 0/1 com <= 8 digitos, ex.: "00000013").
    // 8 digitos de 0/1 formam uma palavra hexadecimal valida; menos que isso,
    // trata-se de hexadecimal curto. Binario sempre precisa dos 32 bits.
    return Radix::Hex;
}

uint32_t convert(const std::string& digits, Radix radix) {
    const int base = radix == Radix::Binary ? 2 : 16;
    const std::size_t maxDigits = radix == Radix::Binary ? 32 : 8;
    if (digits.empty() || digits.size() > maxDigits) throw std::invalid_argument("tamanho invalido");
    for (char ch : digits) {
        const bool ok = radix == Radix::Binary ? isBinaryDigit(ch) : isHexDigit(ch);
        if (!ok) throw std::invalid_argument("digito invalido para a base do arquivo");
    }
    return static_cast<uint32_t>(std::stoul(digits, nullptr, base));
}

}  // namespace

const char* toString(Radix r) {
    switch (r) {
        case Radix::Hex: return "hexadecimal";
        case Radix::Binary: return "binario";
        case Radix::Unknown: return "indefinido";
    }
    return "indefinido";
}

ParseResult parseStream(std::istream& in, uint32_t baseAddress) {
    ParseResult result;
    std::vector<Token> tokens;

    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(in, line)) {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') line.pop_back();

        const std::string body = compact(stripComment(line));
        if (body.empty()) continue;

        Token token;
        token.line = lineNumber;
        token.original = body;
        token.digits = body;

        const bool hasPrefix = body.size() > 2 && body[0] == '0';
        if (hasPrefix && (body[1] == 'x' || body[1] == 'X')) {
            token.forcedHex = true;
            token.digits = body.substr(2);
        } else if (hasPrefix && (body[1] == 'b' || body[1] == 'B') &&
                   std::all_of(body.begin() + 2, body.end(), isBinaryDigit)) {
            token.forcedBinary = true;
            token.digits = body.substr(2);
        }

        tokens.push_back(std::move(token));
    }

    result.radix = detectRadix(tokens);

    uint32_t pc = baseAddress;
    for (const Token& token : tokens) {
        // Um prefixo explicito vence a deteccao global daquela linha.
        Radix radix = result.radix;
        if (token.forcedHex) radix = Radix::Hex;
        if (token.forcedBinary) radix = Radix::Binary;

        try {
            RawWord word;
            word.pc = pc;
            word.value = convert(token.digits, radix);
            word.line = token.line;
            result.words.push_back(word);
            pc += 4;
        } catch (const std::exception& e) {
            result.errors.push_back({token.line, token.original, e.what()});
        }
    }

    return result;
}

}  // namespace riscv
