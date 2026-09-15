#include <zephyr/kernel.h>
#include <pwm_z42.h>
#include <servo_motor.h>

/* ---------------------------------------------------------------------------
   A base de tempo, derivada do TPM: clock de 375 kHz e MOD = 7500 dao periodo
   de 20 ms (50 Hz), e cada milissegundo de pulso vale 375 contagens.

       1,0 ms = 375  -> velocidade maxima, um sentido
       1,5 ms = 563  -> PARADO (neutro)
       2,0 ms = 750  -> velocidade maxima, sentido oposto

   O dono do numero 7500 e a tabela de PWM em
   context/03-hardware_specs/01-plataforma-e-pinagem.md.
   --------------------------------------------------------------------------- */
#define CNV_NEUTRO         563   /* 1,50 ms                                     */
#define EXCURSAO_MAX       188   /* 0,50 ms: do neutro ate a velocidade maxima  */

/* ⭐ VELOCIDADE FIXA POR EIXO, e isso e decisao de projeto, nao preguica.

   FIXA: com um valor so por eixo, omega e uma constante medida. Se a velocidade
   variasse por comando, cada valor precisaria da propria calibracao - e perto do
   neutro cairia na ZONA MORTA, onde o servo nao anda mas a odometria contaria
   que sim.

   POR EIXO: os dois carregam massas diferentes, e o que tira um da zona morta
   pode ser demais para o outro.

   ⛔ MUDOU A VELOCIDADE? O OMEGA DAQUELE EIXO TEM DE SER MEDIDO DE NOVO.
   Velocidade e omega sao o mesmo fenomeno visto de dois lados. Mexer num sem o
   outro faz a odometria mentir - e a mentira e SILENCIOSA: theta deriva, e o
   limite de curso passa a proteger menos do que promete, sem nenhum sintoma ate
   o painel bater em alguma coisa. Os dois campos ficam lado a lado no eixo_t
   exatamente para que quem mexer num veja o outro. */
#define VELOCIDADE_PCT_H     10
#define VELOCIDADE_PCT_V     10

#define PASSO_CNV(pct)     ((EXCURSAO_MAX * (pct)) / 100)

/* Tensao de alimentacao dos servos, em MILIVOLTS. MEDIDA na bancada em
   2026-09-15: 4,8 V.

   Fica como variavel de proposito: omega e PROPORCIONAL a tensao, entao trocar
   a fonte (4x NiMH a 4,8 V por um carregador a 5 V, por exemplo) muda a
   odometria inteira junto. */
#define V_ALIM_MV   4800

/* ⭐ OMEGA VEM DE UM MODELO, nao mais de um chute:

       omega[graus/s] = 125 * V[volts] * alpha,   com alpha = pct/100

   alpha e exatamente o VELOCIDADE_PCT do eixo sobre 100 - a mesma razao que
   PASSO_CNV(pct)/EXCURSAO_MAX calcula, porque alpha e o desvio do pulso sobre a
   excursao maxima de 0,5 ms.

   Em decimos de grau por segundo, com V em mV, a conta fica exata em inteiros:
       omega_d10 = 10 * 125 * (V_mV/1000) * (pct/100) = V_mV * pct / 80

   Sanidade contra o catalogo: alpha = 1 e V = 4,8 V dao 600 graus/s, que sao os
   0,1 s por 60 graus da folha de dados do MG90S. */
#define OMEGA_D10_S(pct)   ((V_ALIM_MV * (pct)) / 80)

