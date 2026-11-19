# Lógica do firmware v1: eixo vertical por potenciômetro e relés

Explica **o que o código faz e por que ele funciona assim**. As decisões, com as alternativas
descartadas, estão em [main_adr.md](main_adr.md); o que ainda falta saber, em [duvidas.md](duvidas.md).

⭐ **Em uma frase:** o potenciômetro mede o ângulo da placa; se ele estiver a mais de 1° do alvo, o
firmware liga o motor na direção certa, lê o ângulo a cada 10 ms e desliga o motor quando a placa
chega ao alvo. Qualquer coisa estranha desliga o motor e trava até o reset.

---

## 1. Arquivos

| Arquivo | Papel |
|---|---|
| `src/main.c` | o controle: parâmetros da montagem, máquina de estados, proteções |
| `lib/motor_rele/` | os relés: a **ordem segura** de ligar, inverter e desligar ([circuito.md](circuito.md) 3.1) |
| `lib/potenciometro/` | a leitura: mediana de 5 amostras do ADC, ruído e conversão N → ângulo ([angulo.md](angulo.md) 3) |
| `lib/pisca_led/` | o LED RGB da placa, usado como indicador de estado |
| `zephyr/frdm_kl25z.overlay` | os **pinos** dos relés e a **polaridade** (ativa em nível baixo) |
| `platformio.ini` + `mcuxpresso_tools.py` + `zephyr_link_fix.py` | build e gravação (seção 10) |

Só `motor_rele`, `pisca_led` e `potenciometro` entram no build: o PlatformIO compila apenas as
bibliotecas que o `main.c` inclui. As outras pastas de `lib/` são legado do protótipo (DV-21).

---

## 2. Ligações na FRDM-KL25Z

| Sinal | Pino | Conector | Vai para |
|---|---|---|---|
| Cursor do potenciômetro | **PTC2** (ADC0_SE11) | A4 | as pontas do potenciômetro em **3V3** e **GND** |
| **LIGA** | **PTD4** | D2 | entrada IN do relé **R1** |
| **SENTIDO** | **PTD2** | D11 | entradas IN de **R2 e R3** (o mesmo sinal) |
| GND | GND | — | GND do módulo de relés |

⛔ **Nunca 5 V no potenciômetro**: o pino do ADC aguenta no máximo 3,3 V.

O módulo de relés **liga quando a entrada IN recebe GND** ([circuito.md](circuito.md) §4, item 1). Por
isso o overlay declara os dois pinos como `GPIO_ACTIVE_LOW`: o código escreve 1 para "relé ligado" e o
Zephyr põe 0 V no pino. Nenhuma outra parte do código precisa saber da polaridade.

---

## 3. O boot

1. **Relés primeiro.** `motor_rele_inicializar()` configura LIGA e SENTIDO como saídas **desligadas**.
   Antes disso os pinos estão desabilitados (alta impedância), e a entrada do módulo fica sem GND:
   relé solto também. Não existe instante em que o motor liga sozinho.
2. Imprime o cabeçalho na serial (115200 baud).
3. **Recibo de gravação:** acende `COR_DESTA_GRAVACAO` (de `lib/pisca_led/pisca_led.h`) por 2 s. Se a
   cor não mudar depois de gravar, o upload não chegou à placa.
4. Inicializa o potenciômetro com a calibração do `main.c`.
5. **Se `CALIBRACAO_MEDIDA` for 0, entra no modo calibração e nunca sai** (seção 6). O motor não liga.
6. Limita o alvo ao curso permitido (`CLAMP`), imprime curso, alvo e limiar, e entra no laço.

Se o passo 1 ou o 4 falhar, o firmware vai direto para **falha** (seção 5).

---

## 4. O laço de controle

Uma única thread, um laço. A cada volta: lê o potenciômetro, decide, imprime a cada 250 ms, dorme.

