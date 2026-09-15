# Controle de 4 quadrantes

⭐ **IMPLEMENTADO EM 2026-09-15, e mora em `src/main.c`, não em `lib/controle/`.** O módulo puro
continua sendo o destino, e a seção "A interface do módulo" no fim continua valendo como alvo — mas
extrair para biblioteca algo que nunca rodou uma vez é prematuro neste projeto, onde a bancada já
contradisse a spec duas vezes (o arranjo em X da **DEC-1** e o servo posicional). A mistura
(`erros()`), a mediana (`mediana()`) e a tradução de erro em comando (`corrigir()`) são `static` no
topo de `main.c`; extrair depois é mecânico.

⚠️ **Três seções abaixo descrevem um desenho que a rotação contínua tornou impossível** — a de passo
proporcional foi reescrita, e as de `servo_saturar()`/`servo_ir()` e da interface do módulo falam de
uma camada posicional que não existe. Leia-as como raciocínio, não como contrato.

Arquivos: `src/main.c` (hoje), `lib/controle/` (alvo).

⚠️ Implemente [05-camada-servo-e-calibracao.md](05-camada-servo-e-calibracao.md) **antes** desta
spec: enquanto cada comando de servo bloquear 1–2 s dentro do laço (**D-14**), o período de
amostragem é irregular e nenhum filtro em cima dele significa o que promete.

## A mistura, isolada em um lugar

Com o divisor em X, cada quadrante é uma direção — ver
[../03-hardware_specs/02-sensores-ldr-e-arranjo-optico.md](../03-hardware_specs/02-sensores-ldr-e-arranjo-optico.md).
Logo a mistura é subtração direta:

```c
/* ⭐ A MATRIZ DE MISTURA. É o único lugar que conhece a geometria óptica.
   Arranjo atual: 2x2 com aleta em CRUZ (+), então cada quadrante é uma
   DIAGONAL - e cada eixo sai de uma SOMA DE PARES.

   Se a aleta voltar a ser um "X", cada sensor volta a apontar para uma
   direção e ISTO é a única coisa que muda no projeto inteiro:
       *erro_h = mv[ESQ]  - mv[DIR];
       *erro_v = mv[CIMA] - mv[BAIXO];

   O SINAL depende da topologia do divisor, e ela está fechada (P-03): pull-down,
   10 kOhm para GND e LDR para 3,3 V, logo mais luz => mais tensão e as duas
   contas valem como estão. Se a montagem virar pull-up, inverta as duas. */
static void controle_erros(const int32_t mv[LDR_COUNT], int32_t *erro_h, int32_t *erro_v)
{
    *erro_h = (mv[LDR_SUP_ESQ] + mv[LDR_INF_ESQ])
            - (mv[LDR_SUP_DIR] + mv[LDR_INF_DIR]);

    *erro_v = (mv[LDR_SUP_ESQ] + mv[LDR_SUP_DIR])
            - (mv[LDR_INF_ESQ] + mv[LDR_INF_DIR]);
}
```

⚠️ **O erro tem o DOBRO da magnitude que teria no arranjo em X**, porque cada lado é a soma de dois
sensores. As constantes de calibração abaixo foram reescaladas na mesma proporção — e continuam
sendo chute, só que chute reescalado.

Erro positivo em `erro_h` significa "sol mais à esquerda". Que sentido de rotação isso pede depende
de como o servo está montado — o código atual soma `correcao` para ir à esquerda, e essa convenção
se mantém.

## O filtro: mediana de 5 por sensor

⛔ **O filtro atual está incoerente com o que o próprio log afirma.** O bloco em `src/main.c` faz
um laço duplo de `LDR_HISTORY_SIZE × LDR_HISTORY_SIZE` = **25 comparações cruzadas** e dispara com
`certeza >= certeza_parametro`, que é **5** — ou seja, **20% dos pares** —, imprimindo
*"Certeza absoluta"*. E os dois contadores podem passar do limiar **ao mesmo tempo** (basta o
histórico ter valores espalhados nos dois sensores), caso em que o `else if` escolhe a esquerda em
silêncio. É **D-10**.

A substituição é mais curta e faz o que o nome diz:

