#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <ldr.h>
#include <teste_ldr.h>

/* D-34: inclui so o que usa. Este arquivo nao toca em PWM, relogio, matematica
   solar nem servo - e ate 2026-09-15 incluia os quatro, herdados por copia do
   antigo testes.c. */

#define PAUSA_MS        500

/* ⭐ Qual quadrante esta mais claro e qual mais escuro.

   Sem isto, o teste do dedo obriga a comparar quatro numeros a olho, na hora, e
   e facil errar quando dois estao proximos. Com isto, cobrir um sensor e ler um
   nome: se o nome que escurece nao for o do sensor que voce cobriu, a ordem de
   io-channels nao casa com o enum. */
static void extremos(const int32_t mv[LDR_COUNT], int *claro, int *escuro)
{
    *claro = 0;
    *escuro = 0;

    for (int i = 1; i < LDR_COUNT; i++) {
        if (mv[i] > mv[*claro])  { *claro = i;  }
        if (mv[i] < mv[*escuro]) { *escuro = i; }
    }
}

int teste_ldr(int n)
{
    printk("\n=== TESTE DOS 4 LDRs (arranjo em CRUZ) ===\n");
    printk("Cobrir UM sensor confere a pinagem; cobrir DOIS do mesmo lado\n");
    printk("confere a soma de pares.\n\n");

    /* D-01: erro separado do valor. Uma leitura que falha aparece como falha,
       nao como um numero baixo plausivel.

       Uma chamada so, e de proposito: ldr_ler_todos() faz as quatro conversoes
       encostadas, sem printk no meio. Quatro ldr_ler() com log entre eles
       mediriam quatro instantes diferentes (a serial a 115200 gasta ~3 ms por
       linha), e a diferenca entre quadrantes viraria em parte temporal. */
    /* n == 0 e INFINITO, na mesma convencao dos outros tres testes. Aqui ele e
       o modo mais util: para medir o PISO DE RUIDO precisa-se da placa parada
       por um minuto, e nao de dez leituras em cinco segundos. */
    for (int i = 0; n == 0 || i < n; i++) {
        int32_t mv[LDR_COUNT] = {0};
        int err = ldr_ler_todos(mv);

        if (err < 0) {
            printk("LDR: varredura falhou (%d)\n", err);
            k_msleep(PAUSA_MS);
            continue;
        }

        /* ⭐ ARRANJO EM CRUZ: SOMA DE PARES, nao subtracao de sensores
           individuais. Com a cruz os quadrantes sao diagonais, entao cada eixo
           usa os QUATRO sensores.

           ⚠️ O SINAL destas duas contas e a mistura do controle, e ela e dona de
           context/02-software_specs/03-controle-4-quadrantes.md. Aqui e so o
           mesmo sentido, para o teste do dedo bater com o que o controle vai
           enxergar.

           ⚠️ O erro tem o DOBRO da magnitude do arranjo em X - sao dois sensores
           somados de cada lado. MARGEM_MV, em main.c, ja foi reescalada na
           mesma proporcao. */
        int32_t esq   = mv[LDR_SUP_ESQ] + mv[LDR_INF_ESQ];
        int32_t dir   = mv[LDR_SUP_DIR] + mv[LDR_INF_DIR];
        int32_t cima  = mv[LDR_SUP_ESQ] + mv[LDR_SUP_DIR];
        int32_t baixo = mv[LDR_INF_ESQ] + mv[LDR_INF_DIR];

        int claro, escuro;
        extremos(mv, &claro, &escuro);

        printk("se=%4d sd=%4d ie=%4d id=%4d mV | erro_h=%+5d erro_v=%+5d | "
               "claro=%s escuro=%s\n",
               mv[LDR_SUP_ESQ], mv[LDR_SUP_DIR],
               mv[LDR_INF_ESQ], mv[LDR_INF_DIR],
               (int)(esq - dir), (int)(cima - baixo),
               ldr_nome(claro), ldr_nome(escuro));

        k_msleep(PAUSA_MS);
    }

    return 0;
}
