# Índice de dívidas e pendências

⚠️ **Como esta lista foi produzida:** leitura completa de `src/main.c`, dos seis módulos de `lib/`,
do `zephyr/frdm_kl25z.overlay`, do `prj.conf`, do `CMakeLists.txt` e do `platformio.ini`, em
2026-09-02. **O projeto não foi compilado nem executado nesta varredura** — são bugs de
**inspeção**, não de observação. Onde há incerteza, está dito no item.

O **D-19** e a parte verificada do **P-02** vêm de uma segunda passada, também em 2026-09-02, lendo
o framework do Zephyr em `~/.platformio/packages/framework-zephyr` (driver `adc_mcux_adc16`,
`Kconfig.mcux`, `include/zephyr/drivers/adc.h` e a dts/pinctrl da placa). Essas conclusões são
leitura de fonte, não memória — os arquivos e as linhas estão citados nos itens.

Os IDs são **estáveis**: as specs de [../02-software_specs/](../02-software_specs/) citam por ID e
não redescrevem o bug. Não renumere; itens resolvidos ficam com o status mudado.

`D-nn` = defeito no que existe. `P-nn` = pendência, algo a decidir ou medir.

⭐ **2026-09-02: os sete críticos (D-01…D-07) foram corrigidos**, mais o D-19 que era par
indivisível do D-03. Ver [01-bugs-criticos.md](01-bugs-criticos.md), inclusive o que foi e o que
**não** foi verificado. O D-30 foi resolvido por commit. Nasceu o **D-40** — o projeto não linkava
nesta máquina — e ele foi **resolvido em 2026-09-14**: era cache de build carregando duas grafias do
mesmo caminho, não o Zephyr nem o código. Desde então **existe binário**.

## Defeitos

| ID | Título | Severidade | Arquivo | Status |
|---|---|---|---|---|
| **D-01** | `ldr_ler` devolve `-1.0` como se fosse leitura | 🔴 alta | [01](01-bugs-criticos.md) | ✅ **corrigido** |
| **D-02** | TPM0 e ADC inicializados três vezes, com `\|=` | 🔴 alta | [01](01-bugs-criticos.md) | ✅ **corrigido** |
| **D-03** | A "conversão para mV" não converte nada | 🔴 alta | [01](01-bugs-criticos.md) | ✅ **corrigido** |
| **D-04** | `acos`/`asin` sem clamp → NaN → `(int)NaN` é UB | 🔴 alta | [01](01-bugs-criticos.md) | ✅ **corrigido** |
| **D-05** | Servo **envolve** o ângulo em vez de saturar | 🔴 alta | [01](01-bugs-criticos.md) | ✅ **corrigido** |
| **D-06** | 12 min 40 s de teste bloqueante em todo boot | 🔴 alta | [01](01-bugs-criticos.md) | ✅ **corrigido** |
| **D-07** | `testes.c` chama servo sem incluir o header | 🟡 média | [01](01-bugs-criticos.md) | ✅ **corrigido** |
| **D-10** | Filtro quadrático incoerente com o próprio log | 🟠 alta | [02](02-bugs-de-controle-e-matematica.md) | aberto |
| **D-11** | θ (ângulo de incidência) usado como posição do servo | 🔴 alta | [02](02-bugs-de-controle-e-matematica.md) | aberto |
| **D-12** | Clamp antes do teste, e recomando no limite | 🟡 média | [02](02-bugs-de-controle-e-matematica.md) | aberto |
| **D-13** | Modo noturno usa a variável do horizontal nos dois eixos | 🟡 média | [02](02-bugs-de-controle-e-matematica.md) | aberto |
| **D-14** | Servo bloqueante torna a amostragem irregular | 🟠 alta | [02](02-bugs-de-controle-e-matematica.md) | aberto |
| **D-15** | `theta` truncado **antes** de ser arredondado | 🟢 baixa | [02](02-bugs-de-controle-e-matematica.md) | aberto |
| **D-16** | Dia do ano aproximado, e **literais octais** na data | 🟡 média | [02](02-bugs-de-controle-e-matematica.md) | aberto |
| **D-17** | Relógio sem data, e volta a uma hora fixa a cada boot | 🟠 alta | [02](02-bugs-de-controle-e-matematica.md) | aberto |
| **D-18** | Comentário do overlay chama pino externo de canal interno | 🟢 baixa | [02](02-bugs-de-controle-e-matematica.md) | aberto |
| **D-19** | `ADC_REF_INTERNAL` faz `adc_raw_to_millivolts_dt()` devolver 0 mV **com sucesso** | 🔴 alta | [02](02-bugs-de-controle-e-matematica.md) | ✅ **corrigido** |
| **D-20** | Validação de pino usa `\|\|` onde queria `&&`: inócua | 🟡 média | [03](03-bugs-na-lib-pwm-vendorizada.md) | **não corrigir** |
| **D-21** | Ramo `GPIOC` configura o registrador e depois falha | 🟡 média | [03](03-bugs-na-lib-pwm-vendorizada.md) | **não corrigir** |
| **D-30** | `lib/servo_motor/` e `lib/testes/` **untracked** | 🔴 crítica | [04](04-dividas-de-projeto-e-repositorio.md) | ✅ **resolvido** |
| **D-31** | Repositório git órfão em `src/.git/` | 🟡 média | [04](04-dividas-de-projeto-e-repositorio.md) | aberto |
| **D-32** | Acento no nome de diretório, typo no nome de arquivo | 🟡 média | [04](04-dividas-de-projeto-e-repositorio.md) | aberto |
| **D-33** | README, CMake e `.vscode` são do carrinho, não do tracker | 🟡 média | [04](04-dividas-de-projeto-e-repositorio.md) | aberto |
| **D-34** | Headers sem include guard, macros duplicados, protótipo vazio | 🟡 média | [04](04-dividas-de-projeto-e-repositorio.md) | aberto |
| **D-35** | Variáveis e macros mortos | 🟢 baixa | [04](04-dividas-de-projeto-e-repositorio.md) | aberto |
| **D-36** | `CONFIG_ADC_ASYNC` e `CONFIG_FPU` sem efeito | 🟢 baixa | [04](04-dividas-de-projeto-e-repositorio.md) | aberto |
| **D-37** | `sample_buffer` `static` compartilhado entre canais | 🟢 baixa | [04](04-dividas-de-projeto-e-repositorio.md) | aberto |
| **D-40** | `pio run` não concluía: falha ao gerar `offsets.h` do Zephyr | 🔴 crítica | [04](04-dividas-de-projeto-e-repositorio.md) | ✅ **resolvido** |
| **D-43** | Mexer no overlay **não** regenera o devicetree: `pio run` reusa o `.dts` antigo | 🟠 alta | [04](04-dividas-de-projeto-e-repositorio.md) | aberto |