/* ⚠️ MODELADO, NAO MEDIDO - e o modelo nao captura tres coisas reais:

     1. ZONA MORTA. O modelo da omega > 0 para qualquer alpha > 0. O servo real
        NAO anda abaixo de ~45 a ~90 us de desvio (30% sao 150 us: acima, mas
        sem folga grande). Perto do neutro o modelo SUPERESTIMA omega - e
        superestimar faz a odometria contar movimento que nao houve.

     2. ASSIMETRIA. Um omega por eixo assume que os dois sentidos giram igual, e
        servo de rotacao continua quase nunca gira. E o motivo de o ritual de
        medicao pedir QUATRO numeros e nao dois: o erro de assimetria nao se
        cancela na ida e volta, acumula.

     3. Saturacao perto de alpha = 1, e variacao com carga e temperatura.

   ⭐ Ainda assim e MUITO melhor que o placeholder de 30 graus/s que estava aqui:
   aquele errava por 6x, e como ms_que_cabem() DIVIDE o curso restante por omega,
   subestimar omega em 6x fazia a funcao autorizar 6x mais tempo. Os limites nao
   estavam so "sem proteger" - estavam permissivos por um fator de seis.

   Quem fecha isto e teste_rot_cont_medir_omega(). Quando os quatro numeros
   medidos existirem, eles substituem estas quatro linhas. */
#define OMEGA_H_HORARIO_D10_S    OMEGA_D10_S(VELOCIDADE_PCT_H)
#define OMEGA_H_ANTI_D10_S       OMEGA_D10_S(VELOCIDADE_PCT_H)
#define OMEGA_V_HORARIO_D10_S    OMEGA_D10_S(VELOCIDADE_PCT_V)
#define OMEGA_V_ANTI_D10_S       OMEGA_D10_S(VELOCIDADE_PCT_V)

/* A odometria trabalha em decimos de grau. GRAUS() deixa a unidade escrita no
   proprio valor, em vez de num comentario ao lado que envelhece toda vez que o
   numero muda. */
#define GRAUS(g)   ((g) * 10)

/* ⚠️ Limites de curso a partir da origem zerada A MAO, e sao SIMETRICOS: o eixo
   anda +-este valor.

   Os valores antigos (+-120 no horizontal, +-50 no vertical) vieram de
   suposicao, e a bancada os contradisse em 2026-09-15 - o tracker tem
   mobilidade muito maior, e o eixo estava parando num LIMITE FALSO bem antes do
   fim de curso real.

   ⛔ O limite falso continua existindo, so que mais longe: ele nasce da
   odometria acumular erro (theta conta omega*t, e o eixo anda menos que isso
   porque a partida do motor consome parte do comando). Subir o limite devolve
   curso util; nao elimina a drift. */
#define LIMITE_H_D10   GRAUS(360)
#define LIMITE_V_D10   GRAUS(360)

/* ⚠️ Comando curto demais e consumido pela PARTIDA do motor: o eixo mal sai do
   lugar, mas a odometria conta omega * t inteiro. O erro e sempre no mesmo
   sentido, entao acumula em vez de se cancelar. Por isso comando abaixo disto e
   RECUSADO (devolve 0) em vez de executado pela metade.
   Valor a medir: comandar 50, 100, 200 ms e ver de onde em diante o movimento
   e proporcional ao tempo.

   ⭐ O NUMERO mora em servo_motor.h, nao aqui: quem controla precisa dele para
   nao pedir movimento que sera recusado. A explicacao fica deste lado, o valor
   fica no header - um fato, um dono. */

/* O modelo de omega so vale nesta faixa de tensao. Fora dela, a odometria passa
   a extrapolar sem base - e como ela protege o curso, extrapolar em silencio e
   o pior modo de falha possivel. */
BUILD_ASSERT(V_ALIM_MV >= 4800 && V_ALIM_MV <= 6000,
             "V_ALIM_MV fora da faixa em que o modelo de omega vale");

/* ⭐ TODO parametro de eixo mora aqui. O mover() guarda so o ALGORITMO -
   saturacao pelo limite, recusa abaixo da duracao minima, acumulo de theta -,
   e nenhum numero. Foi assim que a velocidade deixou de ser macro de arquivo:
   ela e propriedade do eixo, como o canal e o limite. */
typedef struct {
    uint16_t canal;             /* canal do TPM0                           */
    uint16_t cnv_horario;       /* pulso que gira no sentido horario       */
    uint16_t cnv_anti;          /* pulso que gira no anti-horario          */
    int32_t  theta_d10;         /* odometria, decimos de grau              */
    int32_t  limite_d10;
    uint32_t omega_horario;     /* ⚠️ casado com cnv_horario (d10 por s)   */
    uint32_t omega_anti;        /* ⚠️ casado com cnv_anti                  */
} eixo_t;

