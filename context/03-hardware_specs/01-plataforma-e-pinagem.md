# Plataforma e pinagem

⭐ **Este arquivo é o dono da pinagem.** Nenhum outro documento repete esta tabela — todos
referenciam daqui. Se um fio mudar, mude aqui.

## Placa

| Item | Valor |
|---|---|
| Placa | NXP **FRDM-KL25Z** |
| MCU | MKL25Z4 — ARM Cortex-M0+, 48 MHz |
| Framework | Zephyr, via PlatformIO (`framework = zephyr`) |
| Pacote | `platformio/framework-zephyr@~3.40200.0` |
| `platform` | `freescalekinetis` |
| Serial | 115200 baud (`monitor_speed`) |

⚠️ **O Cortex-M0+ não tem FPU.** Todo o cálculo solar usa `double` e roda em **soft-float**. O
`prj.conf` tem `CONFIG_FPU=y`, que é inócuo nesta arquitetura — ver **D-36**.

## A tabela de fiação

| Função | Pino | Periférico | MUX | Onde no código |
|---|---|---|---|---|
| PWM horizontal (azimute) | **PTD3** | `TPM0` canal 3 | `MUX(4)` | `src/main.c:37`, `lib/servo_motor/` |
| PWM vertical (inclinação) | **PTD0** | `TPM0` canal 0 | `MUX(4)` | `src/main.c:38`, `lib/servo_motor/` |
| LDR esquerda | **PTB0** | `ADC0_SE8` | analógico | overlay, nó `channel@8` |
| LDR direita | **PTB2** | `ADC0_SE12` | analógico | overlay, nó `channel@c` |
| LDR cima | **A DEFINIR** — sugestão PTB1 | `ADC0_SE9` | analógico | **P-01** |
| LDR baixo | **A DEFINIR** — sugestão PTB3 | `ADC0_SE13` | analógico | **P-01** |

A sugestão PTB1/PTB3 é por serem os vizinhos diretos dos dois já usados, contíguos no header J10 e
livres no projeto atual. A alternativa avaliada foi PTC1 (`SE15`) / PTC2 (`SE11`), também livres.
A escolha é de montagem física, não de software — ver **P-01**.

⚠️ Os dois eixos compartilham o **mesmo TPM** (`TPM0`), logo compartilham prescaler e `MOD`, logo
compartilham **período**. Isso está certo para servos (ambos querem 50 Hz), mas significa que
mudar o período de um muda o do outro.

## Parâmetros de PWM

⭐ **Este bloco é o dono destes números.** Derivados do clock e do prescaler escolhidos:

| Grandeza | Valor | De onde vem |
|---|---|---|
| Clock do TPM | 48 MHz | `TPM_PLLFLL` |
| Prescaler | 128 | `PS_128` |
| Frequência do contador | **375 kHz** | 48 MHz ÷ 128 |
| `MOD` (`TPM_MODULE`) | **7500** | 375 kHz ÷ 50 Hz → período de 20 ms |
| Contagens para 1 ms (0°) | **375** | 375 kHz × 1 ms |
| Contagens para 2 ms (180°) | **750** | 375 kHz × 2 ms |

Ou seja: **`CnV = 375` é o mínimo de curso e `CnV = 750` o máximo**, e 1 grau vale
(750 − 375) ÷ 180 ≈ 2,08 contagens — antes dos fatores de correção empírica, que são donos de
[03-atuadores-sg90s.md](03-atuadores-sg90s.md).

`TPM_MODULE 7500` está **redefinido em quatro arquivos** (`src/main.c`, `lib/testes/testes.c`,
`lib/servo_motor/servo_angulo_tempo.h` e por herança quem o inclui). Ver **D-34**.

## Pinos declarados e não usados

`INPUT_PORT` (`DT_NODELABEL(gpiod)`) e `INPUT_PIN` (20) estão definidos em **três** arquivos e não
são lidos em nenhum — resquício do carrinho, onde eram o sensor de linha. Ver **D-35**.

O overlay habilita `&gpiod` com `status = "okay"`, o que é necessário para o PWM em PTD0/PTD3.

## Referência do ADC

Os quatro canais usam `ADC_REF_INTERNAL` com `ADC_GAIN_1` e resolução de **12 bits** (0–4095).
Na FRDM-KL25Z a referência interna é o próprio VDD, **3,3 V** — e é por isso que a alimentação dos
servos importa para a leitura dos sensores: ver
[04-alimentacao-e-perguntas-abertas.md](04-alimentacao-e-perguntas-abertas.md).
