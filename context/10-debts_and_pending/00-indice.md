# Índice de dívidas e pendências

⚠️ **Como esta lista foi produzida:** leitura completa de `src/main.c`, dos seis módulos de `lib/`,
do `zephyr/frdm_kl25z.overlay`, do `prj.conf`, do `CMakeLists.txt` e do `platformio.ini`, em
2026-09-02. **O projeto não foi compilado nem executado nesta varredura** — são bugs de
**inspeção**, não de observação. Onde há incerteza, está dito no item.

Os IDs são **estáveis**: as specs de [../02-software_specs/](../02-software_specs/) citam por ID e
não redescrevem o bug. Não renumere; itens resolvidos ficam com o status mudado.

`D-nn` = defeito no que existe. `P-nn` = pendência, algo a decidir ou medir.

## Defeitos

| ID | Título | Severidade | Arquivo | Status |
|---|---|---|---|---|
| **D-01** | `ldr_ler` devolve `-1.0` como se fosse leitura | 🔴 alta | [01](01-bugs-criticos.md) | aberto |
| **D-02** | TPM0 e ADC inicializados três vezes, com `\|=` | 🔴 alta | [01](01-bugs-criticos.md) | aberto |
| **D-03** | A "conversão para mV" não converte nada | 🔴 alta | [01](01-bugs-criticos.md) | aberto |
| **D-04** | `acos`/`asin` sem clamp → NaN → `(int)NaN` é UB | 🔴 alta | [01](01-bugs-criticos.md) | aberto |
| **D-05** | Servo **envolve** o ângulo em vez de saturar | 🔴 alta | [01](01-bugs-criticos.md) | aberto |
| **D-06** | 12 min 40 s de teste bloqueante em todo boot | 🔴 alta | [01](01-bugs-criticos.md) | aberto |
| **D-07** | `testes.c` chama servo sem incluir o header | 🟡 média | [01](01-bugs-criticos.md) | aberto |
| **D-10** | Filtro quadrático incoerente com o próprio log | 🟠 alta | [02](02-bugs-de-controle-e-matematica.md) | aberto |
| **D-11** | θ (ângulo de incidência) usado como posição do servo | 🔴 alta | [02](02-bugs-de-controle-e-matematica.md) | aberto |
| **D-12** | Clamp antes do teste, e recomando no limite | 🟡 média | [02](02-bugs-de-controle-e-matematica.md) | aberto |
| **D-13** | Modo noturno usa a variável do horizontal nos dois eixos | 🟡 média | [02](02-bugs-de-controle-e-matematica.md) | aberto |
| **D-14** | Servo bloqueante torna a amostragem irregular | 🟠 alta | [02](02-bugs-de-controle-e-matematica.md) | aberto |
| **D-15** | `theta` truncado **antes** de ser arredondado | 🟢 baixa | [02](02-bugs-de-controle-e-matematica.md) | aberto |
| **D-16** | Dia do ano aproximado, e **literais octais** na data | 🟡 média | [02](02-bugs-de-controle-e-matematica.md) | aberto |
| **D-17** | Relógio sem data, e volta a uma hora fixa a cada boot | 🟠 alta | [02](02-bugs-de-controle-e-matematica.md) | aberto |
| **D-18** | Comentário do overlay chama pino externo de canal interno | 🟢 baixa | [02](02-bugs-de-controle-e-matematica.md) | aberto |
| **D-20** | Validação de pino usa `\|\|` onde queria `&&`: inócua | 🟡 média | [03](03-bugs-na-lib-pwm-vendorizada.md) | **não corrigir** |
| **D-21** | Ramo `GPIOC` configura o registrador e depois falha | 🟡 média | [03](03-bugs-na-lib-pwm-vendorizada.md) | **não corrigir** |
| **D-30** | `lib/servo_motor/` e `lib/testes/` **untracked** | 🔴 crítica | [04](04-dividas-de-projeto-e-repositorio.md) | aberto |
| **D-31** | Repositório git órfão em `src/.git/` | 🟡 média | [04](04-dividas-de-projeto-e-repositorio.md) | aberto |
| **D-32** | Acento no nome de diretório, typo no nome de arquivo | 🟡 média | [04](04-dividas-de-projeto-e-repositorio.md) | aberto |
| **D-33** | README, CMake e `.vscode` são do carrinho, não do tracker | 🟡 média | [04](04-dividas-de-projeto-e-repositorio.md) | aberto |
| **D-34** | Headers sem include guard, macros duplicados, protótipo vazio | 🟡 média | [04](04-dividas-de-projeto-e-repositorio.md) | aberto |
| **D-35** | Variáveis e macros mortos | 🟢 baixa | [04](04-dividas-de-projeto-e-repositorio.md) | aberto |
| **D-36** | `CONFIG_ADC_ASYNC` e `CONFIG_FPU` sem efeito | 🟢 baixa | [04](04-dividas-de-projeto-e-repositorio.md) | aberto |
| **D-37** | `sample_buffer` `static` compartilhado entre canais | 🟢 baixa | [04](04-dividas-de-projeto-e-repositorio.md) | aberto |

## Pendências

Todas em [05-pendencias-de-hardware.md](05-pendencias-de-hardware.md).

| ID | Pergunta | Bloqueia |
|---|---|---|
| **P-01** | Quais pinos ADC para os 2 LDRs novos? | o overlay |
| **P-02** | De onde sai a alimentação dos servos? | ⭐ a confiabilidade de **toda** leitura de LDR |
| **P-03** | Divisor é pull-up ou pull-down, e com que resistor? | ⭐ o **sinal** do erro |
| **P-04** | Faixa real de mV dos LDRs no local? | a calibração de zona morta e ganho |
| **P-05** | Altura da aleta do X? | o ganho óptico da malha |
| **P-06** | Batente mecânico do eixo vertical? | os limites de curso do vertical |
| **P-07** | Painel fotovoltaico real ou chapa? | torque, inércia, e o que "funciona" quer dizer |
| **P-08** | Qual a faixa real de pulso do SG90S? | trocar os fatores empíricos por medição |
| **P-09** | Para onde aponta o servo horizontal em 0°? | ⭐ traduzir ângulo de servo em azimute do céu |

## Se você só vai consertar três coisas

1. **D-30** — versionar `lib/servo_motor/` e `lib/testes/`. Custa um `git add`, e sem isso um
   `git clean -fd` apaga a arquitetura atual inteira. É a única com risco de perda total.
2. **D-11** — o eixo vertical está sendo comandado por uma função de custo. É o bug que faz o
   produto não fazer o que promete.
3. **D-06** — 12 minutos de boot. É o que impede qualquer iteração rápida em bancada, e portanto
   atrasa a correção de todos os outros.
