# Dívidas de projeto e repositório

Não são bugs de runtime: são armadilhas de manutenção. A primeira tem risco de **perda total do
trabalho** e é a mais urgente da lista inteira.

---

## D-30 — `lib/servo_motor/` e `lib/testes/` estão **untracked**

⛔ **A dívida mais urgente do projeto.**

```
$ git status --short
 M src/main.c
?? lib/servo_motor/
?? lib/testes/
```

O último commit é `d7e4db2` — *"Código antigo, usando apenas 2 LDR's e servo motores sg90 simples"*.
Depois dele veio o refactor que **criou a arquitetura atual**: as funções de servo saíram do
`main.c` para `lib/servo_motor/`, o `main()` de teste antigo virou `lib/testes/`, e o `main.c`
ficou só com o laço de tracking (92 linhas removidas, 13 acrescentadas em relação ao HEAD).

Nada disso está versionado. **Quatro arquivos de código existem apenas nesta máquina**, e um
`git clean -fd` — o comando que todo mundo roda para limpar artefatos de build — apaga os dois
diretórios sem confirmação.

Não há nada a projetar aqui: é `git add lib/servo_motor lib/testes src/main.c` e um commit.
Custo de minutos, risco evitado de refazer o refactor de memória.

⚠️ Notar que **as specs de [../02-software_specs/](../02-software_specs/) renomeiam esses dois
diretórios** (D-32). Commitar **antes** de renomear, senão o git vê a renomeação como criação e o
histórico do refactor se perde de vez.

---

## D-31 — Repositório git órfão em `src/.git/`

Existe um **segundo** repositório git dentro de `src/`, com remoto
`https://github.com/bmchad/carrinho_tracker_3_sensores.git` e um objeto solto de 311 KB. O git da
raiz não o trata como submódulo nem como repo aninhado ativo — é lixo da era do carrinho.

Enquanto está lá: qualquer ferramenta que caminhe pela árvore procurando `.git` pode se confundir
sobre qual repositório é o do projeto, e `src/` aparece como raiz de repo em editores e em scripts.

Confirmar que não há nada de valor só nele (o remoto ainda existe no GitHub) e remover.

---

## D-32 — Acento no nome de diretório, typo no nome de arquivo

| Caminho atual | Problema |
|---|---|
| `lib/angulo_incidência_solar/` | **`ê` não-ASCII** no nome do diretório |
| `lib/servo_motor/sevo_angulo_tenpo.c` | typo em duas letras: "sevo", "tenpo" |

O acento é o mais sério dos dois. Caminho não-ASCII atravessa CMake, Ninja, o shell do Windows e o
toolchain ARM, e cada um tem sua própria opinião sobre codificação de página de código. Funciona
nesta máquina; é exatamente o tipo de coisa que quebra em outra, ou num runner de CI, com mensagem
de erro que não menciona o acento.

O typo é cosmético, mas dobra: `sevo_angulo_tenpo.c` implementa o que `servo_angulo_tempo.h`
declara — os dois nomes são diferentes, e procurar pelo arquivo pelo nome do header não o encontra.

As specs já preveem os dois renomeados para `lib/sol/` e `lib/servo/servo.c` — ver
[../02-software_specs/00-arquitetura-e-ordem-de-execucao.md](../02-software_specs/00-arquitetura-e-ordem-de-execucao.md).
⚠️ Resolver **D-30 antes**, para o git registrar renomeação e não perda + criação.

---

## D-33 — README, CMake e `.vscode` são do carrinho, não do tracker

Três resquícios da origem do projeto (`carrinho_seguindo_linha` →
`carrinho_tracker_3_sensores` → tracker solar):

| Onde | O que diz | O que deveria |
|---|---|---|
| `README.MD` | descreve **o carrinho seguidor de linha** inteiro: rampa de PWM, detecção de curva, o zig-zag por sensores próximos, e um link de vídeo no Drive | descrever o tracker |
| `zephyr/CMakeLists.txt` | `project(carrinho1)` | `project(tracker_solar)` |
| `.vscode/settings.json` | `cmake.sourceDirectory` aponta para **outro projeto**: `.../Projects/carrinho_circuito_sensor/zephyr` | este projeto |

