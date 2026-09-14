#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <pwm_z42.h>
#include <teste_portas.h>

/* ---------------------------------------------------------------------------
   Constantes locais.

   ⚠️ D-34: TPM_MODULE ja esta definido em quatro lugares na arvore. Este
   arquivo NAO acrescenta um quinto com o mesmo nome - usa nome proprio e
   registra que o dono do numero e a tabela de PWM em
   context/03-hardware_specs/01-plataforma-e-pinagem.md.
   --------------------------------------------------------------------------- */
#define MOD_50HZ            7500   /* 375 kHz / 50 Hz - periodo de 20 ms      */
#define SEGUNDOS_POR_DEGRAU 5      /* tempo para o multimetro assentar        */
#define ESCADAS_ANTES_DE_SAIR 2    /* ~50 s por escada. Suba se faltar tempo  */

/* Pulso em que a escada TERMINA, antes de devolver o controle.
   ⛔ Nao pode ser o ultimo valor da fase 3 (750 = 2,0 ms = 180 graus): sair
   assim mandaria os dois eixos para o extremo, e no vertical 180 graus e a
   regiao de stall do D-41 - com o fator 1,47 o pulso vira 2,47 ms. */
#define CNV_REPOUSO         375    /* 1,0 ms = 0 graus                        */

#define CANAL_HORIZONTAL    3      /* TPM0_CH3 - PTD3 */
#define CANAL_VERTICAL      0      /* TPM0_CH0 - PTD0 */
#define PINO_HORIZONTAL     3
#define PINO_VERTICAL       0

/* ⚠️ O numero do alias NAO segue a ordem das cores na FRDM-KL25Z:
   led0 = verde (PTB19), led1 = azul (PTD1), led2 = VERMELHO (PTB18).
   Todos sao ATIVO EM NIVEL BAIXO - e por isso que se usa a variante _dt das
   funcoes, que aplica a inversao declarada no devicetree sozinha. */
static const struct gpio_dt_spec led_vermelho =
    GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);

static const struct device *const portad = DEVICE_DT_GET(DT_NODELABEL(gpiod));

static void esperar_degrau(void)
{
    k_msleep(SEGUNDOS_POR_DEGRAU * 1000);
}

/* --- Fase 0 -----------------------------------------------------------------
   Responde "o firmware esta rodando?" sem USB, sem serial e sem instrumento.
   Se este LED nao piscar, nenhuma medicao das fases seguintes significa nada:
   a investigacao vira gravacao, boot ou clock - nao pino.
   --------------------------------------------------------------------------- */
static void fase0_led_vivo(void)
{
    printk("[fase 0] LED vermelho piscando 1 Hz - o firmware esta vivo\n");

    if (!gpio_is_ready_dt(&led_vermelho)) {
        printk("  ERRO: LED nao esta pronto; as fases seguintes continuam\n");
        return;
    }

    gpio_pin_configure_dt(&led_vermelho, GPIO_OUTPUT_INACTIVE);

    for (int i = 0; i < SEGUNDOS_POR_DEGRAU; i++) {
        gpio_pin_set_dt(&led_vermelho, 1);   /* aceso  */
        k_msleep(500);
        gpio_pin_set_dt(&led_vermelho, 0);   /* apagado */
        k_msleep(500);
    }
}

/* --- Fase 1 -----------------------------------------------------------------
   O pino como GPIO puro: 3,3 V e 0 V, sem ambiguidade nenhuma no multimetro.

   ⭐ MEDIR NA PONTA DO JUMPER, no conector que vai ao servo - nao no pino da
   placa. Assim o mesmo teste pega fio partido e mau contato.

   Roda ANTES de qualquer pwm_tpm_Ch_Init: o mux do pino precisa estar em GPIO.
   --------------------------------------------------------------------------- */
static void fase1_gpio_um_pino(const char *nome, gpio_pin_t pino)
{
    int err = gpio_pin_configure(portad, pino, GPIO_OUTPUT_INACTIVE);

    if (err < 0) {
        printk("  ERRO ao configurar PTD%d como GPIO (%d)\n", pino, err);
        return;
    }

    printk("[fase 1] %s (PTD%d) em ALTO  - multimetro DC deve ler ~3,3 V\n",
           nome, pino);
    gpio_pin_set(portad, pino, 1);
    esperar_degrau();

    printk("[fase 1] %s (PTD%d) em BAIXO - multimetro DC deve ler ~0 V\n",
           nome, pino);
    gpio_pin_set(portad, pino, 0);
    esperar_degrau();
}