```
                 erro > 1,0°  E  já passaram 5 s do último movimento
       ┌────────┐ ─────────────────────────────────────────────────► ┌─────────┐
       │ PARADO │                                                    │ MOVENDO │
       │ verde  │ ◄───────────────────────────────────────────────── │  azul   │
       └────────┘          chegou ao alvo (falta ≤ 0)                └─────────┘
            │                                                             │
            │  leitura inválida                    sentido errado,        │
            │                                      sem movimento,         │
            ▼                                      tempo máximo,          ▼
                         ┌───────────────────────  leitura inválida  ─────┘
                         │  FALHA  (vermelho)
                         │  motor desligado, trava até o reset
                         └───────────────────────
```

### 4.1 PARADO: decidir se liga

- `erro = alvo − ângulo`.
- Liga o motor só se **|erro| > `LIMIAR_PARTIDA_D10`** (1,0°) **e** já passaram
  **`INTERVALO_MOVIMENTOS_MS`** (5 s) desde o fim do último movimento.
- A direção sai do sinal do erro: erro positivo → precisa **aumentar** o ângulo (`dir = +1`).
- `sentido_para(dir)` traduz "aumentar/diminuir" para o estado do sinal SENTIDO, usando
  `SENTIDO_AUMENTA`. É o único lugar do código que sabe qual sentido do motor sobe a placa.
- Leitura a cada **250 ms**.

### 4.2 MOVENDO: decidir se desliga

Duas contas, as duas já multiplicadas pela direção para valerem nos dois sentidos:

| Conta | Significa | Positiva quer dizer |
|---|---|---|
| `falta = (alvo − ângulo) × dir` | quanto falta para o alvo | ainda não chegou |
| `andou = (ângulo − ângulo_inicial) × dir` | quanto a placa andou desde que o motor ligou | andou no sentido certo |

- **`falta ≤ ANTECIPACAO_D10` (0)** → desliga: a placa chegou ao alvo ou passou dele. Volta a PARADO.
- Senão, confere as proteções (seção 5).
- Leitura a cada **10 ms**: a 24 °/s (velocidade estimada, [angulo.md](angulo.md) §0) são 0,24° entre
  leituras.

### 4.3 Por que não oscila: a histerese

O motor **para no alvo** mas só **religa a mais de 1° dele**. Depois de parar, a placa ainda anda um
pouco (inércia), e esse resto fica dentro da faixa de 1°: não há motivo para religar. Se a placa
passar do alvo **mais** de 1° depois de cortar, ela religa ao contrário 5 s depois, e pode ficar
indo e voltando a cada 5 s. O remédio é parar antes, aumentando `ANTECIPACAO_D10`, ou alargar
`LIMIAR_PARTIDA_D10` (DV-17).

⚠️ O esboço original tinha uma zona morta de ±1° lida a cada 250 ms. A 24 °/s a placa anda ~6° entre
duas leituras, atravessa a janela de 2° sem ser vista e fica indo e voltando. É o motivo das duas
mudanças acima ([main_adr.md](main_adr.md), ADR-06 e ADR-07).

---

## 5. Proteções

⭐ **Ainda não há fim de curso por hardware** ([circuito.md](circuito.md) §4, item 7). Até haver, estas
proteções são a única coisa entre um defeito e o motor girando até quebrar algo.

| Proteção | Dispara quando | Pega |
|---|---|---|
| **Leitura inválida** | `pot_ler()` devolve erro, ou N colado num trilho (< 20 ou > 4075) | fio solto, cursor aberto, potenciômetro fora do curso |
| **Sentido errado** | `andou < −1,0°`: a placa foi **ao contrário** do pedido | `SENTIDO_AUMENTA` trocado, potenciômetro montado invertido |
| **Sem movimento** | a cada 1 s, a placa avançou menos de **0,5°** | motor travado, batente, corrente solta, potenciômetro patinando |
| **Tempo máximo** | motor ligado por mais de **30 s** num movimento só | qualquer coisa que as outras não pegaram |

