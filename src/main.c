/* NOTAS
pwm do horizontal = ptd3
pwm do vertical = ptd0
ldr da direita no canal ptb2
ldr da esquerda no canal ptb0
ldr de cima no canal ptb1
ldr de baixo no canal ptb3
os quatro ldr sao ptb0..ptb3 = A0..A3, contiguos no header arduino
em angulo_incidencia_solar.c preencha dia mes ano latitude etc
em init_hora.c coloque hora, minuto segundo
*/

#include <zephyr/kernel.h>             // Funções básicas do Zephyr (ex: k_msleep, k_thread, etc.)
#include <zephyr/device.h>             // API para obter e utilizar dispositivos do sistema
#include <zephyr/drivers/gpio.h>       // API para controle de pinos de entrada/saída (GPIO)
#include <pwm_z42.h>                // Biblioteca personalizada com funções de controle do TPM (Timer/PWM Module)
#include <ldr.h>
#include <init_hora.h>              // Biblioteca do relógio de software
#include <angulo_incidencia_solar.h> // Biblioteca matemática para Zênite e Azimute
#include <servo_angulo_tempo.h>
#include <testes.h>

// Para um clock de 48 MHz e prescaler de 128 (PS_128), a frequência do timer é 375 kHz.
// Para um período de 20ms (50Hz), TPM_MODULE (MOD) deve ser 7500.
// 1ms (0 graus) = 375 contagens.
// 2ms (180 graus) = 750 contagens.
#define TPM_MODULE 7500
#define SLEEP_TIME_MS 1000
#define INPUT_PORT DT_NODELABEL(gpiod)
#define INPUT_PIN 20


//função principal
int main(void){

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

    // Inicia o relógio usando os valores que o usuário digitou no init_hora.c
    relogio_init();

    #define LDR_HISTORY_SIZE 5
    int angulo = 0, correcao = 10, theta = 0;
    int direcao = 1;
    int i = 0;
    double valor_ldr_d[LDR_HISTORY_SIZE] = {0};
    double valor_ldr_e[LDR_HISTORY_SIZE] = {0};
    int buffer_cheio = 0;

    // Posiciona o servo na origem inicial
    servo_horizontal_angulo_tempo(angulo, 1);
    servo_vertical_angulo_tempo(angulo, 1);
    int contador_vertical = 0;

    //teste motores
    teste_motores();
    //teste ldr
    teste_ldr();

    printk("Testes terminados, começando o tracker...");

    while(1){
        relogio_atualizar(); // Mantém o relógio preciso atualizando as variáveis globais hora_atual, etc.
        
        // Lógica de Dia e Noite
        if (hora_atual >= 18 || hora_atual < 6) {
            // Modo Noturno: Sem sol, painel descansa para não ficar caçando luz de poste
            if (angulo != 0) {
                printk("Fim de tarde! Painel indo dormir (Hora: %02d:%02d)...\n", hora_atual, minuto_atual);
                angulo = 0;
                servo_horizontal_angulo_tempo(angulo, 2); // Volta para 0 graus devagar
                servo_vertical_angulo_tempo(angulo, 2); // Volta para 0 graus devagar
            }
            k_msleep(5000); // Dorme por 5 segundos antes de conferir o relógio de novo
            continue; // Pula toda a lógica do LDR
        }
        valor_ldr_d[i] = ldr_ler('d'); // Le o LDR Direito
        valor_ldr_e[i] = ldr_ler('e'); // Le o LDR Esquerdo

        i++; 
        if(i == LDR_HISTORY_SIZE) {
            i = 0;
            buffer_cheio = 1; // Temos LDR_HISTORY_SIZE leituras no historico!
        }
        #define certeza_parametro 5
        if (buffer_cheio) {
            int certeza_esquerda = 0;
            int certeza_direita = 0;
            double margem = 20.0; // Tolerância de millivolts para ignorar ruído
            
            // Varredura no histórico para confirmar a decisão (Filtro Anti-Samba Quadrático)
            for(int k = 0; k < LDR_HISTORY_SIZE; k++){
                for(int j = 0; j < LDR_HISTORY_SIZE; j++){
                    // Se a esquerda foi definitivamente MAIOR que a direita + margem, ganha 1 voto
                    if(valor_ldr_e[k] > valor_ldr_d[j] + margem){
                        certeza_esquerda++; // o sol está na esquerda
                    }
                    // Se a direita foi definitivamente MAIOR que a esquerda + margem, ganha 1 voto
                    if(valor_ldr_d[k] > valor_ldr_e[j] + margem){
                        certeza_direita++; // o sol está na direita
                    }
                }
            }

            int deve_mover = 0;

            if (certeza_esquerda >= certeza_parametro) {
                printk("Certeza absoluta: Sol na esquerda! Girando %d graus e indo para %d.\n", correcao, angulo+correcao);
                angulo += correcao; // gira pra esquerda
                deve_mover = 1;
            } else if (certeza_direita >= certeza_parametro) {
                printk("Certeza absoluta: Sol na direita! Girando %d graus e indo para %d.\n", correcao, angulo-correcao);
                angulo -= correcao; // gira pra direita
                deve_mover = 1;
            } else {
                // Não tem certeza (oscilação, nuvem ou luz equilibrada), fica parado!
            }

            // Protecao dos limites do servo horizontal (0 a 180 graus)
            if (angulo >= 170) {
                angulo = 170;
            } else if (angulo <= 10) {
                angulo = 10;
            }

            // Se decidiu se mover, aplica o movimento e reseta o buffer
            if (deve_mover && angulo <= 170 && angulo >= 10) {
                servo_horizontal_angulo_tempo(angulo, 2);
            }
            
            // Reseta o historico para comecar a avaliar a nova posicao (ou tentar de novo se ficou parado)
            buffer_cheio = 0;
            i = 0;
        }
        if(contador_vertical == 0){
        theta = calcular_theta((double)(hora_atual + minuto_atual/60.0 + segundo_atual/3600.0), angulo);
        theta = arredondar_para_multiplo_de_5(theta);
        servo_vertical_angulo_tempo((int)theta, 2); // lembrando que o servo não aceita mudanças suaves
        k_msleep(200);
        }
        contador_vertical++; if(contador_vertical == 300){
            contador_vertical = 0;
        }
                k_msleep(1000);
    }
    
    return 0;
}