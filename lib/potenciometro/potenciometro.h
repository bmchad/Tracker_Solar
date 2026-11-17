#ifndef POTENCIOMETRO_H
#define POTENCIOMETRO_H

#include <stdint.h>

/* Leitura de angulo por potenciometro - projeto real, mega_context/angulo.md.

   Ligacao (FRDM-KL25Z):
       extremidade 1 -> 3V3, extremidade 2 -> GND, cursor -> A4 = PTC2 = ADC0_SE11
   ⛔ Nunca 5 V no potenciometro: o pino do ADC aguenta no maximo 3,3 V.

   A biblioteca guarda so o ALGORITMO (ler, filtrar, converter). Os numeros da
   calibracao sao de quem chama, porque sao da MONTAGEM: mudam a cada vez que o
   potenciometro e reacoplado ao eixo. */

/* Calibracao de 2 pontos (angulo.md 3.2): duas leituras do ADC em dois angulos
   conhecidos, medidos com transferidor ou inclinometro. A reta entre eles
   absorve o angulo eletrico real, o zero do acoplamento e o SENTIDO de montagem
   (se o angulo cresce com N caindo, a reta sai com inclinacao negativa sozinha).

   ⭐ Escolha A e B perto das pontas do curso: quanto mais afastados, menos o
   erro de leitura do transferidor pesa na inclinacao. */
typedef struct {
    int32_t n_a;     /* leitura do ADC (0..4095) com a placa em theta_A */
    int32_t d10_a;   /* theta_A, em DECIMOS de grau (450 = 45,0 graus)  */
    int32_t n_b;     /* leitura do ADC (0..4095) com a placa em theta_B */
    int32_t d10_b;   /* theta_B, em decimos de grau                     */
} pot_calibracao_t;

typedef struct {
    int32_t n;       /* mediana das amostras, em contagens do ADC (0..4095)    */
    int32_t ruido;   /* max - min das amostras: o tamanho do ruido nesta leitura */
    int32_t d10;     /* angulo calibrado, em decimos de grau                    */
} pot_leitura_t;

/* Configura o canal do ADC e guarda uma COPIA da calibracao.
   0 em sucesso. -EINVAL se a calibracao e impossivel (n_a == n_b, leitura fora
   de 0..4095, ou mais de 360 graus entre os pontos); -ENODEV se o ADC nao
   esta pronto; outro -errno se o driver recusar o canal. */
int pot_inicializar(const pot_calibracao_t *cal);

/* Uma leitura completa: mediana de 5 amostras, ruido e angulo.

   Retorno, e o que vale em *out em cada caso (D-01: erro e valor viajam por
   canais separados):
     0        leitura valida, *out completo;
     -ERANGE  o ADC leu, mas N esta colado num TRILHO (perto de 0 ou de 4095):
              cursor aberto, fio solto ou ponta do curso. *out e preenchido
              para diagnostico, mas o ANGULO NAO E CONFIAVEL. Na estrutura, quem
              chama PARA O MOTOR;
     -ENODEV  pot_inicializar() nao foi chamada (ou falhou). *out intocado;
     outro    falha do ADC. *out intocado. */
int pot_ler(pot_leitura_t *out);

/* So a conversao N -> decimos de grau, com a calibracao guardada. Sem ADC:
   serve para conferir a conta a mao. Trunca em direcao a zero (erro <= 0,1 grau). */
int32_t pot_bruto_para_d10(int32_t n);

#endif /* POTENCIOMETRO_H */