```c
#define AMOSTRAS 5   /* ímpar, para a mediana não precisar de média */

/* Um histórico circular por sensor. Mediana descarta outlier (nuvem, reflexo,
   transiente de corrente do servo) sem arrastar a média para o lado dele —
   que é justamente o que uma média móvel faria. */
static int32_t hist[LDR_COUNT][AMOSTRAS];
static int  idx, cheio;
```

A cada ciclo: `ldr_ler_todos()` grava uma coluna nova; quando o histórico enche, tira-se a
**mediana de 5 por sensor** (ordenação de 5 elementos, ou seleção do 3º), monta-se os dois erros a
partir das quatro medianas, e decide-se **uma vez**. Uma comparação por eixo em vez de 25 cruzadas,
e os dois eixos são independentes por construção — não existe mais o caso "as duas direções
ganharam".

## Passo proporcional com zona morta

O passo fixo de 10° de hoje (`correcao = 10`) tem os dois defeitos de qualquer passo fixo: lento
demais para recuperar um desalinhamento grande, grosso demais para assentar perto do alvo — e é
assentar mal que produz zig-zag.

⛔ **As quatro constantes viraram DUAS, e o passo continua FIXO — proporcional foi tentado e
revertido no mesmo dia (DEC-17).** Com rotação contínua o comando é *direção + tempo*, então o passo
se mede em milissegundos; o resto do desenho é o do código original.

```c
#define MARGEM_MV        60   /* diferenca abaixo disto e ruido/nuvem         */
#define PASSO_MS        200   /* passo fixo; graus = PASSO_MS * omega / 1000  */

BUILD_ASSERT(PASSO_MS >= SERVO_MS_MIN, "todo comando seria recusado");
```

```c
if      (erro >  MARGEM_MV) { dir = SERVO_ANTIHORARIO; }
else if (erro < -MARGEM_MV) { dir = SERVO_HORARIO;     }
else                        { return 0; }
```

⛔ **Por que `GANHO_MV_POR_GRAU` saiu, e não volta:** o erro em milivolts escala com a
**intensidade** da luz, não só com o ângulo — a mesma defasagem de 10° vale da ordem de 50 mV num
amanhecer e 500 mV ao meio-dia. Não existe ganho fixo a calibrar, e medir no bringup não resolveria:
não há número certo a medir. Raciocínio completo, com a alternativa que foi recusada junto
(normalizar pela soma dos quatro LDRs), em [../20-decisoes.md](../20-decisoes.md) **DEC-17**.

As duas constantes que sobram fazem trabalhos distintos:

- **`MARGEM_MV`** é o que impede o zig-zag por ruído. Tem de ficar **acima do piso de ruído** medido
  com a placa parada no alvo — número que sai do passo 1 do bringup.
- **`PASSO_MS`** é o tamanho da correção. `BUILD_ASSERT` garante que não caia abaixo de
  `SERVO_MS_MIN`, o que faria a camada recusar todo comando **em silêncio**.

⛔ **O zig-zag NÃO está resolvido por construção, e isso é honesto dizer.** Passo fixo de 200 ms são
~12° com ω = 60 °/s; se 12° levarem o erro a passar da margem do outro lado, o tracker corrige de
volta e oscila — exatamente o sintoma que o projeto anterior (o carrinho) nunca resolveu, e que o
`README.MD` da raiz ainda descreve. A defesa é **empírica**, e é por isso que os dois parâmetros
existem separados: subir `MARGEM_MV` até parar de vibrar, ou baixar `PASSO_MS` (com o piso do
`BUILD_ASSERT`). Não dá para calcular o ponto, porque calcular exigiria o mV por grau que não existe.

⭐ `MARGEM_MV` só é calibrável porque `lib/ldr/` passou a devolver **milivolts de verdade** —
ver [02-api-ldr-4-sensores.md](02-api-ldr-4-sensores.md).

## Limites: clamp, e uma vez só

⛔ Hoje o clamp acontece **antes** do teste de movimento, e o teste então repete a mesma condição:

```c
if (angulo >= 170) { angulo = 170; } else if (angulo <= 10) { angulo = 10; }
if (deve_mover && angulo <= 170 && angulo >= 10) { ... }   /* sempre verdadeiro */
```

