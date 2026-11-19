/* CONTROLE DO EIXO VERTICAL - tracker solar real, v1.

   Logica explicada:     mega_context/main.md
   Decisoes (ADR):       mega_context/main_adr.md
   Duvidas em aberto:    mega_context/duvidas.md

   O potenciometro mede o angulo da placa (lib/potenciometro). O firmware leva
   a placa ate o ALVO e para, comandando o motor por reles (lib/motor_rele).
   Na v1 o alvo e FIXO; o calculo do angulo solar entra depois.

   LIGACOES (FRDM-KL25Z):
       potenciometro:    pontas em 3V3 e GND, cursor em A4 = PTC2
       modulo de reles:  IN de R1 (LIGA)            em D2  = PTD4
                         IN de R2 e R3 (SENTIDO)    em D11 = PTD2
                         GND do modulo no GND da placa
   ⛔ NUNCA 5 V no potenciometro.

   LED DA PLACA:
       cor do recibo, 2 s   boot: prova que ESTE firmware subiu (pisca_led.h)
       amarelo              modo calibracao: motor travado
       verde                parado
       azul                 motor girando
       vermelho             falha: motor desligado ate apertar reset */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <pisca_led.h>
#include <potenciometro.h>
#include <motor_rele.h>

/* ========================== PARAMETROS DA MONTAGEM ==========================
   Mudam quando o potenciometro e reacoplado ou a mecanica muda. Preencha na
   ordem de main.md, secao "Colocando para rodar". */

/* 1. Calibracao de 2 pontos (angulo.md 3.2), em decimos de grau.
      Enquanto CALIBRACAO_MEDIDA for 0 o motor NAO liga: o firmware so imprime
      N e o angulo (modo calibracao). Com a calibracao nominal abaixo, "45
      graus" e uma posicao qualquer do eixo - mover ate ela nao faz sentido. */
#define CALIBRACAO_MEDIDA   0

static const pot_calibracao_t calibracao = {
    .n_a = 0,    .d10_a = 0,        /* NOMINAL: troque pelos pontos medidos */
    .n_b = 4095, .d10_b = 3000,
};

/* 2. Curso permitido, em decimos de grau. O alvo e limitado a esta faixa.
      Valores da BANCADA (transferidor de 0 a 90 graus, angulo.md secao 8);
      troque pelo curso da estrutura quando a mecanica definir. */
#define ANGULO_MIN_D10      0
#define ANGULO_MAX_D10      900

/* 3. Alvo fixo da v1, em decimos de grau (meio do curso de bancada). */
#define ALVO_D10            450

/* 4. Qual sentido do motor AUMENTA o angulo. So se descobre ligando: se estiver
      errado, a protecao de sentido desliga o motor depois de ~1 grau e manda
      trocar aqui (MOTOR_SENTIDO_A <-> MOTOR_SENTIDO_B). */
#define SENTIDO_AUMENTA     MOTOR_SENTIDO_A

/* ========================== PARAMETROS DO CONTROLE ========================== */

/* Liga o motor so se o erro passar disto. Com a parada NO alvo, isto e a
   histerese: a placa para no alvo e so religa longe dele. Tem de ser maior que
   o quanto ela anda depois de cortar o motor, senao oscila em volta do alvo. */
#define LIMIAR_PARTIDA_D10        10      /* 1,0 grau */

/* Desliga quando faltar isto ou menos para o alvo. 0 = desliga ao chegar.
   Aumente se a placa passar do alvo depois de cortar (inercia, a medir). */
#define ANTECIPACAO_D10           0

/* Espera minima entre o fim de um movimento e o comeco do proximo: deixa a
   placa assentar antes de decidir de novo e poupa R1, o rele que se gasta
   (circuito.md 3.2). */
#define INTERVALO_MOVIMENTOS_MS   5000

/* Leitura rapida com o motor girando (a 24 graus/s, 10 ms = 0,24 grau entre
   leituras, angulo.md 3.5); devagar com ele parado. */
#define PERIODO_MOVENDO_MS        10
#define PERIODO_PARADO_MS         250
#define PERIODO_IMPRESSAO_MS      250

/* ========================== PROTECOES ==========================
   Ainda nao ha fim de curso por hardware (circuito.md, item 7): estas tres
   protecoes sao a unica barreira contra o motor girar ate quebrar algo. Todas
   desligam o motor e TRAVAM o firmware em falha ate o reset. */

/* Sentido errado: o angulo andou isto AO CONTRARIO do pedido desde a partida.
   Acima do ruido de leitura (~0,3 grau, angulo.md 3.6). */
#define SENTIDO_ERRADO_D10        10      /* 1,0 grau */