static void fase1_gpio(void)
{
    if (!device_is_ready(portad)) {
        printk("[fase 1] ERRO: porta D nao esta pronta; pulando\n");
        return;
    }

    fase1_gpio_um_pino("horizontal", PINO_HORIZONTAL);
    fase1_gpio_um_pino("vertical",   PINO_VERTICAL);
}

/* --- Fases 2 e 3 ------------------------------------------------------------
   Agora pelo caminho de verdade: TPM0 -> mux 4 -> pino.

   ⭐ O multimetro mede MEDIA, entao duty vira tensao. E por isso que da para
   validar PWM sem osciloscopio: 50% de duty sobre 3,3 V le ~1,65 V.

   ⚠️ Se os ~0,2 V da fase 3 ficarem ambiguos no seu multimetro, meca o mesmo
   pino em tensao AC: pino comutando da leitura AC nao nula, pino parado da ~0.
   Isso prova CHAVEAMENTO mesmo quando a media DC e pequena demais.
   --------------------------------------------------------------------------- */
static void aplicar_duty(const char *rotulo, uint16_t cnv, int mv_esperado)
{
    printk("[pwm] %s: CnV=%u  -> multimetro DC deve ler ~%d mV\n",
           rotulo, cnv, mv_esperado);

    pwm_tpm_CnV(TPM0, CANAL_HORIZONTAL, cnv);
    pwm_tpm_CnV(TPM0, CANAL_VERTICAL,   cnv);
    esperar_degrau();
}

static void fases_pwm(void)
{
    /* Re-muxa os dois pinos de GPIO de volta para TPM. CnSC |= mode e
       idempotente, entao repetir a cada volta da escada nao acumula nada
       diferente - ao contrario de pwm_tpm_Init, que faz SC |= e por isso fica
       fora do laco (D-02). */
    pwm_tpm_Ch_Init(TPM0, CANAL_HORIZONTAL, TPM_PWM_H, GPIOD, PINO_HORIZONTAL);
    pwm_tpm_Ch_Init(TPM0, CANAL_VERTICAL,   TPM_PWM_H, GPIOD, PINO_VERTICAL);

    printk("[fase 2] duty grosso - e esta fase que exonera ou condena o TPM0\n");
    aplicar_duty("0%",   0,          0);
    aplicar_duty("50%",  MOD_50HZ/2, 1650);
    aplicar_duty("100%", MOD_50HZ,   3300);

    printk("[fase 3] duty real de servo - 1,0 / 1,5 / 2,0 ms a 50 Hz\n");
    aplicar_duty("1,0 ms (0 graus)",   375, 165);
    aplicar_duty("1,5 ms (90 graus)",  563, 248);
    aplicar_duty("2,0 ms (180 graus)", 750, 330);
}

void teste_portas_servo(void)
{
    printk("\n=== TESTE DE PORTAS: PTD3 (horizontal) e PTD0 (vertical) ===\n");
    printk("Servo DESCONECTADO nas fases 0 a 2: nivel fixo e duty de 50%% nao\n");
    printk("sao pulso de servo valido.\n\n");

    /* SC |= ..., nao uma escrita: reinicializar com parametros diferentes
       acumularia bits. Por isso uma vez so, fora do laco (D-02). */
    pwm_tpm_Init(TPM0, TPM_PLLFLL, MOD_50HZ, TPM_CLK, PS_128, EDGE_PWM);

    for (int escada = 1; escada <= ESCADAS_ANTES_DE_SAIR; escada++) {
        printk("--- escada %d de %d ---\n", escada, ESCADAS_ANTES_DE_SAIR);
        fase0_led_vivo();
        fase1_gpio();
        fases_pwm();
    }

    /* Sai em 0 graus, nao nos 180 em que a fase 3 termina - ver CNV_REPOUSO. */
    pwm_tpm_CnV(TPM0, CANAL_HORIZONTAL, CNV_REPOUSO);
    pwm_tpm_CnV(TPM0, CANAL_VERTICAL,   CNV_REPOUSO);
    printk("=== fim do teste de portas; pinos em %u (1,0 ms = 0 graus) ===\n\n",
           CNV_REPOUSO);
}