O `README.MD` é o pior porque é a **primeira** coisa que alguém lê, e ele não está desatualizado —
ele é de outro produto. Ironia útil: o problema que ele relata (*"os sensores estão muito próximos,
deixando o carrinho por vezes preso em zig-zag"*) é o mesmo risco que a zona morta e a geometria
óptica do X existem para evitar aqui. Vale citar isso no README novo.

`.vscode/` está no `.gitignore`, então o terceiro item é só local — mas é o que faz o
IntelliSense apontar para os arquivos errados.

---

## D-34 — Headers sem include guard, macros duplicados, protótipo vazio

**`lib/servo_motor/servo_angulo_tempo.h`** e **`lib/testes/testes.h`**, os dois:

1. **Sem include guard.** Nenhum `#ifndef`. Nada impede dupla inclusão; hoje passa porque nenhum
   dos dois inclui o outro.
2. **Arrastam o Zephyr inteiro:** `kernel.h`, `device.h`, `gpio.h`, `pwm_z42.h`, `ldr.h`,
   `init_hora.h`, `angulo_incidencia_solar.h`. Um header de servo não precisa saber que existe um
   relógio, muito menos matemática solar. Todo consumidor herda tudo, e o grafo de dependências
   deixa de significar algo.
3. **Redefinem macros de outros arquivos:** `TPM_MODULE`, `SLEEP_TIME_MS`, `INPUT_PORT`,
   `INPUT_PIN` — os mesmos quatro que `src/main.c` e `lib/testes/testes.c` também definem, dando
   **quatro** definições de `TPM_MODULE 7500` na árvore. Redefinição **idêntica** de macro é legal
   em C, então o compilador não reclama; no dia em que um dos quatro mudar de valor, o resultado
   passa a depender da ordem de inclusão, e o aviso vem de um arquivo que ninguém tocou.

E em `lib/testes/testes.h`:

```c
int teste_motores();   /* lista vazia: não é protótipo */
int teste_ldr();
```

Lista de parâmetros vazia em C **não** significa "sem argumentos" — significa "não declarado", e o
compilador para de conferir as chamadas. `int teste_motores(void)` é o que o `.c` de fato define.
Em C23 a lista vazia passou a significar `(void)`, o que resolve por baixo, mas não é o padrão em
uso aqui.

---

## D-35 — Variáveis e macros mortos

| Item | Onde | Nota |
|---|---|---|
| `int direcao = 1;` | `src/main.c:57` | atribuído, nunca lido |
| `double N` | `angulo_incidencia_solar.c:111` | duração da insolação, calculada e descartada |
| `static int ano` | `angulo_incidencia_solar.c:13` | declarado, nunca usado — é a **ausência dele** que faz o dia do ano ignorar ano bissexto (D-16) |
| `INPUT_PORT` / `INPUT_PIN` | `main.c`, `testes.c`, `servo_angulo_tempo.h` | gpiod pino 20; resquício do sensor de linha do carrinho |
| `SLEEP_TIME_MS` | os mesmos três arquivos | nunca usado |

`N` e `ano` merecem nota: os dois são *quase* usados. `N` seria útil para decidir dia e noite pela
duração real da insolação em vez do `>= 18 || < 6` cravado (D-13), e `ano` é o que falta para o dia
do ano estar certo. Apagá-los é uma opção; a outra é usá-los.

---

## D-36 — `CONFIG_ADC_ASYNC` e `CONFIG_FPU` sem efeito

**Onde:** `zephyr/prj.conf`.

```
CONFIG_FPU=y
CONFIG_ADC_ASYNC=y
```

- **`CONFIG_FPU=y` num Cortex-M0+.** O MKL25Z4 **não tem FPU**. A opção é inócua, e o efeito
  colateral é de expectativa: todo o cálculo solar usa `double` e roda em **soft-float**, o que num
  M0+ a 48 MHz custa milhares de ciclos por operação transcendental. Não é problema hoje (o cálculo
  roda uma vez a cada 300 ciclos do laço), mas quem ler `CONFIG_FPU=y` vai supor hardware que não
  existe.
- **`CONFIG_ADC_ASYNC=y`** habilita a API assíncrona do ADC, e o código usa só `adc_read()`
  síncrono. Custa código em flash sem consumidor.

`CONFIG_CBPRINTF_FP_SUPPORT=y`, por outro lado, é **necessário**: `lib/testes/testes.c` imprime com
`%f`. Não remover junto.

---

## D-37 — `sample_buffer` `static` compartilhado entre canais

**Onde:** `lib/ldr/ldr.c:15`.

```c
static int16_t sample_buffer[1];
```

Um único buffer para todos os canais, sem lock. **Fecha hoje**: há uma thread só, e as leituras são
sequenciais.

Passa a ser corrida no dia em que o controle virar thread própria — o cenário natural para resolver
**D-14** (tirar a espera bloqueante do laço). Registrado aqui para que essa mudança não seja feita
sem lembrar deste arquivo. A spec da API nova mantém o padrão atual e **não** resolve isto:
[../02-software_specs/02-api-ldr-4-sensores.md](../02-software_specs/02-api-ldr-4-sensores.md).
