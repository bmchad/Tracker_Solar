# Sensores LDR e arranjo óptico

## O arranjo escolhido: 2×2 com divisor em X

Quatro LDRs em quadrantes, separados por uma aleta em forma de **X** que projeta sombra. O X é
girado 45° em relação a um `+`, e essa rotação é o que define toda a matemática do controle:
**cada quadrante aponta para uma direção**, não para uma diagonal.

```
            cima
              ▲
        ╲     │     ╱
          ╲   │   ╱
            ╲ │ ╱
   esquerda   ╳   direita        ← o X é a aleta que gera sombra
            ╱ │ ╲
          ╱   │   ╲
        ╱     │     ╲
              ▼
           baixo
```

Quando o sol está à esquerda, a aleta sombreia o quadrante da direita e o LDR da esquerda recebe
mais luz. Logo:

```
erro_horizontal = ldr[ESQUERDA] - ldr[DIREITA]
erro_vertical   = ldr[CIMA]     - ldr[BAIXO]
```

Uma subtração por eixo, um sensor por direção. Simples de propósito.

⚠️ **Se a aleta virar um `+`**, os quadrantes passam a ser diagonais (sup-esq, sup-dir, inf-esq,
inf-dir) e a mistura muda para **soma de pares**:

```
erro_horizontal = (SUP_ESQ + INF_ESQ) - (SUP_DIR + INF_DIR)
erro_vertical   = (SUP_ESQ + SUP_DIR) - (INF_ESQ + INF_DIR)
```

É por isso que a spec de controle isola a mistura numa função única — ver
[../02-software_specs/03-controle-4-quadrantes.md](../02-software_specs/03-controle-4-quadrantes.md).
Trocar o arranjo físico deve custar uma função, não uma auditoria.

## A geometria óptica é o ganho da malha

A altura da aleta em relação ao espaçamento dos sensores determina o comportamento do controle, e
é o parâmetro físico que ninguém pode ajustar em software:

- **Aleta alta / sensores próximos** → muito contraste, sombra nítida, mas **ângulo de captura
  estreito**: fora de alguns graus, um único quadrante fica totalmente sombreado e o erro
  **satura** — o controle perde a noção de "quão longe" e só sabe "para que lado".
- **Aleta baixa / sensores afastados** → resposta suave e faixa larga, mas pouco contraste perto
  do equilíbrio: o erro no alvo fica da ordem do ruído, e é aí que nasce o zig-zag.

Isso não é ajustável depois: é o que fixa a escala de `GANHO_MV_POR_GRAU` e `ZONA_MORTA_MV` na
spec de controle. Por isso o primeiro passo do bringup é **medir os quatro valores em mV** com a
placa travada à mão, antes de qualquer motor girar — ver
[../02-software_specs/06-bringup-e-testes.md](../02-software_specs/06-bringup-e-testes.md).

⚠️ **A altura da aleta não será medida (P-05 encerrada, DEC-14).** O número serviria para *prever*
este ganho, e ele vai ser **medido** de qualquer forma no passo 1 — prever para depois medir é
trabalho duplicado. A aleta continua sendo o único parafuso físico da malha: se aparecer zig-zag no
alvo (pouco contraste) ou erro que satura cedo (aleta alta demais), é nela que se mexe, e aí a
medida volta a fazer falta.

## Circuito — a topologia, fechada pela montagem (P-03, 2026-09-14)

Cada LDR forma um **divisor de tensão** com um resistor fixo, e o ponto médio vai ao pino do ADC.
⭐ **Este arquivo é o dono deste fato.** A cadeia, do terra para cima:

```
   GND ──[ R 10 kΩ ]──┬──[ LDR ]── 3,3 V
                      │
                      └── pino do ADC (A0..A3)
```

**É pull-down: o resistor fixo está do lado do terra, o LDR do lado do rail.** O que o ADC lê é a
queda no resistor, então mais luz → a resistência do LDR **cai** → o pino **sobe**. Tensão alta =
muita luz, e as duas subtrações da seção acima valem **como estão escritas** — não há sinal a
inverter no controle.

**O resistor é de 10 kΩ**: marrom-preto-laranja, isto é 1-0 ×10³. Os quatro têm de ser o mesmo
valor, e de preferência do mesmo lote, pela razão da seção "Descasamento de resistor" abaixo.

⚠️ **Isto veio da montagem relatada, não de uma medida de bancada.** Confirmar é barato, e é por
isso que o passo 1 do bringup continua no roteiro: iluminar um quadrante e ver o valor **subir**. Se
cair, quem diverge é a montagem — e aí é a mistura do controle que inverte.

⭐ **O que os 10 kΩ confirmam** é uma série de números que o resto do `context/` já vinha usando
como suposição: os **~5 kΩ** de impedância de fonte em meia escala (10 k ∥ 10 k, quando o LDR também
mede 10 kΩ), os **~8 mV** de offset por 1% de descasamento, e o polo do filtro de entrada — 5 kΩ ×
100 nF = 500 µs, ou cerca de **320 Hz**. Ele está acima da fundamental de 50 Hz do PWM e isso é o
esperado: o que o filtro mata é o **acoplamento capacitivo das bordas**, que é rápido, não a
fundamental, que não se acopla por essa via.

### A faixa entregue — P-04, respondida (2026-09-14)

**As leituras ficam na ordem de 10² a 10³ mV**: entre cerca de 100 mV e 1 V, contra 3,3 V de fundo
de escala. Invertendo o divisor (`V = 3,3 × 10 k / (10 k + R_LDR)`), isso diz em que faixa o LDR
opera:

| leitura | R do LDR | o que é |
|---|---|---|
| ~100 mV | ~320 kΩ | o LDR no escuro |
| ~1000 mV | ~23 kΩ | o LDR no claro daquele ambiente |