Duas consequências. A segunda condição é morta (**D-12**); e, pior, ao **bater no limite** o
`deve_mover` continua verdadeiro, então o servo é recomandado ao ângulo em que já está — pagando os
2 s de espera bloqueante por nada, em todo ciclo, para sempre.

A forma certa: saturar, e mover só se a posição de fato mudou.

```c
int alvo = servo_saturar(eixo, atual + passo);
if (alvo != atual) { servo_ir(eixo, alvo); }
```

`servo_saturar()` mora em `lib/servo/` — os limites são propriedade mecânica do eixo, não do
controle. Limites em
[../03-hardware_specs/03-atuadores-mg90s.md](../03-hardware_specs/03-atuadores-mg90s.md):
horizontal 10°–170°, vertical **10°–160°** desde a resposta de **P-06** (provisório até P-08, porque
o fator 1,47 encurta o ângulo comandado que alcança o fim de curso).

## Quando amostrar: no fim da espera, nunca durante o movimento

⭐ **Regra, e ela é dona deste fato** (a spec de servo referencia daqui):

> A leitura acontece no **fim** da espera de assentamento, nunca no começo, e o que for lido
> enquanto o servo se move é **descartado** — não entra no histórico da mediana.

Duas razões independentes, e a segunda vale mesmo se a alimentação dos servos mudar:

1. **Elétrica.** A janela em que o servo se move é a **única** em que a corrente de servo circula,
   logo a única em que o acoplamento por GND existe (**DEC-11**, **P-10**). Amostrar fora dela torna
   o problema irrelevante **de graça** — não precisa de capacitor, de fonte nova nem de limiar mais
   alto.
2. **De controle.** Ruído correlacionado com a **própria ação** do laço é realimentação, e é assim
   que nasce oscilação auto-sustentada. Ruído aleatório não faz isso; ruído que aparece sempre
   *depois* de "decidi mover" faz. E ler durante o movimento mede uma posição que ainda está
   mudando — o valor não corresponde a ângulo nenhum.

Encaixa na separação `servo_ir()` / espera de
[05-camada-servo-e-calibracao.md](05-camada-servo-e-calibracao.md): quem orquestra comanda os dois
eixos, espera o **maior** dos dois movimentos, e **só então** chama `ldr_ler_todos()`.

⭐ Isso é o que faz o período de amostragem ficar **regular**, que é o que **D-14** cobra de fato —
não basta encurtar a espera, ela precisa ser sempre a mesma em relação ao instante da leitura.

## A interface do módulo

```c
typedef struct {
    int graus_h;      /* delta a aplicar no horizontal, já saturado. 0 = não mover */
    int graus_v;      /* idem vertical */
    int32_t erro_h;   /* para log e diagnóstico */
    int32_t erro_v;
} decisao_t;

/* Alimenta o histórico. Devolve 1 quando há decisão, 0 enquanto enche. */
int controle_passo(const int32_t mv[LDR_COUNT], int atual_h, int atual_v, decisao_t *out);
```

Sem `k_msleep`, sem `printk`, sem ADC, sem PWM: a decisão é função dos argumentos. Quem loga e
quem espera é o `main.c`. Isso importa porque não há framework de teste no projeto — a única
verificação disponível é raciocinar sobre a função, e isso só funciona se ela não tiver efeito
colateral. Ver [00-arquitetura-e-ordem-de-execucao.md](00-arquitetura-e-ordem-de-execucao.md).

## O que muda no `main.c`

Sai: os dois vetores `valor_ldr_d`/`valor_ldr_e`, `i`, `buffer_cheio`, o laço duplo de votação, os
`#define` no meio do corpo da função (`LDR_HISTORY_SIZE`, `certeza_parametro`) e a variável morta
`direcao` (**D-35**).

Entra: uma chamada a `ldr_ler_todos()`, uma a `controle_passo()`, e a aplicação da decisão nos dois
eixos — com o eixo vertical passando também pelo envelope de sanidade de
[04-eixo-vertical-feedforward.md](04-eixo-vertical-feedforward.md).