/* Sem movimento: a cada janela, o angulo tem de avancar pelo menos isto no
   sentido pedido. Pega motor travado, batente, corrente solta e
   potenciometro patinando no eixo. */
#define JANELA_PROGRESSO_MS       1000
#define PROGRESSO_MIN_D10         5       /* 0,5 grau por segundo */

/* Tempo maximo com o motor ligado num movimento so. */
#define TEMPO_MAX_MOVIMENTO_MS    30000

BUILD_ASSERT(ANGULO_MIN_D10 < ANGULO_MAX_D10, "curso invertido");
BUILD_ASSERT(LIMIAR_PARTIDA_D10 > ANTECIPACAO_D10,
             "sem histerese: a placa religaria logo depois de parar");

#define TEMPO_RECIBO_MS           2000

typedef enum {
    PARADO,
    MOVENDO,
} estado_t;

/* Imprime decimos como "12,3" / "-0,5": printk nao tem float aqui. */
static void imprimir_d10(int32_t d10)
{
    if (d10 < 0) {
        printk("-");
        d10 = -d10;
    }
    printk("%d,%d", d10 / 10, d10 % 10);
}

/* Traduz a direcao do angulo (+1 aumenta, -1 diminui) para o estado do
   sinal SENTIDO. E o unico lugar que conhece SENTIDO_AUMENTA. */
static motor_sentido_t sentido_para(int32_t dir)
{
    if (dir > 0) {
        return SENTIDO_AUMENTA;
    }
    return (SENTIDO_AUMENTA == MOTOR_SENTIDO_A) ? MOTOR_SENTIDO_B
                                                : MOTOR_SENTIDO_A;
}

/* Desliga o motor e trava aqui ate o reset, repetindo o motivo na serial.
   err = 0 quando a falha e de uma protecao, e nao de uma funcao. */
static FUNC_NORETURN void falhar(const char *motivo, int err)
{
    int err_motor = motor_rele_desligar();

    led(LED_VERMELHO);

    while (1) {
        printk("FALHA: %s", motivo);
        if (err != 0) {
            printk(" (%d)", err);
        }
        printk(". Motor desligado: corrija e aperte reset.\n");

        /* -ENODEV aqui so quer dizer que os reles nem chegaram a ser
           configurados: o pino esta em alta impedancia e o rele, solto. */
        if (err_motor < 0 && err_motor != -ENODEV) {
            printk("  !!! os reles nao responderam (%d): "
                   "CORTE A ALIMENTACAO DO MOTOR\n", err_motor);
        }
        k_msleep(2000);
    }
}

static void imprimir_linha(const pot_leitura_t *l, int32_t alvo,
                           const char *estado)
{
    printk("angulo = ");
    imprimir_d10(l->d10);
    printk(" | alvo = ");
    imprimir_d10(alvo);
    printk(" | N=%4d ruido %2d | %s\n", l->n, l->ruido, estado);
}

/* CALIBRACAO_MEDIDA = 0: o motor nunca liga. Imprime N e o angulo para o
   protocolo de calibracao (angulo.md secao 8). Nao retorna. */
static FUNC_NORETURN void modo_calibracao(void)
{
    led(LED_AMARELO);
    printk("\nMODO CALIBRACAO (CALIBRACAO_MEDIDA = 0): motor travado.\n"
           "Leve a placa a dois angulos conhecidos, anote N em cada um,\n"
           "preencha 'calibracao' em main.c, ponha CALIBRACAO_MEDIDA em 1 e\n"
           "grave de novo.\n\n");

    while (1) {
        pot_leitura_t l = {0};
        int err = pot_ler(&l);

        if (err == 0 || err == -ERANGE) {
            printk("angulo = ");
            imprimir_d10(l.d10);
            printk(" | N=%4d ruido %2d", l.n, l.ruido);
            if (err == -ERANGE) {
                printk("  <- colado no trilho: ponta do curso ou fio solto?");
            }
            printk("\n");
        } else {
            printk("Erro de leitura do potenciometro (%d)\n", err);
        }

        k_msleep(PERIODO_PARADO_MS);
    }
}

