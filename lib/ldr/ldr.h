#ifndef LDR_H
#define LDR_H

#include <stdint.h>

/* ⚠️ A ORDEM deste enum E a ordem de io-channels em zephyr/frdm_kl25z.overlay.
   Os dois lados tem de casar, e trocar duas entradas NAO produz erro de
   compilacao: o sintoma e o painel virando para o lado errado.
   O BUILD_ASSERT em ldr.c confere a QUANTIDADE, nunca a ordem. */
typedef enum {
    LDR_ESQ = 0,
    LDR_DIR,
    LDR_CIMA,
    LDR_BAIXO,
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
