#include <zephyr/kernel.h>             // Funções básicas do Zephyr (ex: k_msleep, k_thread, etc.)
#include <zephyr/device.h>             // API para obter e utilizar dispositivos do sistema
#include <zephyr/drivers/gpio.h>       // API para controle de pinos de entrada/saída (GPIO)
#include <pwm_z42.h>                // Biblioteca personalizada com funções de controle do TPM (Timer/PWM Module)
#include <ldr.h>
#include <init_hora.h>              // Biblioteca do relógio de software
#include <angulo_incidencia_solar.h>
#include <servo_motor.h>     // D-07: faltava. As chamadas de servo abaixo
                                    // eram declaracao implicita - o compilador
                                    // assumia `int f()` e parava de conferir os
                                    // argumentos. Removida do C23.
#include <testes.h>

/* D-06: era 10. Dez varreduras identicas nao revelam nada que a primeira nao
   revele, e custavam 10 x 4 varreduras x 19 passos = 760 comandos de servo,
   com 1 s de sono bloqueante cada: 12 min 40 s antes da primeira leitura de
   tracking. Um reset ao meio-dia custava 12 minutos de sol. */
#define NUMERO_DE_TESTES 5

/* ⭐ ESTE TESTE MUDOU DE NATUREZA em 2026-09-15, junto com a camada de servo.

   Antes ele varria ANGULOS (0, 10, 20... 180), o que deixou de fazer sentido:
   os servos sao de ROTACAO CONTINUA e nao tem posicao. O pulso comanda
   velocidade, e a camada de servo agora fala direcao + tempo.

   O que ele exercita agora:
     - que cada sentido move o eixo para o lado esperado;
     - que a ODOMETRIA acumula (theta cresce e decresce como deveria);
     - ⭐ que o LIMITE DE CURSO satura: ao chegar em +-120 (horizontal) ou
       +-50 (vertical), a funcao devolve MENOS tempo que o pedido, e no limite
       devolve 0. E o unico teste que exercita essa protecao.

   ⛔ NAO serve para medir omega. Medir omega tem de ser feito com CnV cru, por
   fora desta camada - ela satura a duracao usando o proprio omega, entao medir
   por aqui seria medir a suposicao. Ver teste_rot_cont_medir_omega(). */
#define PASSO_MS            500   /* duracao de cada comando                  */
#define COMANDOS_POR_LADO    12   /* 12 x 500 ms: o bastante para bater no limite */
#define PAUSA_ENTRE_ETAPAS 1000

static void empurrar(const char *nome, servo_dir_t dir,
                     uint32_t (*mover)(servo_dir_t, uint32_t),
                     int32_t (*theta)(void))
{
    printk("\n[motores] %s: %d comandos de %d ms no mesmo sentido\n",
           nome, COMANDOS_POR_LADO, PASSO_MS);

    for (int i = 0; i < COMANDOS_POR_LADO; i++) {
        uint32_t aplicado = mover(dir, PASSO_MS);

        printk("  pedido=%4d ms  aplicado=%4u ms  theta=%+5d decimos de grau%s\n",
               PASSO_MS, aplicado, theta(),
               (aplicado == 0)          ? "   <- LIMITE: nao ha mais curso"
             : (aplicado < PASSO_MS)    ? "   <- saturou no limite"
                                        : "");

        if (aplicado == 0) {
            break;   /* bateu no limite; insistir so gasta tempo */
        }
    }

    k_msleep(PAUSA_ENTRE_ETAPAS);
}

int teste_motores(void){

    printk("[teste] motores: direcao + tempo, com odometria e limite de curso\n");
    printk("⚠️ Os limites so protegem se OMEGA estiver medido - ver o .c da\n");
    printk("camada de servo. Com omega placeholder, eles apenas parecem proteger.\n");

    /* Origem: os dois eixos sao posicionados A MAO antes de ligar, no CENTRO do
       curso util (vertical: placa paralela ao solo). Zerar aqui e o que faz
       +-120 e +-50 significarem algo fisico. */
    servo_zerar_horizontal();
    servo_zerar_vertical();

    for(int i=0;; i++){
        empurrar("horizontal, sentido horario", SERVO_HORARIO,
                 servo_horizontal_direcao_tempo, servo_theta_horizontal_d10);
        empurrar("horizontal, anti-horario",    SERVO_ANTIHORARIO,
                 servo_horizontal_direcao_tempo, servo_theta_horizontal_d10);

        empurrar("vertical, sentido horario",   SERVO_HORARIO,
                 servo_vertical_direcao_tempo,   servo_theta_vertical_d10);
        empurrar("vertical, anti-horario",      SERVO_ANTIHORARIO,
                 servo_vertical_direcao_tempo,   servo_theta_vertical_d10);
    }

   return 0;

}

int teste_ldr(void){

    printk("[teste] ldr: leitura dos quatro sensores, em mV\n");

    /* D-01: erro separado do valor. Uma leitura que falha aparece como falha,
       nao como um numero baixo plausivel.

       Uma chamada so, e de proposito: ldr_ler_todos() faz as quatro conversoes
       encostadas, sem printk no meio. Quatro ldr_ler() com log entre eles
       mediriam quatro instantes diferentes (a serial a 115200 gasta ~3 ms por
       linha), e a diferenca entre quadrantes viraria em parte temporal. */
    for(int i=0; i<NUMERO_DE_TESTES; i++){
        int32_t mv[LDR_COUNT] = {0};
        int err = ldr_ler_todos(mv);

        if(err < 0){
            printk("LDR: varredura falhou (%d)\n", err);
        } else {
            /* ⭐ ARRANJO EM CRUZ: SOMA DE PARES, nao subtracao de sensores
               individuais. Com a cruz os quadrantes sao diagonais, entao cada
               eixo usa os QUATRO sensores.

               ⚠️ O SINAL destas duas contas e a mistura do controle, e ela e
               dona de context/02-software_specs/03-controle-4-quadrantes.md.
               Aqui e so o mesmo sentido, para o teste do dedo no bringup bater
               com o que o controle vai enxergar.

               ⚠️ O erro tem o DOBRO da magnitude do arranjo em X - sao dois
               sensores somados de cada lado. ZONA_MORTA_MV e GANHO_MV_POR_GRAU
               precisam ser reescalados na mesma proporcao. */
            int32_t esq   = mv[LDR_SUP_ESQ] + mv[LDR_INF_ESQ];
            int32_t dir   = mv[LDR_SUP_DIR] + mv[LDR_INF_DIR];
            int32_t cima  = mv[LDR_SUP_ESQ] + mv[LDR_SUP_DIR];
            int32_t baixo = mv[LDR_INF_ESQ] + mv[LDR_INF_DIR];

            printk("LDR se=%d sd=%d ie=%d id=%d mV | erro_h=%+d erro_v=%+d mV\n",
                   mv[LDR_SUP_ESQ], mv[LDR_SUP_DIR],
                   mv[LDR_INF_ESQ], mv[LDR_INF_DIR],
                   (int)(esq  - dir),
                   (int)(cima - baixo));
        }
        k_msleep(500);
    }

    return 0;

}
