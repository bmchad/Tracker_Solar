# Camada de servo e calibração

Arquivos: `lib/servo/servo.c` e `lib/servo/servo.h` — renomeados de
`lib/servo_motor/sevo_motor.c` e `servo_motor.h` — renomeados em 2026-09-15 (**D-32**: o `.c` está com
typo em duas letras, "sevo" e "tenpo").

⭐ Implemente esta spec **antes** do controle. Enquanto cada comando bloquear 1–2 s, o período de
amostragem é irregular e nenhum filtro em cima dele significa o que promete (**D-14**).

## ⭐ Todo parâmetro de eixo mora no `eixo_t`; o `mover()` guarda só o algoritmo

A camada tem **uma** função interna com a lógica — saturação pela duração restante, recusa de
comando abaixo da duração mínima, acúmulo de θ — e **nenhum número dentro dela**. Canal, limite de
curso, pulso de cada sentido e ω de cada sentido vivem no `eixo_t`, separados em `eixo_h` e
`eixo_v`. As duas funções públicas só escolhem o eixo.

⚠️ **A velocidade quase ficou de fora**, como macro de arquivo, e isso tornava impossível ajustar um
eixo sem mexer no outro. Ela entrou no struct em 2026-09-15, e o motivo é mais forte que organização:

⛔ **Velocidade e ω são o mesmo fenômeno visto de dois lados.** Mudar a velocidade de um eixo
**invalida o ω daquele eixo** — e se ele não for medido de novo, a odometria mente, θ deriva, e o
limite de curso passa a proteger menos do que promete. **Sem nenhum sintoma**, até o painel bater em
alguma coisa. Os dois campos ficam lado a lado no mesmo struct exatamente para que quem mexer num
veja o outro.

⚠️ **A alternativa considerada e recusada** foi duplicar a função, uma por eixo. Ela dá liberdade
para os eixos divergirem, mas espalha a saturação e a odometria em dois lugares — e este projeto já
pagou por isso: o **D-05** (wrap em vez de clamp) existia nas *duas* funções de servo, que eram
idênticas, e a correção precisou virar um helper. A regra que ficou: **parâmetro diverge, algoritmo
não.** Se um dia um eixo precisar de *comportamento* diferente — não só de número —, aí sim separar
é o certo.

## Clamp, nunca wrap

⛔ O código atual **envolve** o ângulo em vez de saturá-lo:

```c
while(ang>180){ ang-=180; }
while(ang<0)  { ang+=180; }
```

Pedir 185° entrega **5°**. Pedir −10° entrega **170°**. Ou seja: uma ordem que deveria parar no fim
de curso manda a placa para o **extremo oposto**, com força e sem aviso. É **D-05**, e é a razão de
ele estar entre os críticos: combinado com **D-04** (NaN chegando como ângulo) e com o limite ainda
provisório do eixo vertical (**P-06**), o modo de falha é mecânico.

```c
/* Satura no curso do eixo. Sem envolver: um pedido fora de curso vira o limite,
   nunca o extremo oposto. */
int servo_saturar(eixo_t eixo, int graus);
```

Os limites são propriedade **mecânica** do eixo, então moram aqui e não no controle — quem os
conhece é [../03-hardware_specs/03-atuadores-mg90s.md](../03-hardware_specs/03-atuadores-mg90s.md):
horizontal 10°–170°, vertical **10°–160°** (**P-06** respondida: não há batente de montagem, o
batente é o fim de curso do próprio servo — e o fator 1,47 faz 180° comandados virarem 2,47 ms, além
dele). O vertical continua **provisório** até **P-08** medir a faixa de pulso, e o comentário
registra isso.

## Separar comandar de esperar

```c
/* Só escreve CnV. Retorna imediatamente. */
void servo_ir(eixo_t eixo, int graus);

/* Comanda e espera ms. Para o bringup, onde a espera É o objetivo. */
void servo_ir_e_esperar(eixo_t eixo, int graus, int ms);
```

Hoje as duas coisas estão fundidas: `k_msleep((int)(1000 * t))` dentro da função de servo, com `t`
em **segundos inteiros** — 1 ou 2. Três consequências:

1. **O período de amostragem é irregular.** O laço dorme 1 s quando não move e 3 s ou mais quando
   move. O histórico de 5 amostras cobre então um intervalo que varia por um fator de três, e
   "mediana das últimas 5 leituras" deixa de ter significado temporal fixo.
2. **Os dois eixos são serializados.** Mover os dois custa a soma das duas esperas, mesmo eles
   sendo independentes e podendo assentar em paralelo.
