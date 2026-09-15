# Bringup e testes

Arquivos: `lib/testes/testes.c`, `lib/testes/testes.h`, `platformio.ini`, `src/main.c`.

## Os testes saem do caminho do boot

⛔ Hoje `src/main.c:69-71` chama as duas rotinas de teste **sem condição nenhuma**, e
`NUMERO_DE_TESTES` é **10**. A conta:

| Rotina | Passos | Espera | Total |
|---|---|---|---|
| `teste_motores` | 10 × 4 varreduras × 19 passos = 760 | 1 s cada | ~12 min 40 s |
| `teste_ldr` | 10 × 2 leituras | 0,5 s por par | ~5 s |

**Mais de 12 minutos e meio de sono bloqueante em todo boot** antes de a primeira leitura de
tracking acontecer. É **D-06**, e não é um incômodo de desenvolvimento: um reset ao meio-dia custa
12 minutos de sol.

Duas mudanças:

```ini
; platformio.ini — os testes passam a ser opt-in de compilação
[env:frdm_kl25z]
; ...
build_flags = -DRODAR_TESTES_NO_BOOT
```

```c
/* src/main.c */
#ifdef RODAR_TESTES_NO_BOOT
    teste_ldr_4();
    teste_motores();
#endif
```

E `NUMERO_DE_TESTES` cai de 10 para **1**. Dez repetições idênticas não revelam nada que a primeira
não revele — se o objetivo era procurar falha intermitente, o instrumento certo é um teste de
resistência dedicado, não o caminho de boot do produto.

## As rotinas param de inicializar hardware

`teste_motores()` e `teste_ldr()` chamam hoje, cada uma, as mesmas cinco linhas de
`pwm_tpm_Init` + `pwm_tpm_Ch_Init` + `ldr_inicializar()` que o `main.c` já chamou. Elas passam a
**receber** o hardware pronto (**D-02**), pela regra de
[00-arquitetura-e-ordem-de-execucao.md](00-arquitetura-e-ordem-de-execucao.md).

E `testes.c` precisa **incluir o header do servo**: ele chama
`servo_horizontal_angulo_tempo`/`servo_vertical_angulo_tempo` sem declaração visível hoje —
declaração implícita, que passa com aviso em C17 e é **erro** em C23 (**D-07**).

## Ordem de subida, um passo por vez

Cada passo é verificável isoladamente, e nenhum depende do seguinte. A ordem importa: os dois
primeiros acontecem **antes de qualquer motor girar**, porque um erro de sinal descoberto com a
placa em movimento já é um painel no batente.

### Passo 1 — os quatro sensores, placa travada à mão

`teste_ldr_4()`, substituindo `teste_ldr()`: imprime os quatro valores em mV **e os dois erros**.

```
[ldr] esq=1820 dir=1795 cima=1840 baixo=1810 | erro_h=+25 erro_v=+30
```

O que se confirma aqui, e só aqui:

1. **Pinagem.** Cobrir **um LDR por vez com o dedo** e verificar que o valor que cai é o do nome
   certo. É o único teste da ordem de `io-channels` contra o `enum ldr_id_t` — divergência ali não
   produz erro de compilação nenhum (ver
   [01-devicetree-adc-4-canais.md](01-devicetree-adc-4-canais.md)).
2. **Sinal do divisor** (**P-03**, respondida: pull-down de 10 kΩ). Iluminar um quadrante: o valor
   dele tem de **subir**. Virou confirmação, não descoberta — e continua no roteiro porque custa
   30 s e é o que pega uma montagem que divergiu do papel. Se cair, a mistura do controle precisa
   dos sinais invertidos, e descobrir isso agora custa um comentário, descobrir depois custa um
   servo.
3. **Casamento dos quatro** — com os quatro na sombra, os valores têm de ser próximos. O
   espalhamento aqui é um erro **constante** que o controle vai ler como "o sol está daquele lado"
   para sempre.
4. **As medições que calibram `ZONA_MORTA_MV` e `GANHO_MV_POR_GRAU`.** **P-04** já deu a faixa
   (10²–10³ mV, ver
   [../03-hardware_specs/02-sensores-ldr-e-arranjo-optico.md](../03-hardware_specs/02-sensores-ldr-e-arranjo-optico.md)) —
   o que **só** sai daqui é o piso de ruído no alvo, o espalhamento entre os quatro na sombra e o mV
   por grau.

⭐ Deixar rodando 1 minuto com a placa parada: a **variação** dos valores é o piso de ruído, e
`ZONA_MORTA_MV` tem de ficar acima dele. Este é o número que decide se haverá zig-zag.

### Passo 2 — os servos, sem sensores

`teste_motores()` com `NUMERO_DE_TESTES 1`. Confirma curso, sentido e alimentação:

- os limites de curso param onde devem (nada de wrap — D-05 corrigido);
- **qual sentido aumenta o ângulo** — é o que preenche `SENTIDO_SERVO`. `AZIMUTE_DO_SERVO_ZERO`
  saiu do projeto com **DEC-13** (nada de bússola aqui), mas o sentido continua importando: trocado,
  a correção afasta a placa do sol em vez de aproximá-la. Ver
  [04-eixo-vertical-feedforward.md](04-eixo-vertical-feedforward.md);
