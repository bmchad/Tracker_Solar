#ifndef SERVO_MOTOR_H
#define SERVO_MOTOR_H

#include <stdint.h>

/* ⚠️ O NOME DESTE ARQUIVO MENTE, e e divida conhecida (D-32): nao ha mais
   "angulo" nesta API. Renomear para lib/servo/servo.h esta previsto na spec
   02-software_specs/05-camada-servo-e-calibracao.md e e tarefa a parte.

   ⭐ POR QUE A API MUDOU: os servos sao de ROTACAO CONTINUA (confirmado em
   2026-09-14, girando o braco a mao: nao ha batente nenhum). Para eles o pulso
   comanda VELOCIDADE e SENTIDO, nao posicao:

       1,0 ms -> velocidade maxima, um sentido
       1,5 ms -> PARADO
       2,0 ms -> velocidade maxima, sentido oposto

   `servo_*_angulo_tempo(int ang, int t)` era, portanto, uma assinatura que
   MENTIA: prometia posicao a um dispositivo que so entende velocidade. Comandar
   "0 grau" (= 1,0 ms) no boot mandava o servo girar a toda velocidade - foi o
   que custou dois dias de investigacao. Ver context/CONTINUAR-AQUI.md.

   D-34: este header nao arrasta mais o Zephyr inteiro nem define TPM_MODULE,
   SLEEP_TIME_MS, INPUT_PORT e INPUT_PIN. Uma camada de servo nao precisa saber
   que existe um relogio, muito menos matematica solar. */

typedef enum {
    SERVO_HORARIO = 0,   /* aumenta theta */
    SERVO_ANTIHORARIO    /* diminui theta */
} servo_dir_t;

/* ⭐ MENOR COMANDO QUE A CAMADA ACEITA, em milissegundos.

   Vive no header, e nao no .c, porque QUEM CONTROLA precisa do numero: pedir
   menos que isto e receber 0 de volta, entao o laco de controle checa antes em
   vez de chamar e ser recusado. Repetir o numero la seria o mecanismo classico
   da divergencia - a mesma razao pela qual OMEGA_PASSO em teste_rot_cont.c
   deriva de PASSO_CNV em vez de copia-lo.

   ⚠️ Multiplicado por omega, este numero E o PASSO MINIMO do eixo: nenhuma
   correcao mais fina que ele existe. Com omega de 60 graus/s sao 9 graus, e e
   esse piso que obriga o controle a ter zona morta maior que ele. Ver o .c
   sobre por que comando curto demais mente para a odometria. */
#define SERVO_MS_MIN   100

/* Gira no sentido dado, em VELOCIDADE FIXA, por `ms` milissegundos, e para.
   Bloqueante.

   Devolve os milissegundos EFETIVAMENTE aplicados, que podem ser MENOS que o
   pedido: o limite de curso satura a duracao, no mesmo espirito do D-05 (satura,
   nunca envolve). Devolve 0 quando nao ha curso restante, ou quando o tempo
   pedido fica abaixo de SERVO_MS_MIN - ver o .c sobre por que comando curto
   demais mente para a odometria. */
uint32_t servo_horizontal_direcao_tempo(servo_dir_t direcao, uint32_t ms);
uint32_t servo_vertical_direcao_tempo(servo_dir_t direcao, uint32_t ms);

/* Define "aqui e zero" no eixo, e zera a odometria.

   ⚠️ Os dois eixos sao posicionados A MAO antes de ligar, e a origem tem de ser
   o CENTRO do curso util - a janela de limite e simetrica, entao zerar fora do
   meio faz um dos lados nao caber. Vertical: placa paralela ao solo. */
void servo_zerar_horizontal(void);
void servo_zerar_vertical(void);

/* Theta estimado POR ODOMETRIA (theta += omega * t), em DECIMOS DE GRAU.

   ⛔ E estimativa, nao medida: nao ha encoder. O erro acumula a cada comando e
   nunca e corrigido, porque nada no sistema observa a posicao real. Serve para
   proteger o curso e para log - nunca como se fosse leitura de sensor. */
int32_t servo_theta_horizontal_d10(void);
int32_t servo_theta_vertical_d10(void);

#endif /* SERVO_MOTOR_H */
