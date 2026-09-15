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

/* ⚠️ Com a serial muda, nao da para saber QUAL fase esta rodando - por isso as
   fases ficam atras de chaves de compilacao, e o padrao e so a Fase 1, que e
   legivel com multimetro e LED, sem depender de texto nenhum.
   Trocar 0 por 1 religa a fase. */
#define RODAR_FASE_0_LED    0      /* piscar o LED como fase separada         */
#define RODAR_FASE_1_GPIO   1      /* nivel alto/baixo: testa pino e jumper   */
#define RODAR_FASE_2_DUTY   0      /* duty grosso 0/50/100%: testa o TPM0     */
#define RODAR_FASE_3_SERVO  1      /* 1,0 / 1,5 / 2,0 ms: o SERVO deve MOVER  */

/* ⛔ Fase 2 com servo conectado: duty de 50% e 100% NAO sao pulso de servo
   valido - ele zumbe contra o batente sem obedecer. So ligue a fase 2 com o
   servo desconectado, para medir o pino. */

/* Pulso em que a escada TERMINA, antes de devolver o controle.
   ⛔ Nao pode ser o ultimo valor da fase 3 (750 = 2,0 ms = 180 graus): sair
   assim mandaria os dois eixos para o extremo, e no vertical 180 graus e a
   regiao de stall do D-41 - com o fator 1,47 o pulso vira 2,47 ms. */
#define CNV_REPOUSO         375    /* 1,0 ms = 0 graus                        */

#define CANAL_HORIZONTAL    3      /* TPM0_CH3 - PTD3 */
#define CANAL_VERTICAL      0      /* TPM0_CH0 - PTD0 */
#define PINO_HORIZONTAL     3
#define PINO_VERTICAL       0

/* ⭐ RECIBO DE GRAVACAO: troque este numero a cada build/upload.
   Se a placa NAO mudar de cor depois de gravar, o upload nao chegou ao alvo -
   e foi exatamente assim que se perdeu o dia 2026-09-14 inteiro, medindo
   sintomas de um firmware que nunca subiu. Ver context/CONTINUAR-AQUI.md.

       0 = VERDE (led0, PTB19)
       1 = AZUL  (led1, PTD1)
       2 = VERMELHO (led2, PTB18)

   ⚠️ O numero do alias NAO segue a ordem das cores na FRDM-KL25Z - o verde e o
   led0, nao o led1. Os tres sao ATIVO EM NIVEL BAIXO, e e por isso que se usa a
   variante _dt das funcoes: ela aplica a inversao declarada no devicetree
   sozinha.

   ⚠️ O AZUL fica em PTD1 = TPM0_CH1 - mesma porta e mesmo timer dos pinos de
   servo. Como GPIO nao ha conflito, mas quem habilitar o canal 1 do TPM0 um dia
   vai disputar esse pino. Verde e vermelho estao no port B, longe de tudo. */
#define LED_DO_TESTE  1

#if   LED_DO_TESTE == 0
#  define LED_NODE DT_ALIAS(led0)
#  define LED_COR  "VERDE"
#elif LED_DO_TESTE == 1
#  define LED_NODE DT_ALIAS(led1)
#  define LED_COR  "AZUL"
#else
#  define LED_NODE DT_ALIAS(led2)
#  define LED_COR  "VERMELHO"
#endif

static const struct gpio_dt_spec led_teste = GPIO_DT_SPEC_GET(LED_NODE, gpios);

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
    printk("[fase 0] LED " LED_COR " piscando 1 Hz - o firmware esta vivo\n");

    if (!gpio_is_ready_dt(&led_teste)) {
        printk("  ERRO: LED nao esta pronto; as fases seguintes continuam\n");
        return;
    }

    gpio_pin_configure_dt(&led_teste, GPIO_OUTPUT_INACTIVE);

    for (int i = 0; i < SEGUNDOS_POR_DEGRAU; i++) {
        gpio_pin_set_dt(&led_teste, 1);   /* aceso  */
        k_msleep(500);
        gpio_pin_set_dt(&led_teste, 0);   /* apagado */
        k_msleep(500);
    }
}

/* --- Fase 1 -----------------------------------------------------------------
   Os dois pinos como GPIO puro, ACIONADOS JUNTOS, com o LED espelhando o
   estado. Le-se sem serial e sem saber que fase esta rodando:

       LED ACESO   -> PTD3 e PTD0 devem medir ~3,3 V
       LED APAGADO -> os dois devem medir ~0 V

   ⚠️ Acionar um pino por vez (como era antes) deixava o outro em 0 V por 15 de
   cada 20 s - indistinguivel de "pino morto" sem a serial dizendo de quem era
   a vez. Juntos, qualquer pino que se meca segue o mesmo ritmo do LED.

   ⭐ MEDIR NA PONTA DO JUMPER, no conector que vai ao servo - nao no pino da
   placa. Assim o mesmo teste pega fio partido e mau contato.

   Roda ANTES de qualquer pwm_tpm_Ch_Init: o mux do pino precisa estar em GPIO.
   --------------------------------------------------------------------------- */
