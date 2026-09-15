#include <zephyr/kernel.h>             // Funções básicas do Zephyr (ex: k_msleep, k_thread, etc.)
#include <zephyr/device.h>             // API para obter e utilizar dispositivos do sistema
#include <zephyr/drivers/gpio.h>       // API para controle de pinos de entrada/saída (GPIO)
#include <pwm_z42.h>                // Biblioteca personalizada com funções de controle do TPM (Timer/PWM Module)
#include <ldr.h>
#include <init_hora.h>              // Biblioteca do relógio de software
#include <angulo_incidencia_solar.h>
#include <servo_angulo_tempo.h>     // D-07: faltava. As chamadas de servo abaixo
                                    // eram declaracao implicita - o compilador
                                    // assumia `int f()` e parava de conferir os
                                    // argumentos. Removida do C23.
#include <testes.h>

/* D-06: era 10. Dez varreduras identicas nao revelam nada que a primeira nao
   revele, e custavam 10 x 4 varreduras x 19 passos = 760 comandos de servo,
   com 1 s de sono bloqueante cada: 12 min 40 s antes da primeira leitura de
   tracking. Um reset ao meio-dia custava 12 minutos de sol. */
#define NUMERO_DE_TESTES 5

/* Teto de cada eixo na varredura. NAO sao iguais, e a razao e aritmetica:
   o angulo comandado vira pulso por `CnV = 375 + FATOR * ang * 375/180`, e os
   dois eixos tem fatores diferentes.

     horizontal (fator 1,21): 180 graus -> CnV 829 -> 2,21 ms   dentro
     vertical   (fator 1,47): 160 graus -> CnV 865 -> 2,31 ms   dentro
                              180 graus -> CnV 926 -> 2,47 ms   ALEM DO FIM DE CURSO

   ⛔ Por isso o vertical para em 160. A 180 ele empurra o batente interno e
   trava por 1 s em cada passagem, DUAS vezes por ciclo, para sempre - e o MG90S
   e de engrenagem metalica: nada escorrega para aliviar, ele so esquenta.
   Ver D-41 e a tabela em context/03-hardware_specs/03-atuadores-mg90s.md.

   ⚠️ Onde exatamente comeca o estalo e o que o P-08 quer medir - e isso merece
   um teste dedicado e curto, nao uma varredura infinita passando por cima. */
#define ANG_MAX_HORIZONTAL 180
#define ANG_MAX_VERTICAL   160

/* D-02: sem pwm_tpm_Init nem ldr_inicializar aqui. O main() ja fez. */

int teste_motores(void){

    printk("[teste] motores: varredura dos dois eixos "
           "(horizontal ate %d, vertical ate %d)\n",
           ANG_MAX_HORIZONTAL, ANG_MAX_VERTICAL);

    for(int i=0;; i++){
        //teste de motores horizontais
        for(int angulo = 0; angulo <= ANG_MAX_HORIZONTAL; angulo += 10){
            servo_horizontal_angulo_tempo(angulo, 1);
            printk("Angulo horizontal: %d\n", angulo);
        }
        for(int angulo = ANG_MAX_HORIZONTAL; angulo >= 0; angulo -= 10){
            servo_horizontal_angulo_tempo(angulo, 1);
            printk("Angulo horizontal: %d\n", angulo);
        }
        //teste de motores verticais
        for(int angulo = 0; angulo <= ANG_MAX_VERTICAL; angulo += 10){
            servo_vertical_angulo_tempo(angulo, 1);
            printk("Angulo vertical: %d\n", angulo);
        }
        for(int angulo = ANG_MAX_VERTICAL; angulo >= 0; angulo -= 10){
            servo_vertical_angulo_tempo(angulo, 1);
            printk("Angulo vertical: %d\n", angulo);
        }
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
            /* ⚠️ O SINAL destas duas contas e a mistura do controle, e ela e
               dona de context/02-software_specs/03-controle-4-quadrantes.md.
               Aqui e so o mesmo sentido, para o teste do dedo no bringup bater
               com o que o controle vai enxergar. */
            printk("LDR esq=%d dir=%d cima=%d baixo=%d mV | erro_h=%+d erro_v=%+d mV\n",
                   mv[LDR_ESQ], mv[LDR_DIR], mv[LDR_CIMA], mv[LDR_BAIXO],
                   (int)(mv[LDR_ESQ]  - mv[LDR_DIR]),
                   (int)(mv[LDR_CIMA] - mv[LDR_BAIXO]));
        }
        k_msleep(500);
    }

    return 0;

}
