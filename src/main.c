/* Teste de bancada do POTENCIOMETRO - projeto real, mega_context/angulo.md.

   A FRDM-KL25Z do prototipo entra aqui so como instrumento de bancada. Toda a
   leitura mora em lib/potenciometro/; este arquivo guarda a CALIBRACAO (que e
   da montagem) e imprime o angulo no console do kernel (printk, 115200).

   LIGACAO (potenciometro LINEAR de 5 kOhm):
       extremidade 1  -> 3V3
       extremidade 2  -> GND
       cursor (meio)  -> A4 = PTC2
   ⛔ NUNCA 5 V no potenciometro.

   PROTOCOLO (angulo.md secao 8):
     1. Grave com a calibracao NOMINAL abaixo. Gire de ponta a ponta: N tem de
        ir de perto de 0 a perto de 4095 SEM SALTOS.
     2. Com o transferidor, anote N em 0 graus e em 90 graus.
     3. Troque a calibracao pelos dois pontos medidos, grave de novo e confira
        10, 20, ... 80 graus contra o transferidor. A diferenca e o erro de
        linearidade do potenciometro. */

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <pisca_led.h>
#include <potenciometro.h>

#define PERIODO_MS   250

/* Sequencia de boot: pisca vermelho, amarelo e verde, 5 s cada (15 s).
   Prova que o MAIN rodou, nao so que o gravador disse "ok": se a placa nao
   piscar logo depois do upload, ou o firmware nao chegou ou travou antes do
   main (o HardFault que o zephyr_link_fix.py corrige). Bloqueia de proposito -
   e instrumento de bancada, nao tracking. */
#define BOOT_FASE_MS        5000
#define BOOT_MEIO_PISCA_MS  250    /* 250 aceso + 250 apagado = 2 piscadas/s */

static void piscar_por(led_cor_t cor, int32_t duracao_ms)
{
    for (int32_t t = 0; t < duracao_ms; t += 2 * BOOT_MEIO_PISCA_MS) {
        led(cor);
        k_msleep(BOOT_MEIO_PISCA_MS);
        led(LED_APAGADO);
        k_msleep(BOOT_MEIO_PISCA_MS);
    }
}

/* Calibracao de 2 pontos (angulo.md 3.2), em decimos de grau.

   Enquanto nao for medida, fica a NOMINAL: a escala inteira do ADC vale os
   ~300 graus eletricos de catalogo. Serve para ver o potenciometro andar, nao
   para medir - o angulo eletrico real tem tolerancia de alguns graus.

   Depois do passo 2 do protocolo, por exemplo:
       .n_a = 1210, .d10_a = 0,       <- N lido com o transferidor em 0 graus
       .n_b = 2440, .d10_b = 900,     <- N lido com o transferidor em 90 graus */
static const pot_calibracao_t calibracao = {
    .n_a = 0,    .d10_a = 0,
    .n_b = 4095, .d10_b = 3000,
};

/* Imprime decimos como "12,3" / "-0,5" - printk nao tem float aqui. */
static void imprimir_d10(int32_t d10)
{
    if (d10 < 0) {
        printk("-");
        d10 = -d10;
    }
    printk("%d,%d", d10 / 10, d10 % 10);
}

int main(void)
{
    printk("\n[boot] main rodando: vermelho, amarelo e verde piscando, 5 s cada\n");
    piscar_por(LED_VERMELHO, BOOT_FASE_MS);
    piscar_por(LED_AMARELO,  BOOT_FASE_MS);
    piscar_por(LED_VERDE,    BOOT_FASE_MS);

    /* Recibo de gravacao: se a placa NAO mudar de cor, o upload nao chegou.
       A sequencia acima e igual em todo build; so esta cor diferencia um
       upload do anterior. */
    led(COR_DESTA_GRAVACAO);

    printk("\n=== TESTE DO POTENCIOMETRO (A4 = PTC2) ===\n");
    printk("Calibracao: N=%d -> ", calibracao.n_a);
    imprimir_d10(calibracao.d10_a);
    printk(" graus, N=%d -> ", calibracao.n_b);
    imprimir_d10(calibracao.d10_b);
    printk(" graus\n\n");

    int err = pot_inicializar(&calibracao);
    if (err < 0) {
        printk("Falha ao inicializar o potenciometro (%d). Parado.\n", err);
        led(LED_VERMELHO);
        return 0;
    }

    while (1) {
        pot_leitura_t l = {0};

        err = pot_ler(&l);

        if (err == 0 || err == -ERANGE) {
            printk("angulo = ");
            imprimir_d10(l.d10);
            printk(" graus | N=%4d ruido %2d", l.n, l.ruido);

            if (err == -ERANGE) {
                printk("  <- colado no trilho: ponta do curso ou fio solto?");
            }
            printk("\n");
        } else {
            printk("Erro de leitura do potenciometro (%d)\n", err);
        }

        k_msleep(PERIODO_MS);
        printk("Funcionando!\n");
    }

    return 0;
}
