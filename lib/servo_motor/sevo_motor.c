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

/* ⭐ VELOCIDADE FIXA, e isso e decisao de projeto, nao preguica: com um valor
   so, omega e uma constante medida. Se a velocidade variasse por comando, cada
   valor precisaria da propria calibracao - e perto do neutro cairia na ZONA
   MORTA, onde o servo nao anda mas a odometria contaria que sim.
*/
#define VELOCIDADE_PCT      10
#define PASSO_CNV          ((EXCURSAO_MAX * VELOCIDADE_PCT) / 100)

/* ⚠️ A CONFIRMAR NA BANCADA: qual desvio do neutro gira para que lado e
   propriedade da MONTAGEM, nao do software. Se os sentidos sairem trocados,
   inverta estas duas linhas - e so isso. */
#define CNV_HORARIO        (CNV_NEUTRO + PASSO_CNV)
#define CNV_ANTIHORARIO    (CNV_NEUTRO - PASSO_CNV)

/* ⛔ OMEGA AINDA NAO FOI MEDIDO. Estes numeros sao PLACEHOLDER, e enquanto
   forem, a odometria e os limites de curso NAO PROTEGEM NADA - eles apenas
   parecem proteger, que e pior.

   Como medir, por eixo E POR SENTIDO (sao quatro numeros, nao dois): comandar
   um sentido por 5 s, marcar antes e depois, medir o varrido com transferidor,
   omega = delta_theta / 5 s.

   ⚠️ Servo de rotacao continua e ASSIMETRICO: o mesmo desvio do neutro quase
   nunca da a mesma velocidade nos dois lados. Um omega so embute erro
   sistematico que se acumula a cada ida e volta. */
#define OMEGA_H_HORARIO_D10_S    300   /* 30,0 graus/s - PLACEHOLDER */
#define OMEGA_H_ANTI_D10_S       300   /* PLACEHOLDER */
#define OMEGA_V_HORARIO_D10_S    300   /* PLACEHOLDER */
#define OMEGA_V_ANTI_D10_S       300   /* PLACEHOLDER */

/* Limites de curso, em decimos de grau, a partir da origem zerada A MAO. */
#define LIMITE_H_D10            1200   /* +-120 graus */
#define LIMITE_V_D10             500   /* +-50 graus  */

/* ⚠️ Comando curto demais e consumido pela PARTIDA do motor: o eixo mal sai do
   lugar, mas a odometria conta omega * t inteiro. O erro e sempre no mesmo
   sentido, entao acumula em vez de se cancelar. Por isso comando abaixo disto e
   RECUSADO (devolve 0) em vez de executado pela metade.
   Valor a medir: comandar 50, 100, 200 ms e ver de onde em diante o movimento
   e proporcional ao tempo. */
#define SERVO_MS_MIN             150

typedef struct {
    uint16_t canal;             /* canal do TPM0 */
    int32_t  theta_d10;         /* odometria, decimos de grau */
    int32_t  limite_d10;
    uint32_t omega_horario;     /* decimos de grau por segundo */
    uint32_t omega_anti;
} eixo_t;

static eixo_t eixo_h = {
    .canal = 3, .theta_d10 = 0, .limite_d10 = LIMITE_H_D10,
    .omega_horario = OMEGA_H_HORARIO_D10_S, .omega_anti = OMEGA_H_ANTI_D10_S,
};

static eixo_t eixo_v = {
    .canal = 0, .theta_d10 = 0, .limite_d10 = LIMITE_V_D10,
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
        return 0;
    }

    pwm_tpm_CnV(TPM0, e->canal,
                (dir == SERVO_HORARIO) ? CNV_HORARIO : CNV_ANTIHORARIO);
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
