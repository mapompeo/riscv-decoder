# Decodificador de Instruções RISC-V — Etapa 1

Avaliação Prática M1 — Organização de Computadores (UNIVALI)
Prof. Thiago Felski Pereira

Programa em C++17 que lê um arquivo de memória de instruções (ROM) em linguagem
de máquina, decodifica cada palavra de 32 bits e emite a listagem completa, a
desmontagem em assembly e um relatório estatístico com o CPI médio.

A Etapa 2 (detecção de hazards em um pipeline de cinco estágios) vai reaproveitar
esta base: a estrutura `Instruction` já expõe `readsRegisters()` e
`writesRegister()`, que são exatamente a informação necessária para achar os
conflitos.

## Como compilar

Requer CMake 3.16+ e um compilador C++17 (g++, clang ou MSVC).

```bash
cmake -S . -B build
cmake --build build
```

No Windows com MinGW:

```bash
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

## Como executar

```bash
./build/riscv-decoder tests/data/programa.hex --cpi tests/data/cpi.txt
./build/riscv-decoder tests/data/programa.bin --base 0x00400000
```

| Opção | Descrição |
|---|---|
| `<arquivo>` | ROM com uma instrução por linha, em hexadecimal ou binário |
| `-b, --base <end>` | Endereço-base do PC (padrão `0x00000000`) |
| `-c, --cpi <arquivo>` | Tabela de CPI por classe de instrução |
| `-h, --help` | Ajuda |

## Como rodar os testes

```bash
cd build && ctest --output-on-failure
```

ou diretamente:

```bash
./build/test_decoder
```

Os testes cobrem os quatro exemplos do enunciado, a reconstrução dos imediatos
(incluindo os embaralhados dos tipos B e J, com deslocamento negativo), a
validade dos campos por formato, as pseudo-instruções, a leitura de entrada em
hex e binário e o cálculo do CPI.

## Formato dos arquivos de entrada

Uma instrução por linha. O programa identifica sozinho se o arquivo está em
hexadecimal ou em binário — não há opção de linha de comando para isso.

```
# comentários com #, // ou ; são ignorados
0x00500413      # o prefixo 0x é opcional
00C58633
```

```
00000000010100000000010000010011   // binário: 32 dígitos por linha
```

Linhas em branco são ignoradas. Uma linha malformada é reportada e o
processamento segue.

### Tabela de CPI

```
alu    = 1.0
muldiv = 3.0
load   = 2.0
store  = 1.0
branch = 2.0
jump   = 2.0
upper  = 1.0
system = 1.0
```

O CPI médio é a média aritmética ponderada: `Σ(quantidade × CPI) / total`.

## Como os requisitos foram atendidos

| Requisito | Onde está |
|---|---|
| R1 — Leitura robusta da entrada | `src/parser.cpp` |
| R2 — Classificação nos seis formatos | `src/decoder.cpp` (`decode`, por opcode) |
| R2 — Mnemônico por opcode + funct3 + funct7 | `src/decoder.cpp` (`decodeR`, `decodeOpImm`, …) |
| R3 — Campos válidos por formato | `Instruction` usa `std::optional`: campo inexistente fica vazio |
| R3 — Imediatos e extensão de sinal | `immediateI/S/B/U/J` + `signExtend` |
| R4 — Saída, nomes ABI, alvo absoluto, pseudo | `src/disassembler.cpp`, `src/main.cpp` |
| R5 — Estatística e CPI médio | `src/statistics.cpp` |

## Estrutura do projeto

```
include/riscv/     cabeçalhos públicos (instruction, parser, decoder, disassembler, statistics)
src/               implementação e programa principal
tests/             testes automatizados
tests/data/        programa de teste em hex e binário + tabela de CPI
```

O fluxo é `parser → decoder → disassembler / statistics`: cada etapa é
independente e testável isoladamente.

## Instruções suportadas

RV32I completo (aritmética, lógica, deslocamentos, loads, stores, desvios,
saltos, `lui`/`auipc`, `ecall`/`ebreak`, `fence`) e a extensão RV32M
(`mul`, `mulh`, `mulhsu`, `mulhu`, `div`, `divu`, `rem`, `remu`), além dos
acessos a CSR. Pseudo-instruções reconhecidas na saída: `nop`, `mv`, `li`,
`ret`, `jr`, `j`, `jal`, `not`, `neg`, `seqz`, `snez`, `sltz`, `sgtz`,
`beqz`, `bnez`, `blez`, `bgez`, `bltz`, `bgtz`.

## Detalhes de decodificação que valem a atenção

- **Tipo B não tem `rd`.** Em `0xFE628CE3` os bits 11–7 valem 25, mas são parte
  do imediato (`imm[4:1|11]`). O campo `rd` fica vazio.
- **Tipo U não tem `rs1`, `rs2` nem `funct3`**, ainda que esses bits tenham
  algum valor.
- **`srai` × `srli`** se distinguem apenas pelo bit 30 (`funct7 = 0x20`).
- **Deslocamentos imediatos** (`slli`, `srli`, `srai`) usam os 12 bits do
  imediato como `funct7` + `shamt`; a saída mostra `shamt`.
- **`ecall`/`ebreak`** não têm operandos: os campos ficam vazios para não
  produzir dependência falsa na Etapa 2.
