#ifndef LDR_H
#define LDR_H

#include <stdint.h>

/* ⭐ ARRANJO EM CRUZ (`+`): os quadrantes sao DIAGONAIS, nao direcoes.

   Ate 2026-09-15 o arranjo era em X, e cada sensor apontava para uma direcao -
   dai os nomes ESQ/DIR/CIMA/BAIXO. Com a cruz isso deixou de ser verdade: nao
   existe "o sensor da esquerda", existem dois sensores a esquerda, um em cima e
   um embaixo. Os nomes antigos mentiam sobre o hardware - `LDR_CIMA` era,
   fisicamente, o inferior esquerdo.

   Com a cruz, cada eixo sai de uma SOMA DE PARES:

       erro_horizontal = (SUP_ESQ + INF_ESQ) - (SUP_DIR + INF_DIR)
       erro_vertical   = (SUP_ESQ + SUP_DIR) - (INF_ESQ + INF_DIR)

   ⚠️ A ORDEM deste enum E a ordem de io-channels em zephyr/frdm_kl25z.overlay.
   Os dois lados tem de casar, e trocar duas entradas NAO produz erro de
   compilacao: o sintoma e o painel virando para o lado errado.
   O BUILD_ASSERT em ldr.c confere a QUANTIDADE, nunca a ordem. */
typedef enum {
    LDR_SUP_ESQ = 0,   /* PTB0 = A0 = SE8  */
    LDR_SUP_DIR,       /* PTB2 = A2 = SE12 */
    LDR_INF_ESQ,       /* PTB1 = A1 = SE9  */
    LDR_INF_DIR,       /* PTB3 = A3 = SE13 */
    LDR_COUNT          /* sempre por ultimo: e o tamanho, nao um sensor */
} ldr_id_t;

/* Configura os LDR_COUNT canais. 0 em sucesso, -errno no primeiro que falhar. */
int ldr_inicializar(void);

/* Leitura unica, em MILIVOLTS de verdade. Para o bringup, onde ler um sensor
   por vez e o objetivo; o laco de controle usa ldr_ler_todos().

   D-01: erro e valor viajam por canais SEPARADOS. Devolve 0 e escreve em
   *mv_out; em erro devolve -errno e NAO toca em *mv_out. Antes isto devolvia
   -1.0 em qualquer falha, valor indistinguivel de uma leitura baixa valida - e
   o main.c guardava esse -1.0 no historico e votava em cima dele, movendo a
   placa por causa de um erro de barramento. */
int ldr_ler(ldr_id_t id, int32_t *mv_out);

/* Os quatro numa varredura so, sem nada entre as conversoes. 0 em sucesso,
   -errno se QUALQUER canal falhar - e nesse caso mv[] NAO e tocado.

   Nao e conveniencia: o controle usa DIFERENCAS entre sensores, e uma
   diferenca so vale se as parcelas forem do mesmo instante. Quatro chamadas
   separadas com um printk no meio medem quatro instantes (a serial a 115200
   gasta ~3 ms por linha, mais que a conversao), e sob nuvem ou transiente de
   servo o "gradiente" calculado vira em parte temporal, nao espacial. */
int ldr_ler_todos(int32_t mv[LDR_COUNT]);

/* Nome legivel, para log. Nunca NULL, mesmo com id invalido. */
const char *ldr_nome(ldr_id_t id);

#endif /* LDR_H */
