#include <errno.h>
#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/sys/printk.h>
#include <potenciometro.h>   /* o proprio header: confere as assinaturas */

/* ⭐ O canal e configurado AQUI, em tempo de execucao, e nao no overlay.
   O overlay continua com os quatro LDRs do prototipo (e o BUILD_ASSERT de
   lib/ldr, que conta io-channels, continua valendo), e nao ha devicetree a
   regenerar (D-43). PTC2 sai do reset em MUX(0) = analogico, entao nao
   precisa de pinctrl. */
#define POT_CANAL_ADC       11      /* ADC0_SE11 = PTC2 = A4 na FRDM-KL25Z */
#define POT_ADC_BITS        12
#define POT_ADC_MAX         4095

#define POT_N_AMOSTRAS      5       /* mediana de 5: corta pico isolado do motor */

/* Distancia minima dos trilhos (0 e 4095) para a leitura valer. Colado num
   trilho e falha ELETRICA - cursor aberto (com o pull-down de 1 MOhm vai a ~0),
   GND rompido (~4095) - e nao um angulo. E propriedade do circuito, nao da
   montagem, por isso mora aqui e nao na calibracao. */
#define POT_MARGEM_TRILHO   20

#define POT_MAX_SPAN_D10    3600    /* calibracao com mais de 360 graus e erro */

static const struct device *const adc = DEVICE_DT_GET(DT_NODELABEL(adc0));

static const struct adc_channel_cfg cfg_canal = {
    .gain             = ADC_GAIN_1,
    /* D-19: o driver adc_mcux_adc16 ignora este campo; a referencia de
       hardware e VREFH/VREFL = 3,3 V. Fica igual ao overlay por coerencia. */
    .reference        = ADC_REF_EXTERNAL0,
    .acquisition_time = ADC_ACQ_TIME_DEFAULT,
    .channel_id       = POT_CANAL_ADC,
};

static pot_calibracao_t cal;
static bool pronto;

/* Buffer static sem lock, como o de lib/ldr (D-37): vale enquanto houver uma
   thread so lendo. */
static int16_t amostra;

static int ler_uma(int32_t *n)
{
    struct adc_sequence seq = {
        .channels    = BIT(POT_CANAL_ADC),
        .buffer      = &amostra,
        .buffer_size = sizeof(amostra),
        .resolution  = POT_ADC_BITS,
    };

    int err = adc_read(adc, &seq);
    if (err < 0) {
        return err;
    }

    *n = amostra;
    return 0;
}

static bool fora_da_escala(int32_t n)
{
    return n < 0 || n > POT_ADC_MAX;
}

int pot_inicializar(const pot_calibracao_t *c)
{
    pronto = false;

    if (c == NULL || c->n_a == c->n_b
        || fora_da_escala(c->n_a) || fora_da_escala(c->n_b)) {
        printk("Potenciometro: calibracao invalida\n");
        return -EINVAL;
    }

    int32_t span = c->d10_b - c->d10_a;
    if (span > POT_MAX_SPAN_D10 || span < -POT_MAX_SPAN_D10) {
        printk("Potenciometro: calibracao com mais de 360 graus\n");
        return -EINVAL;
    }

    if (!device_is_ready(adc)) {
        printk("Potenciometro: ADC0 nao esta pronto\n");
        return -ENODEV;
    }

    int err = adc_channel_setup(adc, &cfg_canal);
    if (err < 0) {
        printk("Potenciometro: falha ao configurar o canal %d do ADC (%d)\n",
               POT_CANAL_ADC, err);
        return err;
    }

    cal = *c;
    pronto = true;
    return 0;
}

int32_t pot_bruto_para_d10(int32_t n)
{
    /* |n - n_a| <= 4095 e |d10_b - d10_a| <= 3600 (garantido no init):
       o produto fica abaixo de 15 milhoes, longe do limite do int32. */
    return cal.d10_a
         + ((n - cal.n_a) * (cal.d10_b - cal.d10_a)) / (cal.n_b - cal.n_a);
}

int pot_ler(pot_leitura_t *out)
{
    int32_t v[POT_N_AMOSTRAS];

    if (out == NULL) {
        return -EINVAL;
    }
    if (!pronto) {
        return -ENODEV;
    }

    for (int i = 0; i < POT_N_AMOSTRAS; i++) {
        int err = ler_uma(&v[i]);
        if (err < 0) {
            return err;          /* *out intocado */
        }
    }

    /* insercao: 5 elementos, nao vale nada mais esperto */
    for (int i = 1; i < POT_N_AMOSTRAS; i++) {
        int32_t x = v[i];
        int j = i - 1;
        while (j >= 0 && v[j] > x) {
            v[j + 1] = v[j];
            j--;
        }
        v[j + 1] = x;
    }

    out->n     = v[POT_N_AMOSTRAS / 2];
    out->ruido = v[POT_N_AMOSTRAS - 1] - v[0];
    out->d10   = pot_bruto_para_d10(out->n);

    if (out->n < POT_MARGEM_TRILHO || out->n > POT_ADC_MAX - POT_MARGEM_TRILHO) {
        return -ERANGE;
    }

    return 0;
}
