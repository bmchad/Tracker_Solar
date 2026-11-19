# Decisões do firmware v1 (ADR)

Cada decisão de arquitetura e de lógica do firmware, com o motivo e o que ela custa. A lógica em si
está em [main.md](main.md); o que falta saber, em [duvidas.md](duvidas.md).

O ponto de partida foi o **esboço** de `motor_rele.c` e `main.c` vindo de uma sessão no laboratório,
guardado no commit `79c8b99` (arquivo `mega_context/CLAUDE.md` daquele commit). Várias decisões abaixo
são **desvios do esboço**, e cada uma diz por quê.

Todas: **aceitas em 2026-11-19**, na v1.

---

## ADR-01 · Dois sinais (LIGA e SENTIDO), não três

**Contexto.** O esboço tinha três saídas independentes: `motor_avancar`, `motor_recuar` e
`motor_habilitar`. Depois, ao descrever o circuito, o usuário disse que **R2 e R3 recebem o mesmo
sinal** ([circuito.md](circuito.md) §2).

**Decisão.** Duas saídas: **LIGA** (R1) e **SENTIDO** (R2 e R3 juntos). SENTIDO em repouso = sentido A;
acionado = sentido B.

**Consequências.** Um pino a menos. Some o estado perigoso que o esboço precisava evitar ("os dois
sentidos ao mesmo tempo"): com um sinal só, ele não existe. ⚠️ Se a fiação do laboratório seguir o
esboço (três entradas), a v1 não casa com ela (DV-18).

## ADR-02 · Polaridade ativa em nível baixo, declarada só no overlay

**Contexto.** O esboço tratava os relés como ativos em **nível alto**. O módulo usado liga quando a
entrada IN recebe **GND** ([circuito.md](circuito.md) §4, item 1). Com o esboço, `GPIO_OUTPUT_INACTIVE`
(0 V) no boot **ligaria os três relés**, e o comando de parar (escrever 0) **ligaria o motor**.

**Decisão.** `GPIO_ACTIVE_LOW` nos dois pinos, em `zephyr/frdm_kl25z.overlay`. O código só usa nível
lógico: 1 = relé ligado.

**Consequências.** Se a polaridade estiver errada, é uma linha no overlay para corrigir, e nenhuma no
código. ⚠️ O risco é o mesmo nas duas direções: com a polaridade trocada, o motor liga no boot. Por
isso o passo 4 do bring-up ([main.md](main.md) §10) é **sem o motor**.

## ADR-03 · Só R1 abre ou fecha com corrente

**Contexto.** O esboço escrevia os três relés no mesmo instante, para ligar e para desligar. Como cada
relé leva alguns milissegundos para comutar, R2/R3 podiam abrir ou fechar com corrente passando
([circuito.md](circuito.md) 3.1).

**Decisão.** `lib/motor_rele` impõe a ordem: para ligar, SENTIDO → espera 20 ms → R1; para desligar,
R1 → espera 20 ms → SENTIDO. Para inverter, desliga e espera o motor parar antes de religar.

**Consequências.** R2 e R3 só se gastam mecanicamente; o desgaste elétrico fica todo em R1, o relé que
já se previa trocar ([circuito.md](circuito.md) 3.2). Custa até ~320 ms de bloqueio numa inversão (20 + 280 + 20).

## ADR-04 · Soltar SENTIDO ao parar

**Contexto.** Com o motor parado, SENTIDO poderia ficar no último estado.

**Decisão.** `motor_rele_desligar()` solta SENTIDO depois de abrir R1.

**Consequências.** As bobinas de R2 e R3 não ficam energizadas por horas com o motor parado (calor,
consumo da bateria). Não gasta contato, porque R1 já está aberto. O próximo movimento no sentido B
paga uma comutação a mais de R2/R3, também sem corrente.

## ADR-05 · Pinos: LIGA em PTD4 (D2), SENTIDO em PTD2 (D11)

**Contexto.** Nenhum documento definia os pinos ([circuito.md](circuito.md) §4, item 8). O esboço do
laboratório usava PTD2 (avançar), PTD3 (recuar) e PTD4 (habilitar).

**Decisão.** Manter os pinos do esboço sempre que possível: **PTD4** continua sendo o liga/desliga e
**PTD2** vira SENTIDO. PTD3 fica livre. Conferido no devicetree da placa: nenhum dos dois está em uso
(o SPI do conector Arduino não está habilitado).

**Consequências.** No laboratório, o fio que ia ao PTD3 (recuar) deixa de ser usado (DV-18).

## ADR-06 · Liga/desliga com histerese, no lugar da zona morta simétrica

**Contexto.** O esboço parava o motor numa zona morta de ±1° em volta do alvo e lia a cada 250 ms. A
24 °/s (velocidade estimada), a placa anda ~6° entre duas leituras: atravessa a janela de 2° sem ser
vista, o firmware inverte, ela atravessa de novo, e o par de relés fica ligando e desligando.

**Decisão.** Liga só se o erro passar de **1,0°** (`LIMIAR_PARTIDA_D10`); com o motor ligado, desliga
quando a placa **chega ao alvo** (`falta ≤ ANTECIPACAO_D10`, hoje 0).

**Consequências.** A parada não depende de "acertar" uma janela; basta cruzar o alvo. A inércia depois
do corte fica dentro da folga de 1°. Se a inércia for maior que isso, sobe `ANTECIPACAO_D10` (DV-17).

**Alternativa descartada.** Controle proporcional ou PWM: relé é liga/desliga, e a rosca sem-fim já
segura a placa parada.

## ADR-07 · Ler a cada 10 ms com o motor girando

**Contexto.** Mesma conta do ADR-06: 250 ms é lento demais com o motor ligado. A [angulo.md](angulo.md)
3.5 mostra que 10 ms dão 0,24° entre leituras a 24 °/s.

**Decisão.** 10 ms com o motor girando, 250 ms parado. A serial imprime a cada 250 ms nos dois casos.

**Consequências.** A parada erra no máximo uma leitura (~0,24°) mais a inércia. Cada leitura são 5
conversões do ADC, muito menos que 10 ms. Imprimir uma linha leva alguns milissegundos, por isso a
impressão não acompanha a leitura.

## ADR-08 · Intervalo mínimo de 5 s entre movimentos

**Contexto.** Sem intervalo, a placa poderia religar logo depois de parar, ainda balançando.

**Decisão.** `INTERVALO_MOVIMENTOS_MS = 5000`.

**Consequências.** A placa assenta antes da próxima decisão, e R1 abre no máximo uma vez a cada 5 s.
Para acompanhar o sol (15° por hora), 5 s é irrelevante.

## ADR-09 · Um firmware só, com o motor travado até a calibração

**Contexto.** Havia dois firmwares: o teste de bancada do potenciômetro e o esboço de controle. O
esboço ligava o motor com a calibração **nominal**, em que "90°" é uma posição qualquer do eixo.

**Decisão.** O teste de bancada virou o **modo calibração** do firmware de controle: com
`CALIBRACAO_MEDIDA = 0`, o motor nunca liga e a serial mostra N e o ângulo.

**Consequências.** Um firmware só para gravar e manter. Ninguém liga o motor sem antes ter calibrado
de propósito. O teste antigo continua no git (commit `99ef532`).

## ADR-10 · Curso e alvo de bancada: 0° a 90°, alvo em 45°

**Contexto.** O esboço mirava 90°. O curso da estrutura não está definido ([angulo.md](angulo.md) §9,
item 2); o único curso conhecido é o do protocolo de bancada da [angulo.md](angulo.md) §8, de 0° a 90°
com transferidor.

**Decisão.** `ANGULO_MIN_D10 = 0`, `ANGULO_MAX_D10 = 900`, `ALVO_D10 = 450`. O alvo passa por `CLAMP` no
curso.

**Consequências.** Um alvo de 90° ficaria encostado no limite. Com 45° sobra folga dos dois lados para
a bancada. Os três números são da bancada e mudam com a estrutura (DV-12).

## ADR-11 · Proteções por software, com falha travada até o reset

**Contexto.** Não há fim de curso por hardware ([circuito.md](circuito.md) §4, item 7). Com o torque de
um motor de vidro passando por 20:1, um defeito (sensor solto, sentido trocado, batente) faz o motor
girar até quebrar algo.

**Decisão.** Quatro proteções ([main.md](main.md) §5): leitura inválida, sentido errado (1° ao
contrário), sem movimento (menos de 0,5° por segundo) e tempo máximo (30 s). Todas desligam o motor e
**travam** em falha até o reset.

**Consequências.** Um defeito para tudo, e a placa fica onde está (a rosca sem-fim segura). Uma falha
falsa também para tudo, até alguém apertar reset. ⚠️ Os limiares dependem da velocidade real do motor
(DV-11). **Não substituem** o fim de curso por hardware.

**Alternativa descartada.** Tentar de novo sozinho: repetiria o mesmo defeito, agora sem ninguém olhando.

## ADR-12 · Sentido do motor como parâmetro (`SENTIDO_AUMENTA`)

**Contexto.** Não dá para saber de antemão qual estado de SENTIDO sobe a placa: depende de como os
fios do motor entraram nos relés e de como o potenciômetro foi montado (DV-09).

**Decisão.** Uma constante só, `SENTIDO_AUMENTA`, traduzida em `sentido_para()`. A proteção de sentido
errado diz exatamente qual constante trocar.

**Consequências.** Trocar o sentido é uma linha. O primeiro teste com o motor mostra se está certo, e
o erro custa ~1° de movimento.

## ADR-13 · Relés inicializados antes de tudo

**Decisão.** `motor_rele_inicializar()` é a primeira linha do `main()`, antes do recibo de gravação e
do potenciômetro.

**Consequências.** Se qualquer coisa falhar depois, os relés já estão em estado conhecido e desligado.

## ADR-14 · Overlay só com os relés; potenciômetro configurado no código

**Contexto.** O overlay ainda tinha os quatro canais de LDR do protótipo. O resumo do laboratório
perguntava "qual índice do `zephyr,user` o potenciômetro usa".

**Decisão.** O potenciômetro não usa o devicetree: `lib/potenciometro` configura o canal 11 do ADC
(**PTC2 = A4**) em tempo de execução, e PTC2 já sai do reset em modo analógico. O overlay ficou só com
os dois pinos dos relés.

**Consequências.** A pergunta do laboratório se resolve: nenhum índice, o pino é o A4. A `lib/ldr` do
protótipo deixa de compilar se alguém a incluir (DV-21).

## ADR-15 · Um laço só, sem threads

**Contexto.** Um sensor, um atuador, nenhuma tarefa concorrente.

**Decisão.** Tudo no `main()`, num laço com `k_msleep`. As esperas dos relés bloqueiam.

**Consequências.** Fácil de ler e de depurar pela serial. Durante as esperas dos relés (até ~320 ms) o
potenciômetro não é lido, mas nesse intervalo R1 está aberto: o motor não está sendo acionado. Se a
v2 precisar de outras tarefas (relógio, cálculo solar), elas cabem antes da decisão do laço.

## ADR-16 · O que ficou do esboço

Ficaram: a biblioteca do potenciômetro como está (mediana de 5, `-ERANGE` nos trilhos), parar o motor
em qualquer erro de leitura, a zona de 1° (agora como limiar de partida), o `imprimir_d10()` e a
mensagem de "corte a alimentação" quando os relés não respondem.