/* ⚠️ A CONFIRMAR NA BANCADA, POR EIXO: qual desvio do neutro gira para que lado
   e propriedade da MONTAGEM, nao do software - e os dois eixos podem estar
   montados com sentidos opostos, ja que um gira em azimute e o outro em
   inclinacao. Se um sair trocado, troque o `+` pelo `-` NAQUELE eixo so. */
static eixo_t eixo_h = {
    .canal = 3, .theta_d10 = 0, .limite_d10 = LIMITE_H_D10,
    .cnv_horario = CNV_NEUTRO + PASSO_CNV(VELOCIDADE_PCT_H),
    .cnv_anti    = CNV_NEUTRO - PASSO_CNV(VELOCIDADE_PCT_H),
    .omega_horario = OMEGA_H_HORARIO_D10_S, .omega_anti = OMEGA_H_ANTI_D10_S,
};

static eixo_t eixo_v = {
    .canal = 0, .theta_d10 = 0, .limite_d10 = LIMITE_V_D10,
    .cnv_horario = CNV_NEUTRO + PASSO_CNV(VELOCIDADE_PCT_V),
    .cnv_anti    = CNV_NEUTRO - PASSO_CNV(VELOCIDADE_PCT_V),
    .omega_horario = OMEGA_V_HORARIO_D10_S, .omega_anti = OMEGA_V_ANTI_D10_S,
};

/* Quanto tempo ainda cabe antes de bater no limite, no sentido pedido.
   E aqui que mora o espirito do D-05: satura a DURACAO, nunca envolve nem
   ignora o limite. */
static uint32_t ms_que_cabem(const eixo_t *e, servo_dir_t dir, uint32_t omega)
{
    int32_t restante_d10 = (dir == SERVO_HORARIO)
                         ? (e->limite_d10 - e->theta_d10)
                         : (e->theta_d10 + e->limite_d10);

    if (restante_d10 <= 0) {
        return 0;
    }

    /* restante[d10] / omega[d10/s] * 1000 = ms */
    return ((uint32_t)restante_d10 * 1000U) / omega;
}

static uint32_t mover(eixo_t *e, servo_dir_t dir, uint32_t ms)
{
    uint32_t omega = (dir == SERVO_HORARIO) ? e->omega_horario : e->omega_anti;
    uint32_t teto  = ms_que_cabem(e, dir, omega);

    if (ms > teto) {
        ms = teto;              /* satura no limite de curso */
    }

    /* Abaixo do minimo o movimento nao e proporcional, e a odometria mentiria.
       Recusar e mais honesto que executar mal. */
    if (ms < SERVO_MS_MIN) {
        printk("[servo] canal %u: %u ms abaixo do minimo de %u; recusado\n",
               (unsigned)e->canal, (unsigned)ms, (unsigned)SERVO_MS_MIN);
        return 0;
    }

    pwm_tpm_CnV(TPM0, e->canal,
                (dir == SERVO_HORARIO) ? e->cnv_horario : e->cnv_anti);
    k_msleep((int)ms);
    pwm_tpm_CnV(TPM0, e->canal, CNV_NEUTRO);    /* para */

    int32_t andou_d10 = (int32_t)((omega * ms) / 1000U);
    e->theta_d10 += (dir == SERVO_HORARIO) ? andou_d10 : -andou_d10;

    return ms;
}

uint32_t servo_horizontal_direcao_tempo(servo_dir_t direcao, uint32_t ms)
{
    return mover(&eixo_h, direcao, ms);
}

uint32_t servo_vertical_direcao_tempo(servo_dir_t direcao, uint32_t ms)
{
    return mover(&eixo_v, direcao, ms);
}

void servo_zerar_horizontal(void) { eixo_h.theta_d10 = 0; }
void servo_zerar_vertical(void)   { eixo_v.theta_d10 = 0; }

int32_t servo_theta_horizontal_d10(void) { return eixo_h.theta_d10; }
int32_t servo_theta_vertical_d10(void)   { return eixo_v.theta_d10; }
