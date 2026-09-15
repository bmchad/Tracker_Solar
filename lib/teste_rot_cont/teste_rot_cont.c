#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <pwm_z42.h>
#include <teste_portas.h>      /* teste_portas_recibo(): o LED como recibo */
#include <teste_rot_cont.h>

/* ---------------------------------------------------------------------------
   A aritmetica, toda derivada da base de tempo do TPM.

   Com clock de 375 kHz e MOD = 7500, o periodo e 20 ms (50 Hz) e cada
   milissegundo de pulso vale 375 contagens. Para servo de rotacao continua:

       1,0 ms = 375 contagens -> velocidade maxima, sentido A
       1,5 ms = 563 contagens -> PARADO (neutro)
       2,0 ms = 750 contagens -> velocidade maxima, sentido B

   Logo a excursao de velocidade e +-188 contagens em torno do neutro, e "x% da
   velocidade" e simplesmente x% dessas 188.
   --------------------------------------------------------------------------- */
#define MOD_50HZ         7500
#define CNV_NEUTRO        563   /* 1,50 ms - parado                            */
#define EXCURSAO_MAX      188   /* 0,50 ms - do neutro ate a velocidade maxima */

/* ⚠️ 10% e POUCO, e pode nao ser suficiente para o servo sair do lugar.
   Servo de rotacao continua tem uma ZONA MORTA em torno do neutro, tipicamente
   de +-45 a +-90 us. E 10% de 0,50 ms sao 50 us - ou seja, bem dentro dessa
   faixa em muitos servos.

   ⭐ Se o eixo nao se mexer, NAO e defeito: suba este numero para 25 ou 30 e
   grave de novo. O primeiro valor em que ele comeca a girar E a zona morta
   medida, e vale anotar. */
#define VELOCIDADE_PCT     10

#define PASSO_CNV        ((EXCURSAO_MAX * VELOCIDADE_PCT) / 100)
#define CNV_SENTIDO_A    (CNV_NEUTRO - PASSO_CNV)
#define CNV_SENTIDO_B    (CNV_NEUTRO + PASSO_CNV)

#define CANAL_HORIZONTAL   3    /* TPM0_CH3 - PTD3 */
#define CANAL_VERTICAL     0    /* TPM0_CH0 - PTD0 */

#define DEGRAU_MS        1000

/* Aciona os DOIS canais: assim o teste funciona com o servo em qualquer um dos
   dois conectores, sem precisar adivinhar qual esta ligado. */
static void aplicar(const char *rotulo, uint16_t cnv)
{
    printk("[rot-cont] %-22s CnV=%3u  (%d,%02d ms)\n",
           rotulo, cnv, cnv / 375, ((cnv % 375) * 100) / 375);

    pwm_tpm_CnV(TPM0, CANAL_HORIZONTAL, cnv);
    pwm_tpm_CnV(TPM0, CANAL_VERTICAL,   cnv);
    k_msleep(DEGRAU_MS);
}

void teste_rot_cont(void)
{
    printk("\n=== TESTE DE ROTACAO CONTINUA ===\n");
    printk("Pulso = VELOCIDADE, nao angulo. Neutro=%u, passo=%u (%d%% da "
           "velocidade maxima)\n", CNV_NEUTRO, PASSO_CNV, VELOCIDADE_PCT);
    printk("Se o eixo nao girar, suba VELOCIDADE_PCT: 10%% cai dentro da zona\n"
           "morta de muitos servos.\n\n");

    teste_portas_recibo();   /* pisca o LED: prova que esta gravacao chegou */

    for (;;) {
        aplicar("sentido A, devagar",  CNV_SENTIDO_A);
        aplicar("parado (neutro)",     CNV_NEUTRO);
        aplicar("sentido B, devagar",  CNV_SENTIDO_B);
        aplicar("parado (neutro)",     CNV_NEUTRO);
    }
}
