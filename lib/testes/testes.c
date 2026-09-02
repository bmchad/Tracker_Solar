#include <zephyr/kernel.h>             // Funções básicas do Zephyr (ex: k_msleep, k_thread, etc.)
#include <zephyr/device.h>             // API para obter e utilizar dispositivos do sistema
#include <zephyr/drivers/gpio.h>       // API para controle de pinos de entrada/saída (GPIO)
#include <pwm_z42.h>                // Biblioteca personalizada com funções de controle do TPM (Timer/PWM Module)
#include <ldr.h>
#include <init_hora.h>              // Biblioteca do relógio de software
#include <angulo_incidencia_solar.h>
#include <testes.h>

#define TPM_MODULE 7500
#define SLEEP_TIME_MS 1000
#define INPUT_PORT DT_NODELABEL(gpiod)
#define INPUT_PIN 20
#define NUMERO_DE_TESTES 10

int teste_motores(void){

    // Configura os motores
    pwm_tpm_Init(TPM0, TPM_PLLFLL, TPM_MODULE, TPM_CLK, PS_128, EDGE_PWM);
    pwm_tpm_Ch_Init(TPM0, 3, TPM_PWM_H, GPIOD, 3);
    pwm_tpm_Ch_Init(TPM0, 0, TPM_PWM_H, GPIOD, 0);

    //configura o ldr
    printk("Iniciando Aplicacao de Monitoramento do LDR...\n");

    // Inicializa o hardware do ADC
    if (ldr_inicializar() < 0)  //configura o ldr
    {
        printk("Falha critica na inicializacao do ADC. Parando.\n");
        return -1;
    }

    printk("ADC pronto. Entrando no loop de leitura.\n");

/*TESTE 1, MOTORES OK MAS LDR NÃO*/

    for(int i=0; i<NUMERO_DE_TESTES; i++){
        //teste de motores horizontais
        for(int angulo = 0; angulo <= 180; angulo += 10){
            servo_horizontal_angulo_tempo(angulo, 1);
            printk("Angulo horizontal: %d\n", angulo);
        }
        for(int angulo = 180; angulo >= 0; angulo -= 10){
            servo_horizontal_angulo_tempo(angulo, 1);
            printk("Angulo horizontal: %d\n", angulo);
        }
        //teste de motores verticais
        for(int angulo = 0; angulo <= 180; angulo += 10){
            servo_vertical_angulo_tempo(angulo, 1);
            printk("Angulo vertical: %d\n", angulo);
        }
        for(int angulo = 180; angulo >= 0; angulo -= 10){
            servo_vertical_angulo_tempo(angulo, 1);
            printk("Angulo vertical: %d\n", angulo);
        }
    }       
   return 0;

}

int teste_ldr(){
    // Configura os motores
    pwm_tpm_Init(TPM0, TPM_PLLFLL, TPM_MODULE, TPM_CLK, PS_128, EDGE_PWM);
    pwm_tpm_Ch_Init(TPM0, 3, TPM_PWM_H, GPIOD, 3);
    pwm_tpm_Ch_Init(TPM0, 0, TPM_PWM_H, GPIOD, 0);

    //configura o ldr
    printk("Iniciando Aplicacao de Monitoramento do LDR...\n");

    // Inicializa o hardware do ADC
    if (ldr_inicializar() < 0)  //configura o ldr
    {
        printk("Falha critica na inicializacao do ADC. Parando.\n");
        return -1;
    }

    printk("ADC pronto. Entrando no loop de leitura.\n");

    /*Teste 2: LDR*/
       for(int i=0; i<NUMERO_DE_TESTES; i++){
            printk("LDR direito: %f\n", ldr_ler('d'));
            printk("LDR esquerdo: %f\n", ldr_ler('e'));
            k_msleep(500);
        }

        return 0;

}