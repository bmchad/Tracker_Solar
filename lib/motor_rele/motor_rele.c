#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <motor_rele.h>   /* o proprio header: confere as assinaturas */

/* Tempo para um rele terminar de comutar. Rele comum de modulo: fecha em
   <= 10 ms e abre em <= 5 ms (folha de dados). O dobro, de folga. */
#define RELE_TEMPO_COMUTACAO_MS   20

/* Quanto o motor leva para PARAR depois que R1 abre. ⚠️ A MEDIR na bancada
   (duvidas.md): o esboco usava 100 ms. Religar ao contrario com o motor ainda
   girando puxa corrente perto da de motor travado, entao aqui sobra folga.
   So pesa numa inversao imediata: o main.c ja espera segundos entre
   movimentos, e o boot comeca com o motor parado. */
#define MOTOR_TEMPO_PARADA_MS    300

/* Pinos e polaridade vem do overlay (zephyr/frdm_kl25z.overlay). */
#define NO_USUARIO DT_PATH(zephyr_user)

static const struct gpio_dt_spec liga =
    GPIO_DT_SPEC_GET(NO_USUARIO, liga_gpios);
static const struct gpio_dt_spec sentido =
    GPIO_DT_SPEC_GET(NO_USUARIO, sentido_gpios);

static bool pronto;
static bool ligado;                               /* R1 fechado */
static motor_sentido_t sentido_atual = MOTOR_SENTIDO_A;

/* Quando R1 abriu pela ultima vez. Comeca "ha MOTOR_TEMPO_PARADA_MS": no boot
   o motor ja esta parado, e o primeiro ligar nao precisa esperar. */
static int64_t t_abriu_ms = -MOTOR_TEMPO_PARADA_MS;

static void esperar_motor_parar(void)
{
    int64_t passou = k_uptime_get() - t_abriu_ms;

    if (passou < MOTOR_TEMPO_PARADA_MS) {
        k_msleep((int32_t)(MOTOR_TEMPO_PARADA_MS - passou));
    }
}

int motor_rele_inicializar(void)
{
    pronto = false;
    ligado = false;
    sentido_atual = MOTOR_SENTIDO_A;

    if (!gpio_is_ready_dt(&liga) || !gpio_is_ready_dt(&sentido)) {
        printk("Motor: GPIO dos reles nao esta pronto\n");
        return -ENODEV;
    }

    /* R1 primeiro: com ele aberto, nada que aconteca em SENTIDO move o motor.
       INACTIVE e nivel LOGICO 0 = rele solto; com o GPIO_ACTIVE_LOW do overlay
       o pino vai a 3,3 V. Antes desta linha o pino esta desabilitado (alta
       impedancia) e a entrada do modulo fica sem GND: rele solto tambem. */
    int err = gpio_pin_configure_dt(&liga, GPIO_OUTPUT_INACTIVE);
    if (err < 0) {
        printk("Motor: falha ao configurar o pino LIGA (%d)\n", err);
        return err;
    }

    err = gpio_pin_configure_dt(&sentido, GPIO_OUTPUT_INACTIVE);
    if (err < 0) {
        printk("Motor: falha ao configurar o pino SENTIDO (%d)\n", err);
        return err;
    }

    pronto = true;
    return 0;
}

int motor_rele_desligar(void)
{
    if (!pronto) {
        return -ENODEV;
    }

    /* 1. Abre R1. E o UNICO passo que corta corrente. Escreve mesmo se ja
          estiver aberto: e barato e nao depende do estado guardado. */
    int err = gpio_pin_set_dt(&liga, 0);
    if (err < 0) {
        /* Sem certeza de que R1 abriu, mexer em SENTIDO poderia inverter o
           motor energizado. Para aqui e deixa quem chamou tratar a falha. */
        printk("Motor: falha ao abrir R1 (%d)\n", err);
        return err;
    }

    if (ligado) {
        ligado = false;
        t_abriu_ms = k_uptime_get();
    }

    if (sentido_atual == MOTOR_SENTIDO_A) {
        return 0;                   /* SENTIDO ja esta solto */
    }

    /* 2. Espera R1 abrir de fato antes de mexer em R2/R3. */
    k_msleep(RELE_TEMPO_COMUTACAO_MS);

    /* 3. Solta SENTIDO. Com R1 aberto, R2/R3 comutam sem corrente. */
    err = gpio_pin_set_dt(&sentido, 0);
    if (err < 0) {
        printk("Motor: falha ao soltar SENTIDO (%d)\n", err);
        return err;
    }
    sentido_atual = MOTOR_SENTIDO_A;
    return 0;
}

int motor_rele_ligar(motor_sentido_t s)
{
    if (!pronto) {
        return -ENODEV;
    }
    if (s != MOTOR_SENTIDO_A && s != MOTOR_SENTIDO_B) {
        return -EINVAL;
    }
    if (ligado && s == sentido_atual) {
        return 0;
    }

    int err;

    /* 2. Inverter com o motor ligado: abre R1 antes de qualquer outra coisa. */
    if (ligado) {
        err = motor_rele_desligar();
        if (err < 0) {
            return err;
        }
    }

    /* 3. Motor parado antes de fechar R1 de novo. */
    esperar_motor_parar();

    /* 4. SENTIDO no estado pedido, ainda com R1 aberto. */
    if (s != sentido_atual) {
        err = gpio_pin_set_dt(&sentido, (s == MOTOR_SENTIDO_B) ? 1 : 0);
        if (err < 0) {
            printk("Motor: falha ao trocar SENTIDO (%d)\n", err);
            (void)motor_rele_desligar();
            return err;
        }
        sentido_atual = s;
        k_msleep(RELE_TEMPO_COMUTACAO_MS);
    }

    /* 5. Fecha R1: o motor parte. */
    err = gpio_pin_set_dt(&liga, 1);
    if (err < 0) {
        printk("Motor: falha ao fechar R1 (%d)\n", err);
        (void)motor_rele_desligar();
        return err;
    }
    ligado = true;
    return 0;
}

bool motor_rele_ligado(void)
{
    return ligado;
}
