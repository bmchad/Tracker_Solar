#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <pisca_led.h>

/* ⚠️ Os tres sao ATIVOS EM NIVEL BAIXO, e e por isso que se usam as variantes
   _dt: elas aplicam sozinhas a inversao declarada no devicetree. Escrever no
   pino direto acenderia com 0 e apagaria com 1, ao contrario do que o codigo
   parece dizer.

   A ordem deste vetor E a ordem do enum led_cor_t - os indices designados
   existem para que a correspondencia fique escrita, nao implicita na posicao.
   LED_AMARELO e LED_APAGADO nao entram: nao sao pinos. */
static const struct gpio_dt_spec leds[] = {
    [LED_VERDE]    = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios),
    [LED_AZUL]     = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios),
    [LED_VERMELHO] = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios),
};

BUILD_ASSERT(ARRAY_SIZE(leds) == LED_AMARELO,
             "o vetor de LEDs e as cores de um pino so do enum divergiram");

/* Quais pinos cada cor acende: um bit por indice de leds[]. O amarelo e a
   unica mistura. LED_APAGADO fica de fora da tabela de proposito - e o que o
   faz acender nada, junto com qualquer valor invalido. */
static const uint8_t pinos_da_cor[] = {
    [LED_VERDE]    = BIT(LED_VERDE),
    [LED_AZUL]     = BIT(LED_AZUL),
    [LED_VERMELHO] = BIT(LED_VERMELHO),
    [LED_AMARELO]  = BIT(LED_VERMELHO) | BIT(LED_VERDE),
};

BUILD_ASSERT(ARRAY_SIZE(pinos_da_cor) == LED_APAGADO,
             "falta cor na tabela pinos_da_cor");

/* Configuracao PREGUICOSA, uma vez so.

   Nao ha funcao de init de proposito - a biblioteca tem uma funcao publica so -,
   entao a primeira chamada a led() configura os tres pinos. Configurar a cada
   chamada forcaria os pinos a INACTIVE antes de acender, e isso pisca a cada
   troca de cor: exatamente o que esta API existe para nao fazer.

   ⚠️ Sem lock, pelo mesmo motivo do D-37 em ldr.c: ha uma thread so. Vira
   corrida no dia em que o controle ganhar thread propria. */
static void garantir_configurado(void)
{
    static bool pronto;

    if (pronto) {
        return;
    }

    for (size_t i = 0; i < ARRAY_SIZE(leds); i++) {
        if (!gpio_is_ready_dt(&leds[i])) {
            /* ⭐ Uma vez so, e vale a pena: LED morto EM SILENCIO faz o recibo
               de gravacao MENTIR. Voce olharia a placa sem cor nenhuma e
               concluiria "o upload nao chegou", investigando o gravador em vez
               do LED. led() e chamada dezenas de vezes e nao pode logar sempre,
               mas precisa reclamar uma. */
            printk("[led] ERRO: o LED %u nao esta pronto; o recibo de gravacao "
                   "nao vale nesta placa\n", (unsigned)i);
            continue;
        }

        gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_INACTIVE);
    }

    pronto = true;
}

void led(led_cor_t cor)
{
    garantir_configurado();

    /* Aciona os TRES, sempre. E o que faz a funcao significar "a cor agora e
       esta" em vez de "acenda mais esta": sem apagar os outros dois, duas
       chamadas seguidas somariam cores.

       Fora da tabela - LED_APAGADO e qualquer valor invalido - a mascara e 0
       e cai em "nenhum aceso". Fail-safe de graca. */
    const uint8_t acesos = ((unsigned)cor < ARRAY_SIZE(pinos_da_cor))
                           ? pinos_da_cor[cor] : 0U;

    for (size_t i = 0; i < ARRAY_SIZE(leds); i++) {
        if (gpio_is_ready_dt(&leds[i])) {
            gpio_pin_set_dt(&leds[i], (acesos & BIT(i)) ? 1 : 0);
        }
    }
}
