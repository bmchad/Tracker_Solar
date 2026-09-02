#include <zephyr/kernel.h>             // Funções básicas do Zephyr (ex: k_msleep, k_thread, etc.)
#include <zephyr/device.h>             // API para obter e utilizar dispositivos do sistema
#include <zephyr/drivers/gpio.h>       // API para controle de pinos de entrada/saída (GPIO)
#include <pwm_z42.h>                // Biblioteca personalizada com funções de controle do TPM (Timer/PWM Module)
#include <ldr.h>
#include <init_hora.h>              // Biblioteca do relógio de software
#include <angulo_incidencia_solar.h> // Biblioteca matemática para Zênite e Azimute

// Para um clock de 48 MHz e prescaler de 128 (PS_128), a frequência do timer é 375 kHz.
// Para um período de 20ms (50Hz), TPM_MODULE (MOD) deve ser 7500.
// 1ms (0 graus) = 375 contagens.
// 2ms (180 graus) = 750 contagens.
#define TPM_MODULE 7500
#define SLEEP_TIME_MS 1000
#define INPUT_PORT DT_NODELABEL(gpiod)
#define INPUT_PIN 20

//aparentemente a cada 180 que peço para esse ir ele só move 90. por isso vou dobrar o angulo no código
void servo_horizontal_angulo_tempo(int ang, int t);

void servo_vertical_angulo_tempo(int ang, int t);