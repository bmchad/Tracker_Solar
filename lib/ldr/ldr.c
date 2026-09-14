#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/sys/printk.h>
#include <ldr.h>        /* o proprio header: e o que faz o compilador conferir
                           as assinaturas daqui contra as declaradas la. Ate
                           2026-09-14 este include faltava, e ninguem conferia. */

/* ADC = conversor analogico-digital. Um canal por LDR, todos no adc0. */

#define ADC_NODE DT_PATH(zephyr_user)

/* ⚠️ CONTRATO, e e o acoplamento mais facil de quebrar neste projeto: a ordem
   deste vetor e a ordem de io-channels no overlay E a ordem do enum ldr_id_t.
   Os indices designados ([LDR_ESQ] = ...) existem para que a correspondencia
   fique escrita, e nao implicita na posicao.
   Ver context/02-software_specs/01-devicetree-adc-4-canais.md. */
static const struct adc_dt_spec canais[LDR_COUNT] = {
    [LDR_ESQ]   = ADC_DT_SPEC_GET_BY_IDX(ADC_NODE, 0),   /* PTB0 = A0 = SE8  */
    [LDR_DIR]   = ADC_DT_SPEC_GET_BY_IDX(ADC_NODE, 1),   /* PTB2 = A2 = SE12 */
    [LDR_CIMA]  = ADC_DT_SPEC_GET_BY_IDX(ADC_NODE, 2),   /* PTB1 = A1 = SE9  */
    [LDR_BAIXO] = ADC_DT_SPEC_GET_BY_IDX(ADC_NODE, 3),   /* PTB3 = A3 = SE13 */
};

/* ⭐ Pega a divergencia de QUANTIDADE em tempo de compilacao: overlay com tres
   canais e codigo esperando quatro deixa de ser descoberto em campo.
   NAO pega a ordem - nada pega, fora o teste do dedo no passo 1 do bringup. */
BUILD_ASSERT(DT_PROP_LEN(ADC_NODE, io_channels) == LDR_COUNT,
             "io-channels do overlay e LDR_COUNT divergiram");

static const char *const nomes[LDR_COUNT] = {
    [LDR_ESQ]   = "esq",
    [LDR_DIR]   = "dir",
    [LDR_CIMA]  = "cima",
    [LDR_BAIXO] = "baixo",
};

/* D-37: buffer static compartilhado por todos os canais, sem lock. Fecha hoje
   porque ha uma thread so e as leituras sao sequenciais; vira corrida no dia em
   que o controle virar thread propria, que e o caminho natural do D-14. */
static int16_t sample_buffer[1];

/* Uma conversao, sem NENHUM log: e o que permite a ldr_ler_todos() encostar as
   quatro leituras no tempo. Quem chama decide o que registrar. */
static int ler_canal(const struct adc_dt_spec *canal, int32_t *mv_out)
{
    int err;

    struct adc_sequence sequence = {
        .buffer = sample_buffer,
        .buffer_size = sizeof(sample_buffer),
    };

    err = adc_sequence_init_dt(canal, &sequence);
    if (err < 0) {
        return err;
    }

    err = adc_read(canal->dev, &sequence);
    if (err < 0) {
        return err;
    }

    /* D-03: conversao de VERDADE para milivolts.
       Antes era `millivolts * 0.889` sobre um int32_t - um fator que o proprio
       comentario chamava de arbitrario, truncado de volta para inteiro, sobre a
       contagem bruta. O valor NAO estava em milivolts, e por isso a `margem` do
       main.c nao era calibravel contra nenhuma medicao de multimetro.
       adc_raw_to_millivolts_dt usa a referencia e o ganho declarados no
       devicetree - ver o aviso do D-19 no overlay sobre zephyr,reference. */
    int32_t valor = sample_buffer[0];
    err = adc_raw_to_millivolts_dt(canal, &valor);
    if (err < 0) {
        return err;
    }

    *mv_out = valor;
    return 0;
}

int ldr_inicializar(void)
{
    for (int i = 0; i < LDR_COUNT; i++) {
        if (!device_is_ready(canais[i].dev)) {
            printk("Erro: ADC do LDR %s nao esta pronto.\n", nomes[i]);
            return -ENODEV;
        }

        int err = adc_channel_setup_dt(&canais[i]);
        if (err < 0) {
            printk("Erro ao configurar o canal ADC do LDR %s (%d)\n",
                   nomes[i], err);
            return err;
        }
    }

    return 0;
}

int ldr_ler(ldr_id_t id, int32_t *mv_out)
{
    if (mv_out == NULL) {
        return -EINVAL;
    }

    /* Comparacao sem sinal: pega id negativo tambem, porque o enum pode ser
       unsigned e `id < 0` viraria sempre falso, com aviso do compilador. */
    if ((unsigned int)id >= LDR_COUNT) {
        printk("LDR invalido (%d). Use LDR_ESQ, LDR_DIR, LDR_CIMA ou LDR_BAIXO.\n",
               (int)id);
        return -EINVAL;
    }

    int err = ler_canal(&canais[id], mv_out);
    if (err < 0) {
        printk("Erro ao ler o LDR %s (%d)\n", nomes[id], err);
    }

    return err;
}

int ldr_ler_todos(int32_t mv[LDR_COUNT])
{
    /* Escreve num temporario e so copia no fim: em falha o vetor do chamador
       fica intocado, em vez de meio preenchido. Vetor meio preenchido e o tipo
       de coisa que vira subtracao entre uma leitura nova e uma velha. */
    int32_t lidos[LDR_COUNT];

    if (mv == NULL) {
        return -EINVAL;
    }

    for (int i = 0; i < LDR_COUNT; i++) {
        int err = ler_canal(&canais[i], &lidos[i]);
        if (err < 0) {
            printk("Erro ao ler o LDR %s (%d); varredura inteira descartada\n",
                   nomes[i], err);
            return err;
        }
    }

    for (int i = 0; i < LDR_COUNT; i++) {
        mv[i] = lidos[i];
    }

    return 0;
}

const char *ldr_nome(ldr_id_t id)
{
    if ((unsigned int)id >= LDR_COUNT) {
        return "?";
    }

    return nomes[id];
}
