# Guia de Estudo — Decodificador de Instruções RISC-V

**Do zero, para quem nunca viu o assunto.**
Este guia explica o trabalho inteiro: o que é uma instrução, como ela é
organizada em bits, o que o nosso programa faz com ela e como responder às
perguntas da defesa.

---

## Sumário

1. [O trabalho em uma frase](#1-o-trabalho-em-uma-frase)
2. [O básico: processador, instrução e registrador](#2-o-básico-processador-instrução-e-registrador)
3. [O que é RISC-V](#3-o-que-é-risc-v)
4. [Os 32 registradores e seus apelidos (nomes ABI)](#4-os-32-registradores-e-seus-apelidos-nomes-abi)
5. [A anatomia de uma instrução de 32 bits](#5-a-anatomia-de-uma-instrução-de-32-bits)
6. [Os seis formatos: R, I, S, B, U, J](#6-os-seis-formatos-r-i-s-b-u-j)
7. [Números negativos e extensão de sinal](#7-números-negativos-e-extensão-de-sinal)
8. [Por que os imediatos de B e J são embaralhados](#8-por-que-os-imediatos-de-b-e-j-são-embaralhados)
9. [Decodificando quatro instruções na mão](#9-decodificando-quatro-instruções-na-mão)
10. [Pseudo-instruções](#10-pseudo-instruções)
11. [CPI e média ponderada](#11-cpi-e-média-ponderada)
12. [Como o nosso programa está organizado](#12-como-o-nosso-programa-está-organizado)
13. [Como rodar](#13-como-rodar)
14. [Preparação para a defesa](#14-preparação-para-a-defesa)
15. [Glossário](#15-glossário)

---

## 1. O trabalho em uma frase

> Escrever um programa que lê um arquivo cheio de números de 32 bits e explica,
> para cada número, **qual instrução de processador ele representa**.

É um tradutor. Entra `0x00500413`, sai `addi s0, zero, 5` — "some 5 a zero e
guarde no registrador s0".

Quem faz esse trabalho dentro de um processador de verdade é um circuito
chamado **decodificador**. Estamos escrevendo, em software, o que esse circuito
faz em hardware.

**Por que isso importa para a Etapa 2:** na segunda parte do trabalho vamos
detectar *hazards* (conflitos) em um pipeline. Para isso é preciso saber, de
cada instrução, **quais registradores ela lê e qual ela escreve**. Se a
decodificação errar isso, a Etapa 2 acusa conflitos que não existem. Daí a
insistência do enunciado em precisão, e não em quantidade de instruções.

---

## 2. O básico: processador, instrução e registrador

| Conceito | O que é | Analogia |
|---|---|---|
| **Processador (CPU)** | O chip que executa instruções, uma após a outra | Um cozinheiro seguindo uma receita |
| **Instrução** | Uma ordem simples: somar, carregar, desviar | Um passo da receita: "bata dois ovos" |
| **Registrador** | Uma gavetinha ultrarrápida dentro da CPU que guarda 1 número | A bancada onde o cozinheiro deixa o que está usando agora |
| **Memória (RAM)** | Onde ficam os dados grandes, mais lenta | A geladeira: cabe muita coisa, mas é longe |
| **ROM de instruções** | Onde o programa está guardado | O livro de receitas |
| **PC (Program Counter)** | O endereço da instrução que está sendo executada | O dedo apontando a linha da receita |

O processador vive em um ciclo: **busca** a instrução apontada pelo PC,
**decodifica** (descobre o que ela manda fazer), **executa**, e avança o PC para
a próxima instrução.

Nosso trabalho é justamente a fase do meio: **decodificar**.

### Por que o PC anda de 4 em 4?

Cada instrução RISC-V de 32 bits ocupa **4 bytes** (32 ÷ 8 = 4). Endereços são
contados em bytes, então a instrução seguinte está 4 endereços à frente:

```
0x00000000  primeira instrução
0x00000004  segunda instrução
0x00000008  terceira instrução
```

---

## 3. O que é RISC-V

RISC-V (lê-se "risk five") é um **conjunto de instruções** — o "idioma" que um
processador entende. É aberto e gratuito, por isso é usado no ensino.

- **RISC** = *Reduced Instruction Set Computer*: poucas instruções, todas
  simples e do mesmo tamanho. O oposto é o CISC (o x86 do seu PC), com
  instruções complexas e de tamanhos variados.
- **RV32I** = a versão base, com registradores de **32 bits** e o conjunto
  **I**nteiro. É a que o trabalho exige.
- **RV32M** = extensão opcional com multiplicação e divisão (`mul`, `div`…).
  Nosso decodificador também suporta, porque o simulador da disciplina pode
  gerar essas instruções.

Como toda instrução tem exatamente 32 bits, **decodificar é fatiar esses 32
bits em pedaços** e interpretar cada pedaço.

---

## 4. Os 32 registradores e seus apelidos (nomes ABI)

O RV32I tem 32 registradores, `x0` a `x31`. Cada um guarda um número de 32 bits.
Por convenção (a **ABI**), cada um tem um apelido que diz para que serve — e é
esse apelido que aparece no assembly legível.

| Número | Nome ABI | Para que serve |
|---|---|---|
| x0 | `zero` | **Sempre vale 0.** Escrever nele não faz nada |
| x1 | `ra` | *return address* — para onde voltar depois de uma função |
| x2 | `sp` | *stack pointer* — topo da pilha |
| x3 | `gp` | *global pointer* |
| x4 | `tp` | *thread pointer* |
| x5–x7 | `t0`–`t2` | Temporários |
| x8 | `s0` (ou `fp`) | Salvo entre chamadas / *frame pointer* |
| x9 | `s1` | Salvo entre chamadas |
| x10–x17 | `a0`–`a7` | Argumentos e retorno de funções |
| x18–x27 | `s2`–`s11` | Salvos entre chamadas |
| x28–x31 | `t3`–`t6` | Temporários |

> **Por que `x0` é sempre zero?** Porque isso deixa o conjunto de instruções
> menor. Não existe instrução "copiar registrador": basta somar zero
> (`addi a1, a0, 0`). Não existe "carregar constante": basta somar a constante
> com `zero`. Veja a seção de [pseudo-instruções](#10-pseudo-instruções).
>
> Isso também importa para a Etapa 2: como `x0` nunca muda de valor, ele
> **nunca** causa um conflito de dados. Por isso nosso código o ignora em
> `readsRegisters()` e `writesRegister()`.

---

## 5. A anatomia de uma instrução de 32 bits

Os 32 bits são numerados do **31** (mais à esquerda) ao **0** (mais à direita).
Os pedaços possíveis são sempre estes:

```
 31        25 24     20 19     15 14    12 11      7 6         0
┌────────────┬─────────┬─────────┬────────┬─────────┬───────────┐
│   funct7   │   rs2   │   rs1   │ funct3 │   rd    │  opcode   │
└────────────┴─────────┴─────────┴────────┴─────────┴───────────┘
     7 bits    5 bits    5 bits    3 bits   5 bits     7 bits
```

| Campo | Tamanho | Significado |
|---|---|---|
| `opcode` | 7 bits | **A pergunta mais importante**: que tipo de instrução é essa? |
| `rd` | 5 bits | *destination register* — onde o resultado será guardado |
| `funct3` | 3 bits | Refina o opcode (ex.: dentro dos desvios, diz se é `beq` ou `bne`) |
| `rs1` | 5 bits | *source register 1* — primeiro operando |
| `rs2` | 5 bits | *source register 2* — segundo operando |
| `funct7` | 7 bits | Refina ainda mais (é o que separa `add` de `sub`) |
| `imediato` | varia | Uma constante embutida na própria instrução |

5 bits dão 32 combinações — exatamente o número de registradores. Não é
coincidência.

### A identificação é em três níveis

```
opcode  ──▶  "é uma operação registrador-registrador" (0x33)
  └── funct3  ──▶  "é da família da soma/subtração" (0x0)
        └── funct7  ──▶  0x00 = add     0x20 = sub
```

É por isso que o enunciado insiste: **não basta achar o formato, é preciso
combinar opcode + funct3 + funct7** para achar o mnemônico certo.

---

## 6. Os seis formatos: R, I, S, B, U, J

Nem toda instrução precisa de todos os campos. `add a2, a1, a2` precisa de três
registradores; `lui a0, 0x12345` precisa de um registrador e de uma constante
enorme. Para não desperdiçar bits, o RISC-V define **seis arranjos** diferentes.

| Formato | Usado por | rd | rs1 | rs2 | funct3 | funct7 | imediato |
|---|---|:-:|:-:|:-:|:-:|:-:|---|
| **R** | `add`, `sub`, `and`, `or`, `sll`… | ✅ | ✅ | ✅ | ✅ | ✅ | — |
| **I** | `addi`, `lw`, `jalr`, `slli`… | ✅ | ✅ | — | ✅ | — | 12 bits |
| **S** | `sw`, `sh`, `sb` | — | ✅ | ✅ | ✅ | — | 12 bits |
| **B** | `beq`, `bne`, `blt`, `bltu`… | — | ✅ | ✅ | ✅ | — | 13 bits |
| **U** | `lui`, `auipc` | ✅ | — | — | — | — | 32 bits |
| **J** | `jal` | ✅ | — | — | — | — | 21 bits |

### O mapa de bits de cada formato

```
Tipo R   ┌─ funct7 ─┬─ rs2 ─┬─ rs1 ─┬ f3 ┬─ rd ─┬ opcode ┐
         31        25      20      15   12     7        0

Tipo I   ┌──── imm[11:0] ────┬─ rs1 ─┬ f3 ┬─ rd ─┬ opcode ┐
         31                 20      15   12     7        0

Tipo S   ┌ imm[11:5] ┬─ rs2 ─┬─ rs1 ─┬ f3 ┬ imm[4:0] ┬ opcode ┐
         31         25      20      15   12         7        0

Tipo B   ┌ imm[12|10:5] ┬ rs2 ┬ rs1 ┬ f3 ┬ imm[4:1|11] ┬ opcode ┐
         31            25    20    15   12            7        0

Tipo U   ┌──────────── imm[31:12] ────────────┬─ rd ─┬ opcode ┐
         31                                  12     7        0

Tipo J   ┌──── imm[20|10:1|11|19:12] ────┬─ rd ─┬ opcode ┐
         31                             12     7        0
```

### ⚠️ A armadilha principal do trabalho

Nos tipos **S** e **B**, os bits 11 a 7 — que em outros formatos seriam o `rd` —
**fazem parte do imediato**. Nos tipos **U** e **J**, os bits 19 a 12 — que
seriam `rs1` e `funct3` — também são imediato.

Um decodificador descuidado lê os bits 11–7 sempre como `rd` e informa, num
`beq`, um `rd = 25` que **não existe**. Na Etapa 2 isso vira um conflito falso.

Nossa solução em C++: todo campo é um `std::optional`. Se o formato não tem o
campo, ele fica **vazio de verdade** (`std::nullopt`) — não há como imprimir
lixo por engano.

```cpp
std::optional<uint8_t> rd;   // vazio nos tipos S e B
std::optional<uint8_t> rs1;  // vazio nos tipos U e J
```

---

## 7. Números negativos e extensão de sinal

### Complemento de dois em 30 segundos

Computadores representam negativos assim: o bit mais à esquerda é o **sinal**.
Se ele é 1, o número é negativo. Para achar o valor, inverta todos os bits e
some 1.

Em 4 bits:

| Binário | Valor |
|---|---|
| `0011` | +3 |
| `0000` | 0 |
| `1111` | −1 |
| `1000` | −8 |

### O que é extensão de sinal

Um imediato do tipo I tem **12 bits**, mas a CPU trabalha com **32 bits**. Como
transformar um no outro sem mudar o valor?

- Se o número é positivo, preenche-se a esquerda com **zeros**.
- Se é negativo, preenche-se a esquerda com **uns**.

```
12 bits:  1111 1111 1000          (= −8)
32 bits:  1111 1111 1111 1111 1111 1111 1111 1000   (= −8, ainda)
```

Repare que copiar o bit de sinal preserva o valor. Isso é a **extensão de
sinal**, e no nosso código é a função:

```cpp
int32_t signExtend(uint32_t value, int width) {
    const uint32_t signBit = 1u << (width - 1);          // o bit mais alto do campo
    const uint32_t mask = (1u << width) - 1u;            // máscara do campo
    value &= mask;
    if (value & signBit) value |= ~mask;                 // negativo: preenche com 1s
    return static_cast<int32_t>(value);
}
```

Sem isso, um desvio de −8 seria lido como +8184, e o programa saltaria para o
lugar errado.

---

## 8. Por que os imediatos de B e J são embaralhados

Esta é a parte que mais assusta, e a explicação é simples: **é para o hardware
ficar mais barato**.

Os projetistas do RISC-V posicionaram os bits de modo que, comparando os
formatos, **cada bit do imediato caia quase sempre na mesma posição física** da
instrução. Assim o circuito que monta o imediato é praticamente um punhado de
fios, sem multiplexadores caros. Repare, por exemplo, que o bit de sinal está
**sempre** no bit 31, em todos os formatos.

O preço disso é que, em software, precisamos remontar o número na mão.

### Tipo B (desvios) — 13 bits

| Pedaço do imediato | Onde está na instrução |
|---|---|
| `imm[12]` (sinal) | bit 31 |
| `imm[11]` | bit 7 |
| `imm[10:5]` | bits 30–25 |
| `imm[4:1]` | bits 11–8 |
| `imm[0]` | **não existe: é sempre 0** |

```cpp
const uint32_t imm = (bits(word, 31, 31) << 12) |   // sinal
                     (bits(word,  7,  7) << 11) |   // o bit "fora de ordem"
                     (bits(word, 30, 25) <<  5) |
                     (bits(word, 11,  8) <<  1);    // <<1 deixa o bit 0 zerado
return signExtend(imm, 13);
```

### Tipo J (`jal`) — 21 bits

| Pedaço do imediato | Onde está na instrução |
|---|---|
| `imm[20]` (sinal) | bit 31 |
| `imm[19:12]` | bits 19–12 |
| `imm[11]` | bit 20 |
| `imm[10:1]` | bits 30–21 |
| `imm[0]` | **sempre 0** |

### Por que o bit 0 é sempre zero?

Instruções ficam em endereços múltiplos de 2 (na prática, de 4). Um desvio nunca
vai para um endereço ímpar. Guardar esse bit seria desperdício — então ele é
subentendido, e o alcance do desvio **dobra** de graça.

### Endereço absoluto do destino

O imediato é um **deslocamento relativo ao PC**. O endereço real é:

```
destino = PC + imediato
```

O enunciado pede que a saída mostre o destino absoluto, não o deslocamento:

```cpp
std::optional<uint32_t> Instruction::branchTarget() const {
    if (format != Format::B && format != Format::J) return std::nullopt;
    return pc + static_cast<uint32_t>(*imm);
}
```

> `jalr` é a exceção: seu destino depende do **valor** de um registrador em
> tempo de execução, que o decodificador não conhece. Por isso não calculamos
> alvo para ele.

---

## 9. Decodificando quatro instruções na mão

São os exemplos do enunciado — os melhores candidatos a cair na defesa.

### 9.1 `0x00500413` → `addi s0, zero, 5`

Em binário:

```
0000 0000 0101 0000 0000 0100 0001 0011
```

Fatiando de trás para frente:

| Bits | Valor | Campo | Leitura |
|---|---|---|---|
| 6–0 | `0010011` = 0x13 | opcode | operação com imediato → **tipo I** |
| 11–7 | `01000` = 8 | rd | `x8` = `s0` |
| 14–12 | `000` = 0 | funct3 | `addi` |
| 19–15 | `00000` = 0 | rs1 | `x0` = `zero` |
| 31–20 | `000000000101` = 5 | imediato | +5 (positivo, extensão com zeros) |

**Resultado:** `addi s0, zero, 5` → "s0 = 0 + 5".

### 9.2 `0x00C58633` → `add a2, a1, a2`

| Bits | Valor | Campo | Leitura |
|---|---|---|---|
| 6–0 | `0110011` = 0x33 | opcode | registrador-registrador → **tipo R** |
| 11–7 | 12 | rd | `x12` = `a2` |
| 14–12 | 0 | funct3 | família add/sub |
| 19–15 | 11 | rs1 | `x11` = `a1` |
| 24–20 | 12 | rs2 | `x12` = `a2` |
| 31–25 | 0x00 | funct7 | **0x00 → `add`** (fosse 0x20, seria `sub`) |

**Resultado:** `add a2, a1, a2` → "a2 = a1 + a2".

### 9.3 `0x0064A423` → `sw t1, 8(s1)`

| Bits | Valor | Campo | Leitura |
|---|---|---|---|
| 6–0 | `0100011` = 0x23 | opcode | store → **tipo S** |
| 14–12 | 2 | funct3 | `sw` (palavra inteira) |
| 19–15 | 9 | rs1 | `x9` = `s1` — o endereço-base |
| 24–20 | 6 | rs2 | `x6` = `t1` — o valor a gravar |
| 31–25 + 11–7 | `0000000` + `01000` = 8 | imediato | deslocamento +8 |
| — | — | **rd** | **não existe neste formato** |

**Resultado:** `sw t1, 8(s1)` → "grave o conteúdo de t1 na memória, no endereço
s1 + 8".

### 9.4 `0xFE628CE3` → `beq t0, t1, −8` — a armadilha

Em binário:

```
1111 1110 0110 0010 1000 1100 1110 0011
```

| Bits | Valor | Campo | Leitura |
|---|---|---|---|
| 6–0 | `1100011` = 0x63 | opcode | desvio → **tipo B** |
| 14–12 | 0 | funct3 | `beq` |
| 19–15 | 5 | rs1 | `x5` = `t0` |
| 24–20 | 6 | rs2 | `x6` = `t1` |
| 11–7 | `11001` = 25 | ⚠️ | **NÃO é rd!** São `imm[4:1|11]` |

Remontando o imediato:

```
imm[12] = bit 31       = 1
imm[11] = bit  7       = 1
imm[10:5] = bits 30-25 = 111111
imm[4:1]  = bits 11-8  = 1100
imm[0]                 = 0
                       ─────────────────
imm (13 bits)          = 1 1 111111 1100 0
```

`1111111111000` com extensão de sinal = **−8**.

Se a instrução estiver no endereço `0x24`, o destino é `0x24 − 8 = 0x1C` — um
desvio para trás, típico de laço de repetição.

**Resultado:** `beq t0, t1, 0x0000001C`, **sem `rd`**.

> Esse é o ponto que o professor destacou: informar `rd = 25` aqui é o erro
> clássico, e ele só aparece de verdade na Etapa 2, como um conflito inventado.

---

## 10. Pseudo-instruções

Algumas instruções muito comuns não existem no hardware — o montador as escreve
usando outras instruções. Elas se chamam **pseudo-instruções**. O decodificador
deve **reconhecê-las de volta** na saída, porque é assim que o programador as
escreveu.

| Você escreve | O hardware executa | Por que funciona |
|---|---|---|
| `nop` | `addi x0, x0, 0` | Soma 0 a zero e joga em `x0`: não faz nada |
| `mv a1, a0` | `addi a1, a0, 0` | Somar zero é copiar |
| `li a0, 5` | `addi a0, x0, 5` | 0 + 5 = 5 |
| `ret` | `jalr x0, 0(ra)` | Pula para o endereço guardado em `ra` e descarta o retorno |
| `j destino` | `jal x0, destino` | Salta sem guardar endereço de retorno |
| `not a0, a1` | `xori a0, a1, -1` | XOR com todos os bits 1 inverte tudo |
| `neg a0, a1` | `sub a0, x0, a1` | 0 − a1 |
| `beqz a0, dest` | `beq a0, x0, dest` | Comparar com `zero` |

Quase todas se apoiam no truque do `x0`. No nosso código isso está em
`toPseudo()`, no `src/disassembler.cpp`, e aparece na listagem como
`; pseudo: nop`.

---

## 11. CPI e média ponderada

**CPI** = *Cycles Per Instruction*, ciclos por instrução. Nem toda instrução leva
o mesmo tempo: um `add` resolve em um ciclo; um `lw` precisa esperar a memória.

O trabalho pede o **CPI médio** de um programa, que é uma **média aritmética
ponderada** — cada classe de instrução pesa conforme quantas vezes aparece:

```
              Σ (quantidade da classe × CPI da classe)
CPI médio  =  ─────────────────────────────────────────
                    total de instruções
```

### Exemplo com o nosso programa de teste

| Classe | Quantidade | CPI | Ciclos |
|---|---:|---:|---:|
| alu | 6 | 1,0 | 6,00 |
| load | 1 | 2,0 | 2,00 |
| store | 1 | 1,0 | 1,00 |
| branch | 2 | 2,0 | 4,00 |
| jump | 3 | 2,0 | 6,00 |
| upper | 2 | 1,0 | 2,00 |
| system | 1 | 1,0 | 1,00 |
| **Total** | **16** | | **22,00** |

```
CPI médio = 22,00 / 16 = 1,375
```

Ou seja: em média, cada instrução desse programa custa 1,375 ciclos de clock.

> **Cuidado com o erro comum:** a média **não** é a média simples dos CPIs da
> tabela. Uma instrução que aparece 50 vezes pesa 50 vezes mais que uma que
> aparece uma vez.

A tabela de CPI é **entrada do programa** (`tests/data/cpi.txt`), não está
fixa no código — assim o professor pode testar com outros valores.

---

## 12. Como o nosso programa está organizado

O caminho dos dados é uma linha reta, e cada estação faz uma coisa só:

```
arquivo .hex/.bin
       │
       ▼
   ┌────────┐   lê linhas, ignora comentários, detecta hex ou binário,
   │ parser │   atribui o PC de cada palavra
   └────────┘
       │  (pc, palavra de 32 bits)
       ▼
   ┌─────────┐   fatia os bits, escolhe o formato pelo opcode, identifica o
   │ decoder │   mnemônico por funct3/funct7, remonta o imediato
   └─────────┘
       │  Instruction { pc, raw, formato, mnemônico, rd?, rs1?, rs2?, f3?, f7?, imm? }
       ├──────────────────────────┐
       ▼                          ▼
┌──────────────┐          ┌────────────┐
│ disassembler │          │ statistics │
│ nomes ABI,   │          │ contagem   │
│ pseudo,      │          │ por formato│
│ alvo         │          │ e CPI      │
└──────────────┘          └────────────┘
       │                          │
       └──────────┬───────────────┘
                  ▼
                main  (imprime tudo)
```

| Arquivo | Responsabilidade | Requisito |
|---|---|---|
| `src/parser.cpp` | Ler a entrada, detectar hex/binário, atribuir PC | R1 |
| `src/decoder.cpp` | Formato, mnemônico, campos, imediatos e os ganchos da Etapa 2 | R2, R3 |
| `src/disassembler.cpp` | Assembly com nomes ABI, pseudo, alvo absoluto | R4 |
| `src/statistics.cpp` | Distribuição por formato e CPI médio | R5 |
| `src/main.cpp` | Linha de comando e impressão | R4 |
| `tests/test_decoder.cpp` | Testes automatizados | validação |
| `include/riscv.hpp` | Interface pública: `Instruction` e as funções de cada etapa | — |

### As duas decisões de projeto que valem explicar na defesa

**1. `std::optional` para todo campo.**
Impede na raiz o erro do `rd` fantasma: um campo que não existe no formato não
tem valor nenhum, e o compilador obriga a tratar isso.

**2. Ganchos prontos para a Etapa 2.**

```cpp
std::vector<uint8_t> readsRegisters() const;    // registradores lidos (sem x0)
std::optional<uint8_t> writesRegister() const;  // registrador escrito (vazio se x0)
```

Detectar um hazard RAW ("leia depois de escrever") na Etapa 2 vira comparar o
`writesRegister()` de uma instrução com o `readsRegisters()` das seguintes.

---

## 13. Como rodar

```bash
# compilar
cmake -S . -B build
cmake --build build

# rodar com a tabela de CPI
./build/riscv-decoder tests/data/programa.hex --cpi tests/data/cpi.txt

# a mesma ROM em binário, começando em outro endereço
./build/riscv-decoder tests/data/programa.bin --base 0x00400000

# testes automatizados
./build/test_decoder
```

Saída (trecho):

```
ENDERECO    PALAVRA     FMT  MNEM     CAMPOS                                      ASSEMBLY
0x00000000  0x00500413  I    addi     rd=8 rs1=0 f3=0x0 imm=5                     addi s0, zero, 5   ; pseudo: li s0, 5
0x00000018  0x0064A423  S    sw       rs1=9 rs2=6 f3=0x2 imm=8                    sw t1, 8(s1)
0x00000024  0xFE628CE3  B    beq      rs1=5 rs2=6 f3=0x0 imm=-8 alvo=0x0000001C    beq t0, t1, 0x0000001C
```

Repare: no `sw` e no `beq` **não aparece `rd`**. É o requisito R3 funcionando.

---

## 14. Preparação para a defesa

A defesa é individual: cada integrante precisa saber explicar qualquer parte.
Estas são as perguntas mais prováveis.

**Como você descobre o formato de uma instrução?**
Pelo opcode, bits 6–0. Cada opcode pertence a um único formato: `0x33` é R,
`0x13`/`0x03`/`0x67` são I, `0x23` é S, `0x63` é B, `0x37`/`0x17` são U e `0x6F`
é J.

**Formato e mnemônico são a mesma coisa?**
Não. O formato diz o *arranjo dos bits*; o mnemônico diz *qual operação é*.
O opcode `0x33` é sempre tipo R, mas pode ser `add`, `sub`, `xor`, `sll`… A
distinção vem de `funct3` e `funct7`.

**Como você diferencia `add` de `sub`?**
Mesmo opcode (`0x33`) e mesmo `funct3` (`0x0`). Muda o `funct7`: `0x00` é `add`,
`0x20` é `sub`. O mesmo vale para `srl`/`sra` e `srli`/`srai` — é o bit 30.

**Por que o tipo B não tem `rd`?**
Porque um desvio não produz resultado para guardar; ele só altera o PC. Os bits
11–7, que em outros formatos seriam `rd`, são usados para carregar parte do
imediato — `imm[4:1]` e `imm[11]`.

**Como você remonta o imediato do tipo B?**
Junto quatro pedaços: bit 31 → `imm[12]`, bit 7 → `imm[11]`, bits 30–25 →
`imm[10:5]`, bits 11–8 → `imm[4:1]`; o bit 0 é sempre 0. Depois aplico extensão
de sinal de 13 bits.

**O que é extensão de sinal e por que é necessária?**
É replicar o bit de sinal ao ampliar um número para 32 bits, para preservar o
valor. Sem ela, um deslocamento de −8 viraria um número positivo grande e o
desvio iria para o lugar errado.

**Como o programa sabe se o arquivo é hexadecimal ou binário?**
Ele olha os dígitos de todas as linhas: se houver qualquer dígito de `a` a `f`,
só pode ser hexadecimal; se houver linhas com mais de 8 dígitos usando apenas
`0` e `1`, é binário. Um prefixo explícito (`0x`, `0b`) decide a linha na hora.

**O que acontece se a palavra não for uma instrução válida?**
Ela é marcada como inválida, com o endereço e o motivo, e o programa **continua**
processando o restante do arquivo — como pede o R2.

**Como o CPI médio é calculado?**
Média aritmética ponderada: soma-se `quantidade × CPI` de cada classe e divide-se
pelo total de instruções. A tabela de CPI vem de um arquivo de entrada.

**Por que essa decodificação importa para a Etapa 2?**
Porque a detecção de hazards compara o registrador escrito por uma instrução com
os registradores lidos pelas seguintes. Um campo errado — como um `rd` fantasma
num `beq` — produz um conflito que não existe.

---

## 15. Glossário

| Termo | Significado |
|---|---|
| **ABI** | Convenção de uso dos registradores; dá os apelidos (`sp`, `a0`, `s0`…) |
| **Assembly** | A forma legível de escrever instruções (`add a2, a1, a2`) |
| **Bit / byte** | 1 dígito binário / 8 bits |
| **Complemento de dois** | Forma de representar números negativos em binário |
| **CPI** | Ciclos por instrução |
| **Decodificar** | Descobrir, a partir dos bits, qual instrução é e quais seus operandos |
| **Desmontar (*disassemble*)** | Converter código de máquina de volta em assembly |
| **Extensão de sinal** | Ampliar um número mantendo seu valor (copiando o bit de sinal) |
| **funct3 / funct7** | Campos que refinam o opcode até o mnemônico exato |
| **Hazard** | Conflito no pipeline, quando uma instrução depende de outra ainda não concluída (Etapa 2) |
| **Hexadecimal** | Base 16; cada dígito vale 4 bits |
| **Imediato** | Constante embutida na própria instrução |
| **Linguagem de máquina** | Os bits que o processador executa |
| **Mnemônico** | O nome da instrução (`add`, `beq`, `lw`) |
| **opcode** | Os 7 bits que dizem o tipo da instrução |
| **PC** | *Program Counter*, endereço da instrução atual |
| **Pipeline** | Técnica de executar partes de várias instruções ao mesmo tempo |
| **Pseudo-instrução** | Atalho de escrita traduzido para instruções reais (`nop`, `mv`, `ret`) |
| **rd / rs1 / rs2** | Registrador de destino / fontes 1 e 2 |
| **Registrador** | Memória minúscula e rapidíssima dentro da CPU |
| **RISC-V** | Conjunto de instruções aberto usado no trabalho |
| **ROM de instruções** | O arquivo com o programa em linguagem de máquina |
