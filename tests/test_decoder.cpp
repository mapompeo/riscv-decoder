// Testes do decodificador. Sem framework externo: cada verificacao imprime
// PASS/FAIL e o programa retorna != 0 se qualquer uma falhar.

#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "riscv.hpp"

namespace {

int failures = 0;

template <typename A, typename B>
void check(const std::string& name, const A& actual, const B& expected) {
    if (actual == expected) {
        std::cout << "[ ok ] " << name << '\n';
    } else {
        std::cout << "[FAIL] " << name << ": obtido <" << actual << ">, esperado <" << expected
                  << ">\n";
        ++failures;
    }
}

void checkTrue(const std::string& name, bool condition) {
    check(name, condition ? "verdadeiro" : "falso", "verdadeiro");
}

// --- Exemplos do enunciado -------------------------------------------------

void testExemploAddi() {
    const auto inst = riscv::decode(0x00500413, 0x00000000);
    check("addi: formato", std::string(riscv::toString(inst.format)), std::string("I"));
    check("addi: mnemonico", inst.mnemonic, std::string("addi"));
    check("addi: rd", int(inst.rd.value_or(255)), 8);
    check("addi: rs1", int(inst.rs1.value_or(255)), 0);
    check("addi: imm", inst.imm.value_or(-1), 5);
    check("addi: f3", int(inst.funct3.value_or(255)), 0);
    checkTrue("addi: sem rs2", !inst.rs2.has_value());
    checkTrue("addi: sem f7", !inst.funct7.has_value());
    check("addi: assembly", riscv::toAssembly(inst), std::string("addi s0, zero, 5"));
}

void testExemploAdd() {
    const auto inst = riscv::decode(0x00C58633, 0x00000004);
    check("add: formato", std::string(riscv::toString(inst.format)), std::string("R"));
    check("add: mnemonico", inst.mnemonic, std::string("add"));
    check("add: rd", int(inst.rd.value_or(255)), 12);
    check("add: rs1", int(inst.rs1.value_or(255)), 11);
    check("add: rs2", int(inst.rs2.value_or(255)), 12);
    check("add: f3", int(inst.funct3.value_or(255)), 0);
    check("add: f7", int(inst.funct7.value_or(255)), 0);
    checkTrue("add: sem imediato", !inst.imm.has_value());
    check("add: assembly", riscv::toAssembly(inst), std::string("add a2, a1, a2"));
}

void testExemploSw() {
    const auto inst = riscv::decode(0x0064A423, 0x00000008);
    check("sw: formato", std::string(riscv::toString(inst.format)), std::string("S"));
    check("sw: mnemonico", inst.mnemonic, std::string("sw"));
    check("sw: rs1", int(inst.rs1.value_or(255)), 9);
    check("sw: rs2", int(inst.rs2.value_or(255)), 6);
    check("sw: imm", inst.imm.value_or(-1), 8);
    checkTrue("sw: tipo S nao tem rd", !inst.rd.has_value());
    check("sw: assembly", riscv::toAssembly(inst), std::string("sw t1, 8(s1)"));
}

void testExemploBeq() {
    // Armadilha do enunciado: os bits 11-7 valem 25, mas tipo B nao possui rd.
    const auto inst = riscv::decode(0xFE628CE3, 0x00000020);
    check("beq: formato", std::string(riscv::toString(inst.format)), std::string("B"));
    check("beq: mnemonico", inst.mnemonic, std::string("beq"));
    check("beq: rs1", int(inst.rs1.value_or(255)), 5);
    check("beq: rs2", int(inst.rs2.value_or(255)), 6);
    check("beq: imm negativo", inst.imm.value_or(0), -8);
    checkTrue("beq: tipo B nao tem rd", !inst.rd.has_value());
    check("beq: alvo absoluto", inst.branchTarget().value_or(0), uint32_t(0x00000018));
    check("beq: assembly", riscv::toAssembly(inst), std::string("beq t0, t1, 0x00000018"));
}

// --- Imediatos -------------------------------------------------------------

void testImediatos() {
    check("signExtend 12 bits negativo", riscv::signExtend(0xFFF, 12), -1);
    check("signExtend 13 bits negativo", riscv::signExtend(0x1FF8, 13), -8);
    check("immediateI negativo", riscv::immediateI(0xFFF00093), -1);   // addi ra, zero, -1
    check("immediateS negativo", riscv::immediateS(0xFE112E23), -4);   // sw ra, -4(sp)
    check("immediateB negativo", riscv::immediateB(0xFE628CE3), -8);
    check("immediateJ negativo", riscv::immediateJ(0xFF1FF0EF), -16);  // jal ra, -16
    check("immediateJ positivo", riscv::immediateJ(0x0100006F), 16);   // j +16
    check("immediateU", uint32_t(riscv::immediateU(0x123452B7)), uint32_t(0x12345000));
}

// --- Campos validos por formato -------------------------------------------

void testCamposPorFormato() {
    const auto lui = riscv::decode(0x123452B7, 0);  // lui t0, 0x12345
    check("lui: formato", std::string(riscv::toString(lui.format)), std::string("U"));
    checkTrue("lui: sem rs1", !lui.rs1.has_value());
    checkTrue("lui: sem rs2", !lui.rs2.has_value());
    checkTrue("lui: sem funct3", !lui.funct3.has_value());
    check("lui: rd", int(lui.rd.value_or(255)), 5);

    const auto jal = riscv::decode(0x008000EF, 0x00000100);  // jal ra, +8
    check("jal: formato", std::string(riscv::toString(jal.format)), std::string("J"));
    checkTrue("jal: sem rs1", !jal.rs1.has_value());
    check("jal: alvo", jal.branchTarget().value_or(0), uint32_t(0x00000108));

    const auto srai = riscv::decode(0x4020D093, 0);  // srai ra, ra, 2
    check("srai: mnemonico", srai.mnemonic, std::string("srai"));
    check("srai: shamt", srai.imm.value_or(-1), 2);
    check("srai: f7", int(srai.funct7.value_or(0)), 0x20);
    const auto srli = riscv::decode(0x0020D093, 0);  // srli ra, ra, 2
    check("srli: mnemonico", srli.mnemonic, std::string("srli"));

    const auto sub = riscv::decode(0x40B50533, 0);  // sub a0, a0, a1
    check("sub: mnemonico", sub.mnemonic, std::string("sub"));
    const auto bltu = riscv::decode(0x00B56463, 0);  // bltu a0, a1, +8
    check("bltu: mnemonico", bltu.mnemonic, std::string("bltu"));
    const auto lb = riscv::decode(0x00050503, 0);  // lb a0, 0(a0)
    check("lb: mnemonico", lb.mnemonic, std::string("lb"));
    const auto lw = riscv::decode(0x00052503, 0);  // lw a0, 0(a0)
    check("lw: mnemonico", lw.mnemonic, std::string("lw"));
}

void testInvalidas() {
    const auto inst = riscv::decode(0xFFFFFFFF, 0x00000040);
    checkTrue("palavra invalida e marcada", !inst.valid());
    check("palavra invalida guarda o pc", inst.pc, uint32_t(0x00000040));
    checkTrue("palavra invalida tem motivo", !inst.error.empty());

    const auto zero = riscv::decode(0x00000000, 0);
    checkTrue("palavra zero e invalida", !zero.valid());
}

// --- Pseudo-instrucoes -----------------------------------------------------

void testPseudo() {
    check("nop", riscv::toPseudo(riscv::decode(0x00000013, 0)).value_or(""), std::string("nop"));
    check("ret", riscv::toPseudo(riscv::decode(0x00008067, 0)).value_or(""), std::string("ret"));
    check("mv", riscv::toPseudo(riscv::decode(0x00050593, 0)).value_or(""),
          std::string("mv a1, a0"));  // addi a1, a0, 0
    check("li", riscv::toPseudo(riscv::decode(0x00500513, 0)).value_or(""),
          std::string("li a0, 5"));  // addi a0, zero, 5
    check("j", riscv::toPseudo(riscv::decode(0x0100006F, 0x00000010)).value_or(""),
          std::string("j 0x00000020"));  // jal zero, +16
}

// --- Registradores lidos/escritos (gancho da Etapa 2) ----------------------

void testRegistradores() {
    const auto add = riscv::decode(0x00C58633, 0);  // add a2, a1, a2
    check("add le dois registradores", add.readsRegisters().size(), std::size_t(2));
    check("add escreve rd", int(add.writesRegister().value_or(255)), 12);

    const auto beq = riscv::decode(0xFE628CE3, 0);
    check("beq le dois registradores", beq.readsRegisters().size(), std::size_t(2));
    checkTrue("beq nao escreve registrador", !beq.writesRegister().has_value());

    const auto nop = riscv::decode(0x00000013, 0);  // addi x0, x0, 0
    checkTrue("nop nao escreve (rd = x0)", !nop.writesRegister().has_value());
    check("nop nao le (rs1 = x0)", nop.readsRegisters().size(), std::size_t(0));
}

// --- Leitura da entrada ----------------------------------------------------

void testParser() {
    {
        std::istringstream in("# comentario\n0x00500413\n\n00C58633   // add\n");
        const auto result = riscv::parseStream(in, 0x00000000);
        check("hex: base detectada", std::string(riscv::toString(result.radix)),
              std::string("hexadecimal"));
        check("hex: quantidade de palavras", result.words.size(), std::size_t(2));
        check("hex: primeira palavra", result.words[0].value, uint32_t(0x00500413));
        check("hex: pc da segunda", result.words[1].pc, uint32_t(0x00000004));
    }
    {
        std::istringstream in("00000000010100000000010000010011\n; comentario\n");
        const auto result = riscv::parseStream(in, 0x00400000);
        check("bin: base detectada", std::string(riscv::toString(result.radix)),
              std::string("binario"));
        check("bin: valor", result.words[0].value, uint32_t(0x00500413));
        check("bin: endereco-base configuravel", result.words[0].pc, uint32_t(0x00400000));
    }
    {
        std::istringstream in("0x00500413\nZZZZ\n");
        const auto result = riscv::parseStream(in, 0);
        check("linha invalida nao interrompe", result.words.size(), std::size_t(1));
        check("linha invalida e reportada", result.errors.size(), std::size_t(1));
    }
}

// --- CPI -------------------------------------------------------------------

void testCpi() {
    std::istringstream cpiFile("# tabela\nalu = 1\nload = 2\nbranch = 3\n");
    const auto table = riscv::loadCpiTable(cpiFile);

    std::vector<riscv::Instruction> program{
        riscv::decode(0x00500413, 0),   // addi  -> alu    (1)
        riscv::decode(0x00052503, 4),   // lw    -> load   (2)
        riscv::decode(0xFE628CE3, 8),   // beq   -> branch (3)
    };
    const auto stats = riscv::computeStatistics(program, table);
    check("stats: validas", stats.valid, std::size_t(3));
    check("stats: formato I", stats.byFormat.at(riscv::Format::I), std::size_t(2));
    check("stats: formato B", stats.byFormat.at(riscv::Format::B), std::size_t(1));
    checkTrue("stats: CPI medio = 2.0", stats.averageCpi > 1.999 && stats.averageCpi < 2.001);
}

}  // namespace

int main() {
    testExemploAddi();
    testExemploAdd();
    testExemploSw();
    testExemploBeq();
    testImediatos();
    testCamposPorFormato();
    testInvalidas();
    testPseudo();
    testRegistradores();
    testParser();
    testCpi();

    std::cout << '\n' << (failures == 0 ? "Todos os testes passaram." : "Falhas: " + std::to_string(failures)) << '\n';
    return failures == 0 ? 0 : 1;
}
