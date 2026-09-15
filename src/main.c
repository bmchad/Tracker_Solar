/* NOTAS
pwm do horizontal = ptd3
pwm do vertical = ptd0
ldr superior esquerdo no ptb0
ldr inferior esquerdo no ptb1
ldr superior direito no ptb2
ldr inferior direito no ptb3
os quatro ldr sao ptb0..ptb3 = A0..A3, contiguos no header arduino
em angulo_incidencia_solar.c preencha dia mes ano latitude etc
em init_hora.c coloque hora, minuto segundo

ANTES DE LIGAR, POSICIONE MANUALMENTE O TRACKER COM A PLACA
PARALELA AO SOLO E ALINHADA COM O EQUADOR

*/

#include <zephyr/kernel.h>             // Funções básicas do Zephyr (ex: k_msleep, k_thread, etc.)
#include <zephyr/device.h>             // API para obter e utilizar dispositivos do sistema
#include <zephyr/drivers/gpio.h>       // API para controle de pinos de entrada/saída (GPIO)
#include <pwm_z42.h>                // Biblioteca personalizada com funções de controle do TPM (Timer/PWM Module)
#include <ldr.h>
#include <init_hora.h>              // Biblioteca do relógio de software
#include <angulo_incidencia_solar.h> // Biblioteca matemática para Zênite e Azimute
#include <servo_motor.h>
#include <teste_ldr.h>
#include <pisca_led.h>
#include <teste_rot_cont.h>
#include <teste_console.h>

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

    /* ⭐ RECIBO DE GRAVACAO, e e a PRIMEIRA coisa que main() faz de proposito:
       acende o LED na cor desta build e deixa aceso. Se a placa nao mudar de
       cor depois de gravar, o upload NAO chegou ao alvo - troque
       COR_DESTA_GRAVACAO em pisca_led.h a cada build.

       ⚠️ Vem ANTES de teste_console(), que e laco infinito: quando a serial
       esta muda, o LED e a unica prova de que este firmware e o novo. Foi
       exatamente essa duvida que custou o dia 2026-09-14 inteiro. */
    led(COR_DESTA_GRAVACAO);

    teste_console(0); //teste_console(tentaivas), onde tentativa = 0 é infinito

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

    /* ⭐ ORIGEM DA ODOMETRIA. O bloco de NOTAS no topo manda posicionar o
       tracker a mao antes de ligar; e AQUI que esse instante vira o zero.

       ⚠️ A origem precisa ser o CENTRO do curso util, nao um ponto qualquer: a
       janela de limite e simetrica (+-120 no horizontal, +-50 no vertical),
       entao zerar fora do meio faz um dos lados nao caber. */
    servo_zerar_horizontal();
    servo_zerar_vertical();

    /* ⚠️ TESTES DE BANCADA - ligue UM de cada vez.

       teste_rot_cont() e teste_rot_cont_medir_omega() sao lacos INFINITOS e nao
       devolvem o controle; o que vier depois deles nunca roda. As medicoes de
       posicao e o teste de LDR retornam, entao podem ser encadeados.

       Ordem util quando se quer os tres: medir omega primeiro (a odometria
       depende dele), depois posicao, depois LDR. */

    //teste_rot_cont();              /* o servo gira? NAO RETORNA              */
    //teste_rot_cont_medir_omega();  /* quanto vale omega? NAO RETORNA         */

    printk("Iniciando teste de medicao de posicionamento\n");
    teste_medicao_posicao_vertical();
    teste_medicao_posicao_horizontal();

    printk("Iniciando teste de LDR\n");
    teste_ldr();

    /* =====================================================================
       ⛔ LACO DE TRACKING DESATIVADO EM 2026-09-15 - NAO E ABANDONO, E PORTE
          PENDENTE. Ele nao LINKA: chama servo_horizontal_angulo_tempo() e
          servo_vertical_angulo_tempo(), que deixaram de existir quando a camada
          de servo passou a falar direcao + tempo (os servos sao de rotacao
          continua e nao tem posicao).

       ⚠️ NAO escreva um wrapper `servo_*_angulo_tempo` chamando a camada nova
          para "resolver" isto. Seria exatamente a mentira que o refactor
          removeu: prometer angulo a um dispositivo que so entende velocidade.
          Foram dois dias de investigacao ate descobrir que era isso.

       O QUE FALTA, quando for portar:
         - `angulo += correcao` vira `servo_horizontal_direcao_tempo(dir, ms)`;
         - o limite 10..170 sai: quem limita agora e a odometria, com +-120;
         - `calcular_theta()` como POSICAO do vertical continua sendo o D-11, e
           com rotacao continua nem posicao existe para mandar;
         - `ldr_ler('d'/'e')` virou ldr_ler_todos() com quatro quadrantes em
           cruz, e a mistura e soma de pares.
       ===================================================================== */
