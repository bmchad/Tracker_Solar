#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <pwm_z42.h>
#include <teste_portas.h>      /* recibo de gravacao e controle do LED */
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

/* --- Medicao de omega ------------------------------------------------------ */

#define OMEGA_MS_GIRO      5000   /* 5 s girando: o varrido a medir      */
#define OMEGA_MS_PAUSA     8000   /* 8 s parado: tempo de marcar e medir */

/* Velocidade da MEDICAO. ⚠️ Tem de ser a MESMA que a camada de servo usa em
   operacao (VELOCIDADE_PCT no sevo_motor.c), senao o omega medido nao
   descreve o movimento real - velocidade diferente, omega diferente. */
#define OMEGA_VEL_PCT        30
#define OMEGA_PASSO        ((EXCURSAO_MAX * OMEGA_VEL_PCT) / 100)

static void medir_um(const char *eixo, uint16_t canal,
                     const char *sentido, uint16_t cnv)
{
    printk("\n[omega] %s / %s: MARQUE a posicao agora (%d s)\n",
           eixo, sentido, OMEGA_MS_PAUSA / 1000);
    teste_portas_led(0);
    k_msleep(OMEGA_MS_PAUSA);

    printk("[omega] %s / %s: GIRANDO por %d s...\n",
           eixo, sentido, OMEGA_MS_GIRO / 1000);
    teste_portas_led(1);
    pwm_tpm_CnV(TPM0, canal, cnv);
    k_msleep(OMEGA_MS_GIRO);
    pwm_tpm_CnV(TPM0, canal, CNV_NEUTRO);
    teste_portas_led(0);

    printk("[omega] %s / %s: PARADO. Meca o varrido; omega = angulo / %d s\n",
           eixo, sentido, OMEGA_MS_GIRO / 1000);
}

void teste_rot_cont_medir_omega(void)
{
    printk("\n=== MEDICAO DE OMEGA (graus/s) ===\n");
    printk("LED ACESO = girando. LED APAGADO = marque ou meca.\n");
    printk("Sao QUATRO numeros: cada eixo em cada sentido - servo de rotacao\n");
    printk("continua e assimetrico, e um omega so embute erro que acumula.\n");

    teste_portas_recibo();

    for (;;) {
        medir_um("horizontal", CANAL_HORIZONTAL, "sentido A",
                 CNV_NEUTRO + OMEGA_PASSO);
        medir_um("horizontal", CANAL_HORIZONTAL, "sentido B",
                 CNV_NEUTRO - OMEGA_PASSO);
        medir_um("vertical",   CANAL_VERTICAL,   "sentido A",
                 CNV_NEUTRO + OMEGA_PASSO);
        medir_um("vertical",   CANAL_VERTICAL,   "sentido B",
                 CNV_NEUTRO - OMEGA_PASSO);
    }
}
