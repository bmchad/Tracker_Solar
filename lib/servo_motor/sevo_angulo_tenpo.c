#include <zephyr/kernel.h>             // Funções básicas do Zephyr (ex: k_msleep, k_thread, etc.)
#include <zephyr/device.h>             // API para obter e utilizar dispositivos do sistema
#include <zephyr/drivers/gpio.h>       // API para controle de pinos de entrada/saída (GPIO)
#include <pwm_z42.h>                // Biblioteca personalizada com funções de controle do TPM (Timer/PWM Module)
#include <ldr.h>
#include <init_hora.h>              // Biblioteca do relógio de software
#include <angulo_incidencia_solar.h> // Biblioteca matemática para Zênite e Azimute
#include <servo_angulo_tempo.h>

// Para um clock de 48 MHz e prescaler de 128 (PS_128), a frequência do timer é 375 kHz.
// Para um período de 20ms (50Hz), TPM_MODULE (MOD) deve ser 7500.
// 1ms (0 graus) = 375 contagens.
// 2ms (180 graus) = 750 contagens.
#define TPM_MODULE 7500
#define SLEEP_TIME_MS 1000
#define INPUT_PORT DT_NODELABEL(gpiod)
#define INPUT_PIN 20

/* D-05: SATURAR, nunca envolver.
   Antes era `while(ang>180) ang-=180;` / `while(ang<0) ang+=180;`, o que fazia
   185 graus virarem 5 e -10 virarem 170: uma ordem que deveria parar no fim de
   curso mandava a placa para o extremo OPOSTO, com forca e sem aviso. Um
   controle incremental pede angulo fora de faixa naturalmente, na borda da
   correcao, e a resposta certa a "va para 185" e "vou para 180 e paro".
   Combinado com o D-04 (NaN chegando como angulo) o modo de falha era mecanico.

   Os limites aqui sao o curso mecanico do proprio servo. O limite de montagem
   do eixo vertical continua em aberto - ver P-06. */
#define SERVO_ANG_MIN 0
#define SERVO_ANG_MAX 180

static int servo_saturar(int ang){
    if(ang > SERVO_ANG_MAX){ return SERVO_ANG_MAX; }
    if(ang < SERVO_ANG_MIN){ return SERVO_ANG_MIN; }
    return ang;
}

//aparentemente a cada 180 que peço para esse ir ele só move 90. por isso vou dobrar o angulo no código
void servo_horizontal_angulo_tempo(int ang, int t){
    ang = servo_saturar(ang);
    // 375 é o offset para 1ms (0 graus), (750-375) = variação para até 2ms (180 graus)
    pwm_tpm_CnV(TPM0, 3, (375 +(ang * 375 / 180))); //1a correcao: aqui multiplico por 2 para tentar corrigir e multiplico por 140/180 para aplicar uma correção empírica -> 2*140/180 = 1,555 //segunda correcao: multiplico denovo por 140/180 -> 1,21
    k_msleep((int)(1000 * t));
}

void servo_vertical_angulo_tempo(int ang, int t){
    ang = servo_saturar(ang);   /* D-05 */
    // 375 é o offset para 1ms (0 graus), (750-375) = variação para até 2ms (180 graus)
    pwm_tpm_CnV(TPM0, 0, (375 + (ang * 375 / 180))); //1a correção: aqui multiplico por 2 para tentar corrigir e multiplico por 140/180 para aplicar uma correção empírica -> 2*140/180 = 1,555 // 2a correcao: multiplico por 170/180 -> 1,555*170/180 = 1,47
    k_msleep((int)(1000 * t));
}