int main(void)
{
    /* Reles PRIMEIRO: antes de qualquer outra coisa o motor fica desligado. */
    int err = motor_rele_inicializar();
    if (err < 0) {
        falhar("os reles nao inicializaram", err);
    }

    printk("\n=== TRACKER: EIXO VERTICAL POR POTENCIOMETRO E RELES (v1) ===\n");

    /* Recibo de gravacao (pisca_led.h): se a cor nao mudar, o upload nao
       chegou a placa. */
    led(COR_DESTA_GRAVACAO);
    k_msleep(TEMPO_RECIBO_MS);

    err = pot_inicializar(&calibracao);
    if (err < 0) {
        falhar("o potenciometro nao inicializou", err);
    }

    if (!CALIBRACAO_MEDIDA) {
        modo_calibracao();
    }

    /* O alvo fixo ja tem de estar no curso, mas o alvo calculado (versao
       futura) passara por aqui tambem. */
    const int32_t alvo = CLAMP(ALVO_D10, ANGULO_MIN_D10, ANGULO_MAX_D10);

    printk("Curso ");
    imprimir_d10(ANGULO_MIN_D10);
    printk(" a ");
    imprimir_d10(ANGULO_MAX_D10);
    printk(" graus | alvo ");
    imprimir_d10(alvo);
    if (alvo != ALVO_D10) {
        printk(" (ALVO_D10 fora do curso: limitado)");
    }
    printk(" | liga se errar mais de ");
    imprimir_d10(LIMIAR_PARTIDA_D10);
    printk(" graus\n\n");

    estado_t estado = PARADO;
    led(LED_VERDE);

    /* Comeca "ha INTERVALO_MOVIMENTOS_MS": no boot pode mover de imediato. */
    int64_t t_fim_movimento = -INTERVALO_MOVIMENTOS_MS;
    int64_t t_inicio = 0;
    int64_t t_janela = 0;
    int64_t t_impressao = -PERIODO_IMPRESSAO_MS;

    int32_t dir = 0;            /* +1: aumentando o angulo; -1: diminuindo */
    int32_t angulo_inicio = 0;  /* angulo quando o motor ligou */
    int32_t angulo_janela = 0;  /* angulo no comeco da janela de progresso */

    while (1) {
        pot_leitura_t l = {0};

        /* Sem leitura confiavel nao ha como saber para onde a placa vai. */
        err = pot_ler(&l);
        if (err == -ERANGE) {
            falhar("potenciometro colado no trilho (fio solto ou fora do curso)",
                   err);
        }
        if (err < 0) {
            falhar("erro de leitura do potenciometro", err);
        }

        int64_t agora = k_uptime_get();

        if (estado == PARADO) {
            int32_t erro = alvo - l.d10;
            bool longe = (erro > LIMIAR_PARTIDA_D10)
                      || (erro < -LIMIAR_PARTIDA_D10);
            bool assentou = (agora - t_fim_movimento) >= INTERVALO_MOVIMENTOS_MS;

            if (longe && assentou) {
                dir = (erro > 0) ? 1 : -1;

                err = motor_rele_ligar(sentido_para(dir));
                if (err < 0) {
                    falhar("nao consegui ligar o motor", err);
                }

                /* ligar() pode ter bloqueado: as janelas contam daqui. */
                agora = k_uptime_get();
                estado = MOVENDO;
                angulo_inicio = l.d10;
                angulo_janela = l.d10;
                t_inicio = agora;
                t_janela = agora;
                led(LED_AZUL);
                printk(">>> motor ligado para %s o angulo\n",
                       (dir > 0) ? "AUMENTAR" : "DIMINUIR");
            }
        } else {
            int32_t falta = (alvo - l.d10) * dir;           /* > 0: nao chegou */
            int32_t andou = (l.d10 - angulo_inicio) * dir;  /* < 0: ao contrario */

            if (falta <= ANTECIPACAO_D10) {
                err = motor_rele_desligar();
                if (err < 0) {
                    falhar("nao consegui desligar o motor", err);
                }
                estado = PARADO;
                t_fim_movimento = k_uptime_get();
                led(LED_VERDE);
                printk(">>> chegou ao alvo: motor desligado\n");
            } else if (andou < -SENTIDO_ERRADO_D10) {
                falhar("SENTIDO ERRADO - o angulo andou ao contrario do pedido. "
                       "Troque SENTIDO_AUMENTA em main.c", 0);
            } else if ((agora - t_inicio) > TEMPO_MAX_MOVIMENTO_MS) {
                falhar("motor ligado por mais que TEMPO_MAX_MOVIMENTO_MS", 0);
            } else if ((agora - t_janela) >= JANELA_PROGRESSO_MS) {
                if ((l.d10 - angulo_janela) * dir < PROGRESSO_MIN_D10) {
                    falhar("motor ligado sem mover a placa - travado, batente, "
                           "corrente solta ou potenciometro patinando?", 0);
                }
                angulo_janela = l.d10;
                t_janela = agora;
            }
        }

        if ((agora - t_impressao) >= PERIODO_IMPRESSAO_MS) {
            imprimir_linha(&l, alvo, (estado == MOVENDO) ? "girando" : "parado");
            t_impressao = agora;
        }

        k_msleep((estado == MOVENDO) ? PERIODO_MOVENDO_MS : PERIODO_PARADO_MS);
    }

    return 0;
}