3. **A granularidade é de 1 segundo.** Um MG90S leva cerca de 0,1 s por 60° — a espera mínima
   possível é uma ordem de magnitude maior do que o movimento precisa.

⭐ **A separação é o que viabiliza a regra de "não amostrar durante o movimento"**, que é dona da
spec de controle ([03-controle-4-quadrantes.md](03-controle-4-quadrantes.md)): com a espera fora da
função de servo, quem orquestra pode ler **depois** dela, e não no meio.

Com a separação, o `main.c` comanda os dois eixos e faz **uma** espera, dimensionada pelo maior dos
dois movimentos. A espera continua existindo — sem realimentação de posição ela é a única garantia
de que o servo chegou (ver
[../03-hardware_specs/03-atuadores-mg90s.md](../03-hardware_specs/03-atuadores-mg90s.md)) — mas
passa a ser decisão de quem orquestra, em milissegundos.

## Calibração parametrizada

Os literais `1.21` e `1.47` estão cravados no meio da expressão, um em cada função:

```c
pwm_tpm_CnV(TPM0, 3, (375 + (1.21*ang * 375 / 180)));
pwm_tpm_CnV(TPM0, 0, (375 + (1.47*ang * 375 / 180)));
```

Viram constantes nomeadas, e o comentário histórico (as correções empíricas encadeadas) é
**preservado** — ele é a única documentação de como os números apareceram:

```c
/* Contagens de pulso. Derivadas em 03-hardware_specs/01-plataforma-e-pinagem.md:
   375 kHz de contador, MOD 7500 => 20 ms. 375 = 1 ms, 750 = 2 ms. */
#define CNT_PULSO_MIN  375
#define CNT_PULSO_MAX  750

/* ⚠️ Fatores EMPÍRICOS, nunca medidos com goniômetro — ver P-08.
   Historico declarado no codigo original: partiu de x2 ("pedi 180, moveu 90"),
   depois x140/180 duas vezes no horizontal e x140/180 + x170/180 no vertical.
   Suspeita registrada em P-08: o servo provavelmente quer 0,5-2,4 ms, e nesse
   caso o certo e mexer em CNT_PULSO_MIN/MAX, nao multiplicar o angulo. */
#define FATOR_H  1.21
#define FATOR_V  1.47
```

⚠️ **Não "conserte" a matemática antes de P-08.** Os fatores estão empiricamente ajustados ao
hardware que está montado; trocá-los por uma fórmula limpa sem medir substitui um erro conhecido e
compensado por um desconhecido. A parametrização existe para que a medição, quando vier, seja uma
edição de duas linhas.

## Header decente

O header atual tem três problemas, todos em **D-34**:

- **sem include guard** — nada impede dupla inclusão;
- **arrasta o Zephyr inteiro**: inclui `kernel.h`, `device.h`, `gpio.h`, `pwm_z42.h`, `ldr.h`,
  `init_hora.h` e `angulo_incidencia_solar.h`. Um header de servo não precisa saber que existe um
  relógio, e muito menos que existe matemática solar;
- **redefine macros de outros**: `TPM_MODULE`, `SLEEP_TIME_MS`, `INPUT_PORT`, `INPUT_PIN` — os
  mesmos quatro que `src/main.c` e `lib/testes/testes.c` também definem. Redefinição idêntica é
  legal em C, então passa calado; no dia em que um dos quatro lugares mudar de valor, o resultado
  depende da ordem de inclusão.

O novo declara o que oferece e nada mais:

```c
#ifndef SERVO_H
#define SERVO_H

typedef enum { SERVO_H = 0, SERVO_V, SERVO_COUNT } eixo_t;

void servo_inicializar(void);            /* TPM0 + os 2 canais. Só o main chama. */
void servo_ir(eixo_t eixo, int graus);
void servo_ir_e_esperar(eixo_t eixo, int graus, int ms);
int  servo_saturar(eixo_t eixo, int graus);
int  servo_posicao(eixo_t eixo);         /* último ângulo COMANDADO, não medido */

#endif /* SERVO_H */
```

⚠️ **`servo_posicao()` devolve uma crença, não uma medida.** Não há encoder — é o último valor
comandado. O nome é deliberadamente `posicao` e não `angulo_atual` para não sugerir leitura de
sensor. Se o servo travar num obstáculo, esta função mente e nada no sistema desmente.

`servo_inicializar()` centraliza aqui as três chamadas de `pwm_tpm_*` que hoje aparecem copiadas em
três arquivos (**D-02**).