- **o teste de acoplamento**, abaixo — é ele que responde **P-02** e **P-10**.

#### ⭐ O teste de 3 passos: separa problema elétrico de problema de algoritmo

Com o `teste_ldr_4()` imprimindo os quatro valores **e os dois erros**, em sequência:

| # | Estado | O que se espera | O que significa se falhar |
|---|---|---|---|
| a | servo **desligado** | valores estáveis; anote o espalhamento | é o **offset estático** — resistores (1% ≈ 8 mV) e dispersão dos LDRs |
| b | servo **energizado e parado** | nada muda em relação a (a) | ruído de repouso da fonte |
| c | servo **girando** | ⭐ `erro_h` e `erro_v` **não se movem** | se movem: **acoplamento por GND** (**P-10**), não algoritmo |

Só o passo (c) responde a pergunta que importa, e é a diferença entre fechar **P-02** sem comprar
nada e descobrir que precisa de fonte externa (**DEC-11**).

⚠️ **Olhe os ERROS, não os valores.** Se os quatro valores caem **juntos**, a subtração cancela e
está tudo bem — é o modo comum, e ele é esperado. O que condena é `erro_h` ou `erro_v` se mexer,
porque isso é a parcela **diferencial**, que nada no algoritmo cobre.

⛔ E um sintoma que não aparece nos números: se o **MCU resetar** ou a serial cair durante (c), é
o `P5V_USB` afundando abaixo do dropout do LDO de 3,3 V. Isso não é erro de leitura e nenhuma
subtração o cancela — vai direto para fonte externa.

### Passo 3 — malha fechada de um eixo

Só o horizontal, com o vertical travado. Lanterna movida à mão na horizontal. O que se observa:

- a placa **segue** (se fugir, sinal invertido — passo 1 deixou passar);
- ela **para** quando a luz para (se oscilar, `ZONA_MORTA_MV` está abaixo do ruído);
- ela **assenta** sem passar do ponto (se passar, `GANHO_MV_POR_GRAU` está pequeno demais).

Depois o vertical, sozinho, do mesmo jeito.

### Passo 4 — os dois eixos, e o feedforward

Lanterna na diagonal. E o teste que só o feedforward permite: **conferir θ**. Com `calcular_theta()`
recebendo os ângulos reais (ver [04-eixo-vertical-feedforward.md](04-eixo-vertical-feedforward.md)),
θ tem de **cair** conforme a malha converge. θ subindo com o log dizendo que moveu é sinal invertido
ou calibração de servo errada.

Vale simular horas diferentes mudando a hora inicial em `lib/init_hora/` e recompilando — é
trabalhoso, e é consequência direta de **D-17** (nenhum parâmetro é ajustável em runtime).

## O que este projeto não tem

**Não há framework de teste, nem CI.** Não existe `pytest`, `unity`, `ztest` — o diretório `test/`
tem só o stub do PlatformIO. Toda verificação é manual, na bancada, pelos quatro passos acima.

É a razão de `lib/controle/` ser **puro** (sem ADC, sem PWM, sem `k_msleep`): num projeto sem
teste automatizado, a única verificação disponível é raciocinar sobre a função, e isso só funciona
se ela não tiver efeito colateral. Ver
[00-arquitetura-e-ordem-de-execucao.md](00-arquitetura-e-ordem-de-execucao.md).

⛔ **E não há typecheck útil além do compilador.** `pio run` é a única barreira, e ela deixa passar
declaração implícita (D-07) com aviso. Ler os avisos do build faz parte do procedimento, não é
opcional.

---

## ⭐ A cor do LED como recibo de gravação

⭐ **Troque a cor do LED a cada build, e confira a placa antes de medir qualquer coisa.** Se a cor
não mudou, a gravação não chegou ao alvo — e você descobre isso em dois segundos, em vez de passar
horas medindo sintomas de um firmware que nunca subiu.

O mecanismo está em `lib/pisca_led/pisca_led.h`, numa constante única:

```c
#define COR_DESTA_GRAVACAO  LED_AZUL   /* LED_VERDE | LED_AZUL | LED_VERMELHO */
```

`src/main.c` chama `led(COR_DESTA_GRAVACAO)` como **primeira** instrução de `main()`, e o LED fica
aceso. Editar essa linha e regravar é o ritual inteiro.

⚠️ **O número do alias não segue a ordem das cores:** o verde é `led0` (PTB19), o azul é `led1`
(PTD1) e o vermelho é `led2` (PTB18). Os três são ativos em nível baixo, e é por isso que
`lib/pisca_led/` usa as variantes `_dt` das funções de GPIO — elas aplicam a inversão declarada no
devicetree sozinhas.