Todas fazem a mesma coisa: `falhar()` desliga o motor, acende o **vermelho** e repete o motivo na
serial a cada 2 s, **para sempre**. Só o reset sai daí. O motor parado é o estado seguro: a rosca
sem-fim segura a placa sem energia. Religar sozinho repetiria o mesmo defeito.

Se nem desligar os relés funcionar, a serial avisa: `CORTE A ALIMENTACAO DO MOTOR`.

---

## 6. Modo calibração

Com `CALIBRACAO_MEDIDA = 0` (como está no commit), o firmware **nunca liga o motor**. Ele acende o
**amarelo** e imprime a cada 250 ms:

```
angulo = 57,7 | N= 788 ruido  1
```

É o antigo teste de bancada do potenciômetro, agora dentro do firmware de controle. Serve para o
protocolo da [angulo.md](angulo.md) §8: girar o eixo, conferir que N anda sem saltos e anotar N em
dois ângulos conhecidos.

⭐ O motivo: com a calibração **nominal** (N = 0 → 0°, N = 4095 → 300°), "45°" é uma posição qualquer
do eixo. Mover o motor até ela poderia levar a placa ao batente.

---

## 7. Como os relés são acionados (`lib/motor_rele`)

A regra é a da [circuito.md](circuito.md) 3.1: **só R1 abre ou fecha com corrente passando.** R2 e R3
só trocam com R1 aberto, então nunca fazem arco.

**Ligar** (`motor_rele_ligar(sentido)`):

| Passo | O que faz | Espera |
|---|---|---|
| 1 | já está ligado no mesmo sentido? então não faz nada | — |
| 2 | ligado no outro sentido? desliga primeiro (abaixo) | — |
| 3 | espera o motor parar, contando desde a última vez que R1 abriu | até 300 ms |
| 4 | põe SENTIDO no estado pedido (R1 ainda aberto) | 20 ms |
| 5 | fecha R1: o motor parte | — |

**Desligar** (`motor_rele_desligar()`):

| Passo | O que faz | Espera |
|---|---|---|
| 1 | abre R1: o motor perde energia **aqui** | — |
| 2 | espera R1 abrir de fato | 20 ms |
| 3 | solta SENTIDO (sem corrente, não gasta contato) | — |

Soltar SENTIDO ao parar evita deixar as bobinas de R2 e R3 energizadas por horas com o motor parado.
Se abrir R1 falhar, a função **não** mexe em SENTIDO: inverter com o motor talvez energizado é pior do
que parar com o erro.

Os 20 ms vêm da folha de dados de relé comum (fecha em ≤ 10 ms). Os **300 ms de parada são chute**
(o esboço usava 100 ms) e só pesam numa inversão imediata (DV-16).

---

## 8. LED e serial

| LED | Estado |
|---|---|
| cor do recibo, 2 s | boot (troque `COR_DESTA_GRAVACAO` a cada gravação) |
| amarelo | modo calibração: motor travado |
| verde | parado |
| azul | motor girando |
| vermelho | falha: motor desligado até o reset |

Serial em **115200 baud**, uma linha a cada 250 ms:

```
angulo = 43,2 | alvo = 45,0 | N=1912 ruido  2 | girando
>>> chegou ao alvo: motor desligado
FALHA: SENTIDO ERRADO - o angulo andou ao contrario do pedido. Troque SENTIDO_AUMENTA em main.c. Motor desligado: corrija e aperte reset.
```

---

## 9. Parâmetros

Todos ficam no topo do `src/main.c`, menos os dois tempos dos relés (`lib/motor_rele/motor_rele.c`).