## Pendências

Todas em [05-pendencias-de-hardware.md](05-pendencias-de-hardware.md).

| ID | Pergunta | Bloqueia |
|---|---|---|
| ~~**P-01**~~ | ~~Quais pinos ADC para os 2 LDRs novos?~~ | ✅ **resolvido**: PTB1 (`SE9`) e PTB3 (`SE13`) |
| **P-02** | O acoplamento servo→LDR pela rede de GND é tolerável? | ⭐ a confiabilidade de **toda** leitura de LDR |
| ~~**P-03**~~ | ~~Divisor é pull-up ou pull-down, e com que resistor?~~ | ✅ **resolvido**: pull-down com 10 kΩ — mais luz = mais tensão |
| ~~**P-04**~~ | ~~Faixa real de mV dos LDRs no local?~~ | ✅ **resolvido**: 10²–10³ mV (o resto é medição do passo 1 do bringup) |
| ~~**P-05**~~ | ~~Altura da aleta do X?~~ | ❌ **encerrada**: fora de escopo (DEC-14) — o ganho é medido, não previsto |
| ~~**P-06**~~ | ~~Batente mecânico do eixo vertical?~~ | ✅ **resolvido**: não há — o batente é o do servo; vertical satura em 10°–160° |
| ~~**P-07**~~ | ~~Painel fotovoltaico real ou chapa?~~ | ❌ **encerrada**: fora de escopo (DEC-15) — sem critério de sucesso externo ao firmware |
| **P-08** | Qual a faixa real de pulso do MG90S? | trocar os fatores empíricos por medição |
| ~~**P-09**~~ | ~~Para onde aponta o servo horizontal em 0°?~~ | ❌ **encerrada**: horizontal em malha fechada pura (DEC-13) |
| **P-10** | Quanta queda IR **diferencial** o trilho de GND introduz? | ⭐ o mecanismo que a subtração não cobre |

## Se você só vai consertar três coisas

1. **D-11** — o eixo vertical é comandado por uma função de custo. É o bug que faz o produto não
   fazer o que promete, e o mais caro que continua aberto.
2. **D-14** — o servo bloqueante torna o período de amostragem irregular, e por isso nenhum filtro
   em cima dele significa o que promete. É pré-requisito do controle de 4 quadrantes.
3. **D-10** — o filtro de votação contradiz o que o próprio log afirma, e mistura variação temporal
   com diferença espacial. Está no mesmo caminho dos dois acima, e morre junto quando a mediana de
   **DEC-3** entrar.

~~D-30~~ (resolvido por commit), ~~D-06~~ e ~~D-19~~ (corrigidos) saíram desta lista em
2026-09-02. ~~D-40~~ saiu em **2026-09-14**, com o build destravado — e é o que promoveu os três
acima, que até então estavam atrás dele.
