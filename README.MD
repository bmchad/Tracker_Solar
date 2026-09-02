EVIDÊNCIAS: 
https://drive.google.com/file/d/1Eu0Jbbld2EuSbPxGxIpqz8YSCFXWB1Ga/view?usp=drive_link

# O que é o Tracker Solar

Uma placa montada sobre dois servos que se orienta para o sol sozinha, e um firmware bare-metal
sobre Zephyr que decide para onde ela vira.

## O hardware, em uma linha

**FRDM-KL25Z** (Cortex-M0+, MKL25Z4, 48 MHz) + **Zephyr 3.4** (via PlatformIO,
`platform_packages = platformio/framework-zephyr@~3.40200.0`) + **2 servos SG90S** em PWM por
`TPM0` + **LDRs** em divisor de tensão lidos pelo `ADC0`.

A pinagem completa é dona de [../03-hardware_specs/01-plataforma-e-pinagem.md](../03-hardware_specs/01-plataforma-e-pinagem.md).

## O laço de operação

1. Atualiza o relógio de software (`relogio_atualizar`).
2. Se está fora da janela de sol (hoje: 18h–6h), recolhe a placa e dorme 5 s — o painel não fica
   caçando luz de poste.
3. Lê os LDRs.
4. Decide se o erro entre eles é grande e consistente o bastante para justificar mover.
5. Comanda os servos em passos pequenos e espera.

Não há PID, não há velocidade: o controle é **posição incremental por passos** com zona morta.
Isso é adequado ao problema — o sol se move 15°/h, e um servo SG90 não faz movimento suave de
qualquer forma.

## O que ele deliberadamente NÃO é

Estas ausências são de projeto, não esquecimento. Quem for propor melhoria precisa saber que o
firmware já assume cada uma:

- **Não há RTC.** O relógio é de software: `init_hora.c` guarda uma hora inicial **compilada no
  binário** e soma o `k_uptime_get()` do Zephyr. Toda queda de energia volta o relógio para a hora
  cravada no código. Consequências em **D-17**.
- **Não há realimentação de posição.** Nenhum encoder, nenhum potenciômetro. O firmware manda um
  ângulo e **presume que ele foi atingido** — a variável `angulo` no código é uma crença, não uma
  medida. Se o servo travar num obstáculo, o software não fica sabendo.
- **Não há calibração medida dos servos.** O que existe são dois fatores empíricos encadeados
  (`1.21` no horizontal, `1.47` no vertical) que ninguém registrou como foram obtidos. Ver
  [../03-hardware_specs/03-atuadores-sg90s.md](../03-hardware_specs/03-atuadores-sg90s.md) e **P-08**.
- **Não há persistência.** Nenhum parâmetro em flash: latitude, longitude, hora inicial e
  inclinação são `static` no código-fonte e mudar qualquer um exige recompilar e regravar.
- **Não escreve em nada externo.** A única saída fora dos servos é `printk` na serial a 115200.

## Os módulos

| Diretório | Papel |
|---|---|
| `src/main.c` | o laço de controle inteiro, e a única entrada |
| `lib/ldr/` | leitura do ADC, um canal por LDR |
| `lib/servo_motor/` | graus → contagens de PWM, por eixo (⚠️ untracked, D-30) |
| `lib/pwm/` | driver TPM bare-metal, de origem terceira (2017), não escrito aqui |
| `lib/init_hora/` | o relógio de software |
| `lib/angulo_incidência_solar/` | a matemática solar (declinação, hora solar, ângulo horário) |
| `lib/testes/` | rotinas de bringup chamadas no boot (⚠️ untracked, D-30) |

`lib/pwm/pwm_z42.c` é o único código que **não** foi escrito neste projeto. Ele é tratado como
dependência: tem bugs conhecidos (D-20, D-21) que a decisão é **não** corrigir sem necessidade.