| Parâmetro | Valor | Unidade | Situação |
|---|---|---|---|
| `CALIBRACAO_MEDIDA` | 0 | — | vira 1 depois de calibrar |
| `calibracao` | nominal 0 → 0°, 4095 → 300° | — | **a medir** (DV-19) |
| `ANGULO_MIN_D10` / `ANGULO_MAX_D10` | 0 / 900 | décimos de grau | curso de **bancada**; o da estrutura é desconhecido (DV-12) |
| `ALVO_D10` | 450 | décimos de grau | meio do curso de bancada; vira o alvo solar na v2 |
| `SENTIDO_AUMENTA` | `MOTOR_SENTIDO_A` | — | **a descobrir** ligando (DV-09) |
| `LIMIAR_PARTIDA_D10` | 10 | décimos de grau | do esboço (zona morta de 1°) |
| `ANTECIPACAO_D10` | 0 | décimos de grau | ajustar pela inércia medida (DV-17) |
| `INTERVALO_MOVIMENTOS_MS` | 5000 | ms | escolha da v1 |
| `PERIODO_MOVENDO_MS` / `PARADO` | 10 / 250 | ms | [angulo.md](angulo.md) 3.5 |
| `SENTIDO_ERRADO_D10` | 10 | décimos de grau | acima do ruído de ~0,3° |
| `JANELA_PROGRESSO_MS` / `PROGRESSO_MIN_D10` | 1000 / 5 | ms / décimos | depende da velocidade real (DV-11) |
| `TEMPO_MAX_MOVIMENTO_MS` | 30000 | ms | 120° a 24 °/s levam 5 s: sobra folga |
| `RELE_TEMPO_COMUTACAO_MS` | 20 | ms | folha de dados de relé comum, com folga |
| `MOTOR_TEMPO_PARADA_MS` | 300 | ms | **chute** (DV-16) |

---

## 10. Colocando para rodar

Nesta ordem. Cada passo depende do anterior.

1. **Relé desliga com 3,3 V?** Sem o motor: alimente o módulo, ligue a entrada IN no GND (o relé tem de
   clicar) e depois no 3V3 da placa (o relé tem de soltar e o LED do canal apagar de vez). Se não
   soltar, o firmware não consegue parar o motor (DV-01). **Não siga sem isso.**
2. **Gravar e conferir o modo calibração.** Com `CALIBRACAO_MEDIDA = 0`: o LED fica amarelo e a serial
   mostra N. Gire o potenciômetro de ponta a ponta: N tem de andar sem saltos.
3. **Calibrar.** Leve a placa a dois ângulos medidos (transferidor ou celular), anote N em cada um e
   preencha `calibracao`. Preencha também `ANGULO_MIN_D10`, `ANGULO_MAX_D10` e `ALVO_D10` com o curso
   real. Ponha `CALIBRACAO_MEDIDA = 1`, troque a cor do recibo e grave.
4. **Primeira partida sem o motor.** Com os fios do motor soltos, leve o eixo para longe do alvo. Os
   relés têm de clicar (LIGA e SENTIDO) e, 1 s depois, aparecer `FALHA: motor ligado sem mover a placa`.
   ⭐ Isso é o esperado: prova que a proteção de "sem movimento" funciona. Aperte reset.
5. **Primeira partida com o motor**, com a mão na alimentação do motor. Se aparecer `SENTIDO ERRADO`,
   troque `SENTIDO_AUMENTA` e grave de novo. Se a placa chegar ao alvo e o LED ficar verde, o laço
   está fechado.
6. **Medir a inércia.** Depois de `chegou ao alvo`, veja quanto o ângulo ainda anda. Se passar de 1°,
   aumente `ANTECIPACAO_D10` (DV-17).

⚠️ Mexeu no overlay? **Apague a pasta `.pio/`** antes de compilar (armadilha do ambiente, em
[CLAUDE.md](CLAUDE.md)).

---

## 11. O que a v1 não faz

- **Alvo solar.** O alvo é fixo; o cálculo do ângulo do sol fica para a v2 (DV-22).
- **Fim de curso por hardware.** Precisa de chaves no circuito ([circuito.md](circuito.md) §4, item 7).
- **Acelerômetro.** Só o potenciômetro; a comparação dos dois é o experimento da [angulo.md](angulo.md) §8.
- **Medir corrente** do motor ou detectar travamento por corrente.
- **Velocidade variável.** Relé é liga/desliga, sem PWM.
- **Sair sozinho de uma falha.** Só o reset.