#if 0
    /* Variaveis do controle antigo: 2 LDRs e servo POSICIONAL. Vivem aqui
       dentro porque so este laco as usa - fora do #if 0 viravam oito avisos de
       variavel nao utilizada. D-35: `direcao` e atribuida e nunca lida. */
    #define LDR_HISTORY_SIZE 5
    int angulo = 0, correcao = 10, theta = 0;
    int direcao = 1;
    int i = 0;
    double valor_ldr_d[LDR_HISTORY_SIZE] = {0};
    double valor_ldr_e[LDR_HISTORY_SIZE] = {0};
    int buffer_cheio = 0;
    int contador_vertical = 0;

    while(1){
        relogio_atualizar(); // Mantém o relógio preciso atualizando as variáveis globais hora_atual, etc.
        
        // Lógica de Dia e Noite
        if (hora_atual >= 18 || hora_atual < 6) {
            printk("Fim de tarde! Painel indo dormir (Hora: %02d:%02d)...\n",
                   hora_atual, minuto_atual);
            k_msleep(5000); // Dorme por 5 segundos antes de conferir o relógio de novo
            continue; // Pula toda a lógica do LDR
        }
           //D-01: erro separado do valor. Antes, uma falha de leitura devolvia
           //-1.0, entrava no historico e VOTAVA - o firmware concluia que o sol
           //estava do lado oposto e movia a placa por causa de um erro de
           //barramento. Agora o ciclo inteiro e descartado.
        int32_t mv_d = 0, mv_e = 0;
        int err_d = ldr_ler('d', &mv_d); // Le o LDR Direito
        int err_e = ldr_ler('e', &mv_e); // Le o LDR Esquerdo

        if (err_d < 0 || err_e < 0) {
            printk("Falha na leitura do LDR (dir=%d esq=%d); ciclo descartado.\n",
                   err_d, err_e);
            k_msleep(1000);
            continue;
        }

        valor_ldr_d[i] = (double)mv_d;
        valor_ldr_e[i] = (double)mv_e;

        i++; 
        if(i == LDR_HISTORY_SIZE) {
            i = 0;
            buffer_cheio = 1; // Temos LDR_HISTORY_SIZE leituras no historico!
        }
        #define certeza_parametro 5
        if (buffer_cheio) {
            int certeza_esquerda = 0;
            int certeza_direita = 0;
               // D-03: agora isto E milivolts de verdade. Antes o valor lido era
               // contagem bruta escalada por um fator arbitrario, e esta "margem
               //de millivolts" era adimensional - nao havia multimetro que a
               //conferisse. O numero em si continua sendo chute ate as medicoes
               //de P-04.
            double margem = 20.0; // Tolerância em mV para ignorar ruído
            
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

//aqui é ctrl c + ctrl v na lógica horizontal, e apagar a lógica com calcular theta

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
#endif  /* laco de tracking desativado - ver o bloco acima */

    return 0;
}