static bool fase1_preparar(void)
{
    if (!device_is_ready(portad)) {
        printk("[fase 1] ERRO: porta D nao esta pronta\n");
        return false;
    }

    int e1 = gpio_pin_configure(portad, PINO_HORIZONTAL, GPIO_OUTPUT_INACTIVE);
    int e2 = gpio_pin_configure(portad, PINO_VERTICAL,   GPIO_OUTPUT_INACTIVE);

    if (e1 < 0 || e2 < 0) {
        printk("[fase 1] ERRO ao configurar PTD3/PTD0 como GPIO (%d, %d)\n",
               e1, e2);
        return false;
    }

    /* O LED e o unico indicador que sobrevive a serial muda. Se ele nao estiver
       pronto o teste continua - o multimetro ainda vale -, mas sem espelho. */
    if (gpio_is_ready_dt(&led_teste)) {
        gpio_pin_configure_dt(&led_teste, GPIO_OUTPUT_INACTIVE);
    }

    return true;
}

static void fase1_aplicar(int nivel)
{
    gpio_pin_set(portad, PINO_HORIZONTAL, nivel);
    gpio_pin_set(portad, PINO_VERTICAL,   nivel);

    if (gpio_is_ready_dt(&led_teste)) {
        gpio_pin_set_dt(&led_teste, nivel);   /* espelho */
    }

    printk("[fase 1] PTD3 e PTD0 em %s - LED " LED_COR " %s - multimetro DC ~%s\n",
           nivel ? "ALTO " : "BAIXO",
           nivel ? "ACESO " : "APAGADO",
           nivel ? "3,3 V" : "0 V");

    esperar_degrau();
}

static void fase1_gpio(void)
{
    fase1_aplicar(1);
    fase1_aplicar(0);
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

    if (RODAR_FASE_2_DUTY) {
        printk("[fase 2] duty grosso - exonera ou condena o TPM0\n");
        aplicar_duty("0%",   0,          0);
        aplicar_duty("50%",  MOD_50HZ/2, 1650);
        aplicar_duty("100%", MOD_50HZ,   3300);
    }

    if (RODAR_FASE_3_SERVO) {
        /* ⭐ E AQUI que o servo tem de se MEXER. As fases anteriores medem
           tensao; esta emite pulso de servo de verdade, e o teste deixa de ser
           de multimetro e passa a ser de olho: o eixo anda, ou nao anda. */
        printk("[fase 3] pulso de servo - o eixo DEVE se mover a cada degrau\n");
        aplicar_duty("1,0 ms (0 graus)",   375, 165);
        aplicar_duty("1,5 ms (90 graus)",  563, 248);
        aplicar_duty("2,0 ms (180 graus)", 750, 330);
    }
}

void teste_portas_recibo(void)
{
    printk("[recibo] LED " LED_COR " - se a cor nao mudou, o upload NAO chegou\n");

    if (!gpio_is_ready_dt(&led_teste)) {
        printk("[recibo] ERRO: LED nao esta pronto\n");
        return;
    }

    gpio_pin_configure_dt(&led_teste, GPIO_OUTPUT_INACTIVE);

    for (int i = 0; i < 3; i++) {
        gpio_pin_set_dt(&led_teste, 1);
        k_msleep(250);
        gpio_pin_set_dt(&led_teste, 0);
        k_msleep(250);
    }
}

void teste_portas_led(int aceso)
{
    if (!gpio_is_ready_dt(&led_teste)) {
        return;
    }

    gpio_pin_configure_dt(&led_teste, GPIO_OUTPUT_INACTIVE);
    gpio_pin_set_dt(&led_teste, aceso ? 1 : 0);
}

void teste_portas_servo(void)
{
    printk("\n=== TESTE DE PORTAS: PTD3 (horizontal) e PTD0 (vertical) ===\n");
    printk("Servo DESCONECTADO nas fases 0 a 2: nivel fixo e duty de 50%% nao\n");
    printk("sao pulso de servo valido.\n\n");
    printk("LED desta gravacao: " LED_COR " - se a cor da placa nao mudou, o\n");
    printk("upload NAO chegou ao alvo. Ver context/CONTINUAR-AQUI.md\n\n");

    /* SC |= ..., nao uma escrita: reinicializar com parametros diferentes
       acumularia bits. Por isso uma vez so, fora do laco (D-02).
       So importa para as fases de PWM; com elas desligadas, e inofensivo. */
    pwm_tpm_Init(TPM0, TPM_PLLFLL, MOD_50HZ, TPM_CLK, PS_128, EDGE_PWM);

    if (!fase1_preparar()) {
        printk("ERRO: sem GPIO, o teste nao tem o que medir. Parando.\n");
        return;
    }

    /* Repete para sempre: o alvo aqui e um osciloscopio humano com multimetro,
       e ele precisa de tempo. Nao ha fim porque nao ha o que rodar depois -
       ver o cabecalho sobre este arquivo ser andaime. */
    for (;;) {
        if (RODAR_FASE_0_LED) {
            fase0_led_vivo();
        }

        if (RODAR_FASE_1_GPIO) {
            fase1_gpio();
        }

        if (RODAR_FASE_2_DUTY || RODAR_FASE_3_SERVO) {
            fases_pwm();

            /* Sai dos 180 graus em que a fase 3 termina - ver CNV_REPOUSO. */
            pwm_tpm_CnV(TPM0, CANAL_HORIZONTAL, CNV_REPOUSO);
            pwm_tpm_CnV(TPM0, CANAL_VERTICAL,   CNV_REPOUSO);

            /* O PWM deixou os pinos no mux 4; devolve os dois ao GPIO para a
               proxima volta da fase 1 medir o que ela promete medir. */
            fase1_preparar();
        }
    }
}
