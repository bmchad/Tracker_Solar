# A API do `lib/ldr/` para 4 sensores

Arquivos: `lib/ldr/ldr.h`, `lib/ldr/ldr.c`

## Por que a API quebra

`double ldr_ler(char lado)` tem dois problemas que quatro sensores tornam insustentáveis:

1. **`char lado` não escala.** Com `'e'` e `'d'` o despacho por caractere já era frágil; com cima e
   baixo entram `'c'` e `'b'` — e `'b'` de *baixo* colide com nada, mas `'c'` de *cima* e `'C'`
   viram convenção que ninguém lembra em três meses. Um enum é o mesmo esforço e o compilador
   confere.
2. ⛔ **O erro volta como valor.** Falha de ADC, lado inválido, sequência mal inicializada: tudo
   devolve `-1.0`. E `-1.0` é um `double` perfeitamente plausível de leitura baixa. Hoje o `main.c`
   guarda isso no histórico e **vota em cima do erro** — uma falha de leitura enviesa a direção do
   painel. É **D-01**, e com quatro sensores fica pior: uma falha num canal desequilibra uma
   subtração inteira.

**Sem camada de compatibilidade.** O protótipo tem um consumidor só (`main.c`, mais as rotinas de
teste) e a decisão é quebra limpa — ver [../20-decisoes.md](../20-decisoes.md).

## O header novo

```c
#ifndef LDR_H
#define LDR_H

#include <stdint.h>

/* ⚠️ A ORDEM deste enum é a ordem de io-channels no zephyr/frdm_kl25z.overlay.
   Os dois lados têm de casar, e nada no build pega a divergência. */
typedef enum {
    LDR_ESQ = 0,
    LDR_DIR,
    LDR_CIMA,
    LDR_BAIXO,
    LDR_COUNT          /* sempre por último: é o tamanho, não um sensor */
} ldr_id_t;

/* Configura os LDR_COUNT canais. 0 em sucesso, -errno no primeiro que falhar. */
int ldr_inicializar(void);

/* Leitura única. Devolve 0 e escreve em *mv_out, ou -errno e NÃO toca em *mv_out. */
int ldr_ler(ldr_id_t id, int32_t *mv_out);

/* Os quatro numa varredura só. 0 em sucesso, -errno se QUALQUER canal falhar. */
int ldr_ler_todos(int32_t mv[LDR_COUNT]);

/* Nome legível, para log. Nunca NULL, mesmo com id inválido. */
const char *ldr_nome(ldr_id_t id);

#endif /* LDR_H */
```

O valor passou de `double` para `int32_t`: milivolts inteiros bastam num ADC de 12 bits sobre
3,3 V (0,8 mV por LSB), e o Cortex-M0+ **não tem FPU** — todo `double` aqui era soft-float pago
sem retorno. Ver D-36.

## As três mudanças de fundo no `.c`

### 1. Um vetor de canais, não variáveis soltas

```c
#define ADC_NODE DT_PATH(zephyr_user)

static const struct adc_dt_spec canais[LDR_COUNT] = {
    ADC_DT_SPEC_GET_BY_IDX(ADC_NODE, 0),   /* LDR_ESQ   */
    ADC_DT_SPEC_GET_BY_IDX(ADC_NODE, 1),   /* LDR_DIR   */
    ADC_DT_SPEC_GET_BY_IDX(ADC_NODE, 2),   /* LDR_CIMA  */
    ADC_DT_SPEC_GET_BY_IDX(ADC_NODE, 3),   /* LDR_BAIXO */
};
```

`ldr_inicializar()` passa a ser um laço de 0 a `LDR_COUNT`, com `device_is_ready()` e
`adc_channel_setup_dt()` por canal — em vez dos dois blocos copiados de hoje, que já eram
duplicação e que com quatro sensores seriam quatro.

Todo acesso valida `id`: `if (id < 0 || id >= LDR_COUNT) return -EINVAL;`

### 2. Milivolts de verdade

⛔ **O que o código faz hoje não é conversão:**

```c
int32_t raw_value = sample_buffer[0];
millivolts = raw_value;
millivolts = millivolts * 0.889;   // 0.889 : valor arbitrario
```

Três defeitos numa linha e o próprio comentário admite o principal:

- a variável se chama `millivolts` mas carrega **contagem bruta escalada**;
- o fator é declaradamente arbitrário — a escala correta para 12 bits em 3,3 V seria
  3300 ÷ 4095 ≈ **0,806**, não 0,889;
- `millivolts` é `int32_t`, então a multiplicação por `double` **trunca** de volta para inteiro.

Consequência prática, e é ela que importa: a `margem = 20.0` do `main.c`, comentada como
*"tolerância de millivolts"*, **não está em milivolts**. É um número adimensional que ninguém pode
calibrar contra uma medição de multímetro. É **D-03**.

O Zephyr já resolve isso, e a função existe desde antes desta versão:

```c
int32_t mv = sample_buffer[0];
int err = adc_raw_to_millivolts_dt(&canais[id], &mv);
if (err < 0) { return err; }
*mv_out = mv;
```

`adc_raw_to_millivolts_dt` usa a referência e o ganho **declarados no devicetree**, então mudar
`zephyr,reference` ou `zephyr,resolution` no overlay passa a corrigir a conversão sozinho, em vez
de exigir um novo fator empírico.

⭐ **É esta mudança que faz os limiares da spec de controle serem calibráveis com multímetro** —
sem ela, `ZONA_MORTA_MV` continua sendo chute.

### 3. `ldr_ler_todos()`, e por que ele não é conveniência

```c
int ldr_ler_todos(int32_t mv[LDR_COUNT]);
```

O controle não usa os valores absolutos: usa **diferenças** entre eles. E uma diferença só é
válida se as parcelas forem do mesmo instante.

Quatro chamadas separadas a `ldr_ler()`, com um `printk` entre elas, medem quatro instantes
diferentes — e a serial a 115200 gasta cerca de 3 ms para despejar uma linha, mais do que a
conversão do ADC. Sob nuvem passando, ou com o transiente de corrente de um servo girando (**P-02**),
o "gradiente" calculado é em parte **temporal**, não espacial. O painel então persegue uma sombra
que já passou.

`ldr_ler_todos()` faz as quatro conversões em sequência apertada, sem log no meio, e falha inteiro
se um canal falhar. Ele é a **única** função que o laço de controle chama; `ldr_ler()` fica para o
bringup, onde ler um sensor por vez é justamente o objetivo.

## Detalhe que sobra de dívida

`sample_buffer` continua um `static` compartilhado. Fecha hoje porque há uma thread só, e passa a
ser corrida no dia em que o controle virar thread própria — registrado como **D-37**, não resolvido
aqui.
