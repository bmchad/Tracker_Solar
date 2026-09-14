# A API do `lib/ldr/` para 4 sensores

Arquivos: `lib/ldr/ldr.h`, `lib/ldr/ldr.c`

> ✅ **IMPLEMENTADO em 2026-09-14.** O header e o `.c` no repositório já são os descritos aqui, e
> compilam. O que saiu **diferente** da spec está na última seção — leia antes de tratar este texto
> como descrição fiel do código.

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

O Zephyr resolve isso — **mas há uma armadilha neste driver, e ela é silenciosa. Leia o D-19
antes de escrever esta linha.**

```c
int32_t mv = sample_buffer[0];

/* ⚠️ EXIGE, nos nós de canal do overlay:
     zephyr,vref-mv = <3300>;
     zephyr,reference = "ADC_REF_EXTERNAL0";   (qualquer coisa != ADC_REF_INTERNAL)

   Com ADC_REF_INTERNAL esta função devolve 0 mV com codigo de SUCESSO, porque o
   adc_mcux_adc16 nunca preenche ref_internal. Ver D-19. */
int err = adc_raw_to_millivolts_dt(&canais[id], &mv);
if (err < 0) { return err; }
*mv_out = mv;
```

⛔ **O modo de falha é o pior possível.** Se o overlay ficar em `ADC_REF_INTERNAL`, os quatro
sensores leem **0 mV**, os dois erros ficam sempre zero, a zona morta nunca é excedida e o tracker
**não se move** — sem erro, sem log, com todas as chamadas retornando sucesso. E na tela isso é
indistinguível de "está escuro". Trocar o `× 0.889` por isto sem mexer no overlay é **pior que não
mexer em nada**: a escala errada de hoje pelo menos varia com a luz.

Com o overlay corrigido ([01-devicetree-adc-4-canais.md](01-devicetree-adc-4-canais.md)),
`adc_raw_to_millivolts_dt` passa a usar o `zephyr,vref-mv` e o ganho declarados, então mudar a
referência ou a resolução no devicetree corrige a conversão sozinho, em vez de exigir um novo fator
empírico.

⭐ **É esta mudança que faz os limiares da spec de controle serem calibráveis com multímetro** —
sem ela, `ZONA_MORTA_MV` continua sendo chute.

**Alternativa aceitável, se preferir não depender do overlay:** a conta à mão,
`mv = (raw * 3300) / 4096`. Perde a vantagem de o devicetree ser a fonte da escala, mas não tem
como falhar em silêncio — e é honesta sobre o 3,3 V ser nominal, não medido.

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

## ⚠️ Canal sem sensor conectado **não** é erro de inicialização

Vale escrito porque a intuição diz o contrário, e a crença já apareceu na bancada: com os LDRs
desligados, `ldr_inicializar()` **não** devolve erro.

- `device_is_ready()` pergunta se o **driver do `adc0`** está pronto — ele não tem como saber o que
  está pendurado no pino;
- `adc_channel_setup_dt()` valida **canal, tempo de aquisição, diferencial e ganho**, e nada mais
  (mesma leitura de `adc_mcux_adc16.c:102-132` que a spec do overlay já cita).

Entrada analógica solta lê **valor sem sentido, não erro** — tipicamente flutuando conforme o que
houver de acoplamento no ar. Então "o tracker não dá boot sem os sensores" é falso, e usar isso para
explicar uma falha fecha, por engano, uma hipótese que nunca foi testada.

## Detalhe que sobra de dívida

`sample_buffer` continua um `static` compartilhado. Fecha hoje porque há uma thread só, e passa a
ser corrida no dia em que o controle virar thread própria — registrado como **D-37**, não resolvido
aqui.

## O que saiu diferente desta spec, na implementação

Três coisas que o texto acima não previa, todas de 2026-09-14:

1. ⭐ **`BUILD_ASSERT(DT_PROP_LEN(ADC_NODE, io_channels) == LDR_COUNT)`.** A spec dizia que nada no
   build pega a divergência entre overlay e enum. Metade disso deixou de ser verdade: a
   **quantidade** passa a ser erro de compilação. A **ordem** continua sem rede — só o teste do dedo
   no passo 1 do bringup. Foi este assert que expôs o **D-43** (o `pio run` reusando um devicetree
   antigo) como erro de build em vez de sensor mudo em campo.
2. **`ler_canal()`, estático e sem nenhum `printk`.** A conversão de um canal virou função privada
   sem log, e é o que permite a `ldr_ler_todos()` encostar as quatro leituras no tempo de verdade.
   Quem loga é o chamador — `ldr_ler()` para o bringup, `ldr_ler_todos()` só no caminho de falha.
3. **`ldr_ler_todos()` escreve num vetor temporário e copia no fim.** A spec pedia "falha inteiro se
   um canal falhar"; retornar no meio do laço deixaria o vetor do chamador **meio preenchido**, com
   valores novos ao lado de valores da varredura anterior — exatamente a mistura de instantes que a
   função existe para impedir. Custa 16 bytes de pilha.

E uma correção de rota que veio junto: **`ldr.c` não incluía `ldr.h`**. As assinaturas das
definições nunca foram conferidas contra as declarações — a mesma família do **D-07**.