⭐ **`led(cor)` acende e SEGURA, não pisca** — e isso é desenho, não descuido. O nível segurado é o
que permite usar o LED como **cronômetro** de um protocolo de bancada: em
`teste_rot_cont_medir_omega()` o LED apagado significa "marque a posição agora" por 8 s e o aceso
significa "girando" por 5 s. Uma piscada não consegue dizer "continue marcando". Quem quiser piscar
monta o laço (é o que o `recibo()` de `teste_rot_cont.c` faz).

⛔ **Por que isso existe:** em 2026-09-14 **nenhuma** gravação de uma das máquinas chegou ao alvo,
enquanto o terminal dizia `Firmware has been successfully uploaded` e o DAPLink não escrevia
`FAIL.TXT`. O dia inteiro foi gasto investigando servo, alimentação e pino — tudo sintoma de uma
imagem antiga que continuava rodando. A confirmação veio por comparação controlada: o mesmo
binário, na mesma placa, gravado de outro computador, funcionou na hora.

⚠️ **Nenhuma mensagem do gravador prova que o alvo foi reprogramado.** O `E:` montar, o arquivo ser
consumido e o log dizer sucesso são trabalho do **OpenSDA**, um segundo microcontrolador na própria
placa. Só algo que **mude visivelmente no alvo** prova gravação — e a cor do LED é a forma mais
barata disso.

## A escada de 4 fases — apagada, e o que ela ensinou

⛔ **`lib/teste_portas/` foi apagada em 2026-09-15**, quando a biblioteca virou `lib/pisca_led/` com
uma função só. A escada de diagnóstico de pino que morava lá (`teste_portas_servo()`) já não tinha
chamador vivo, e o código está preservado no commit `a411ae6`. O que ela ensinou, e que só existia
nos comentários dela, fica aqui:

1. **A ordem das fases, cada uma eliminando um suspeito diferente:** LED piscando (o firmware está
   vivo?) → PTD3/PTD0 como GPIO em nível alto e baixo (o pino, o mux e o **jumper**) → PWM com duty
   grosso de 0 / 50 / 100 % (o TPM0 em si) → pulso de servo real (o eixo anda, ou não anda).
2. ⛔ **A fase de duty grosso só roda com o servo DESCONECTADO.** 50 % e 100 % não são pulso de
   servo válido: ele zumbe contra o batente sem obedecer, e o teste passa a medir o servo errado.
3. **A fase de GPIO tem de vir ANTES de qualquer `pwm_tpm_Ch_Init`** — ela precisa do pino no mux de
   GPIO, e o PWM o leva para o mux 4. Depois das fases de PWM, voltar os dois ao GPIO antes de
   repetir.
4. ⭐ **Medir na ponta do JUMPER**, no conector que vai ao servo, não no pino da placa. O mesmo teste
   passa a pegar fio partido e mau contato, que é o que o pino sozinho nunca mostra.
5. **Acionar os dois pinos JUNTOS, com o LED espelhando o estado.** Um por vez deixava o outro em
   0 V por 15 de cada 20 s — indistinguível de "pino morto" quando a serial estava muda, que era
   exatamente a situação em que o teste foi escrito.

## ⭐ Medir PWM com multímetro — sem osciloscópio

Não há osciloscópio na bancada, e não é preciso: **multímetro em DC mede a média**, e média de um
PWM é o *duty* vezes a tensão. Sobre os 3,3 V dos pinos do KL25Z, com `MOD = 7500` (50 Hz):

| `CnV` | Pulso | Duty | Multímetro (DC) |
|---|---|---|---|
| 0 | — | 0% | **0 V** |
| **375** | 1,0 ms | 5% | **~0,165 V** ← 0° de servo |
| 563 | 1,5 ms | 7,5% | ~0,248 V |
| 750 | 2,0 ms | 10% | ~0,330 V |
| 3750 | 10 ms | 50% | **~1,65 V** ← valor de teste, inconfundível |
| 7500 | — | 100% | **~3,3 V** |

⭐ **Isso torna leituras pequenas interpretáveis em vez de suspeitas.** Em 2026-09-14, "0,16 V
estáveis em PTD3 e PTD0" parecia pino morto ou ruído; pela tabela, é **exatamente um pulso de 1 ms**
— ou seja, os canais estavam ativos e parados em 0°, o que apontou o diagnóstico para um firmware
antigo ainda em execução, e não para o pino.

⚠️ **Para validar o TPM, use 50% de duty, não os valores de servo.** 1,65 V no meio da escala é
inconfundível; distinguir 0,165 de 0,248 V num multímetro barato, não.

⭐ **E quando a média DC for pequena demais para confiar, meça o mesmo ponto em tensão AC:** pino
comutando dá leitura AC não nula, pino parado (alto ou baixo fixo) dá ~0. Isso prova **chaveamento**
sem depender do valor absoluto.

⚠️ **O TPM é hardware e continua gerando pulso depois que a CPU para.** `main` retornar, travar num
laço ou dar hard fault **não** desliga o timer nem muda o `CnV`. Então uma tensão estável e
plausível num pino de servo **não prova** que o firmware está rodando — prova só que o timer foi
configurado em algum momento. Para saber se a CPU está viva, é preciso algo que **mude** ao longo
do tempo: o LED, a serial, ou o próprio duty variando.