⭐ **A faixa útil está no terço de baixo da escala, e ainda assim sobra resolução de sobra:** 900 mV
de excursão com 12 bits sobre 3,3 V (1 LSB ≈ 0,81 mV) são **~1100 contagens**, e a
`ZONA_MORTA_MV = 30` fica em ~3% disso. Não há motivo para trocar o resistor por causa da escala.

⚠️ **Mas a medida é do ambiente em que foi feita.** R do LDR entre 23 kΩ e 320 kΩ é luz de interior;
sol direto leva o LDR para a casa de 1 kΩ, e aí o divisor entrega ~3,0 V nos quatro. Isso não satura
o ADC, mas **comprime o contraste no topo** — perto de 3,3 V a curva V(R) é quase plana, e é
justamente o contraste entre quadrantes que o controle usa. Se os quatro valores ao sol ficarem
todos acima de ~3 V, quem desce é o resistor (1–2,2 kΩ), e as contas desta seção se refazem.

⛔ **O que a faixa NÃO dá, e continua saindo só do passo 1 do bringup:** o **piso de ruído** com a
placa parada no alvo, o **espalhamento entre os quatro** na sombra e o **mV por grau**. Não são
pendência — são medição de bringup, e é o que mantém `ZONA_MORTA_MV` e `GANHO_MV_POR_GRAU`
marcados como chute inicial na spec de controle.

## Topologia de ligação — decidida (DEC-10)

Os quatro LDRs ficam num **único 3,3 V** e num **único GND**, ligados em **estrela**. Os dois novos
entram por protoboard.

⭐ **Separar em dois pinos de 3,3 V não isola nada:** a FRDM-KL25Z tem **um** regulador, e todos os
pinos `P3V3` são o **mesmo net**, ligados por plano/trilha de miliohms dentro da PCB. É por isso que
a montagem atual — esquerda e direita em pinos de 3,3 V diferentes — funciona: são o mesmo rail.

⛔ **E se isolasse, agrupar esquerda+cima / direita+baixo seria o pior arranjo possível.** Ele cruza
os dois pares comparados, então uma queda de modo comum num rail viraria:

| efeito | leitura do controle |
|---|---|
| esquerda cai, direita não | `erro_h` negativo → "o sol foi para a direita" |
| cima cai, baixo não | `erro_v` negativo → "o sol foi para baixo" |

Um distúrbio elétrico em **um** rail produziria um **falso movimento diagonal do sol**, nos dois
eixos ao mesmo tempo e de forma correlacionada — indistinguível de um sol de verdade se movendo. O
agrupamento correto, se houvesse dois rails, seria **por par comparado** (esq+dir juntos, cima+baixo
juntos), onde a perturbação é comum ao par e morre na subtração.

### Estrela, não trilho corrido

O que o controle usa é **diferença**, então perturbação **comum aos dois membros de um par cancela**
e perturbação que os diferencie vira sinal falso. Isso decide a topologia:

- um fio de 3,3 V do header até **um ponto** da protoboard; dali **quatro** fios curtos, um por
  divisor;
- um fio de GND até **um ponto**; dali quatro fios, um por divisor.

O que se evita é a corrente de um ramo passar pelo contato de outro. Ver **P-10** para o número: a
queda IR **diferencial** ao longo de um trilho compartilhado é o mecanismo que nenhuma das defesas
do algoritmo cobre.

### 100 nF em cada entrada de ADC

De cada pino de ADC ao nó de GND da estrela, pernas curtas. Com a impedância do próprio divisor
(~5 kΩ de fonte, num divisor de 10 kΩ) isso forma um passa-baixas que come o acoplamento capacitivo
do PWM, e ajuda o capacitor de amostragem do ADC. ⭐ É o item de maior retorno por centavo desta
página.

Mantenha os quatro fios analógicos afastados dos de PWM e de potência do servo, e sem trechos
longos em paralelo.

### Descasamento de resistor é offset estático

Os quatro resistores fixos devem ser do mesmo lote, de preferência medidos. Em meia escala, com os
10 kΩ confirmados em "Circuito":

> **1% de descasamento ≈ 8 mV** de offset estático

Comparável à `ZONA_MORTA_MV` da spec de controle. Some-se a dispersão dos próprios LDRs, na seção
abaixo, que é maior.

⛔ **A quarta faixa é OURO: os resistores são de 5%, e é isso que domina o erro de casamento.** Dois
deles em extremos opostos da tolerância descasam **10%**, o que por esta mesma conta dá **~80 mV**
de offset estático — quase **três vezes** a `ZONA_MORTA_MV = 30`. E no pior sentido possível: um "o
sol está daquele lado" permanente, que o controle persegue com passo proporcional até o fim de
curso.

⭐ **Medir os quatro com multímetro e escolher os mais próximos entre si é o item de montagem mais
barato desta página.** Os 5% são a garantia do fabricante sobre o lote inteiro, não a dispersão de
um punhado comprado junto — quatro resistores da mesma fita costumam cair dentro de 1%, e medir é o
que transforma "costumam" em "estão". Se não der para casar, a saída é a rotina de offset em sombra
total descrita na seção abaixo, que hoje está fora de escopo justamente porque casar componente é
mais barato que carregar calibração.

## Os quatro LDRs precisam ser o mesmo componente

O controle compara sensores **entre si**, não contra um valor absoluto — então um LDR com
sensibilidade diferente dos outros é um erro **constante** que o firmware lê como "o sol está
daquele lado" para sempre, e o painel para no batente.

Os quatro devem ser do mesmo lote, com os mesmos resistores. Casar isso em software exigiria uma
rotina de calibração em sombra total, gravando um offset por sensor — não está no escopo desta
virada, e é a razão de o casamento ser requisito de montagem.
