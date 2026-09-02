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
2. **Sinal do divisor** (**P-03**). Iluminar um quadrante: o valor dele tem de **subir** se for
   pull-down. Se cair, a mistura do controle precisa dos sinais invertidos — e descobrir isso agora
   custa um comentário, descobrir depois custa um servo.
3. **Casamento dos quatro** — com os quatro na sombra, os valores têm de ser próximos. O
   espalhamento aqui é um erro **constante** que o controle vai ler como "o sol está daquele lado"
   para sempre.
4. **As medições de P-04**, que calibram `ZONA_MORTA_MV` e `GANHO_MV_POR_GRAU`.

⭐ Deixar rodando 1 minuto com a placa parada: a **variação** dos valores é o piso de ruído, e
`ZONA_MORTA_MV` tem de ficar acima dele. Este é o número que decide se haverá zig-zag.

### Passo 2 — os servos, sem sensores

`teste_motores()` com `NUMERO_DE_TESTES 1`. Confirma curso, sentido e alimentação:

- os limites de curso param onde devem (nada de wrap — D-05 corrigido);
- **qual sentido aumenta o ângulo** — é o que preenche `SENTIDO_SERVO` e
  `AZIMUTE_DO_SERVO_ZERO` de [04-eixo-vertical-feedforward.md](04-eixo-vertical-feedforward.md);
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
