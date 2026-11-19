#ifndef MOTOR_RELE_H
#define MOTOR_RELE_H

#include <stdbool.h>

/* Motor de vidro comandado por tres reles - mega_context/circuito.md.

   Dois sinais saem da placa:
     LIGA    -> R1: fecha o caminho do motor (liga / desliga);
     SENTIDO -> R2 e R3 JUNTOS: invertem GND e VCC nos terminais do motor.

   A biblioteca guarda so a SEQUENCIA segura dos reles (circuito.md 3.1):
   R1 e o unico rele que abre ou fecha com corrente passando. R2 e R3 so
   trocam com R1 aberto, entao nunca fazem arco.

   Os pinos e a polaridade (o modulo liga com GND na entrada IN) estao no
   overlay, em zephyr,user. O codigo trabalha com nivel LOGICO: 1 = rele
   ligado, e o GPIO_ACTIVE_LOW do overlay transforma isso em 0 V no pino. */

/* Os dois sentidos do motor, pelo estado do sinal SENTIDO.

   ⚠️ Qual deles SOBE e qual DESCE a placa so se sabe com a estrutura montada
   (duvidas.md). Quem traduz "aumentar o angulo" para A ou B e o main.c. */
typedef enum {
    MOTOR_SENTIDO_A = 0,   /* SENTIDO em repouso: R2 e R3 soltos  */
    MOTOR_SENTIDO_B,       /* SENTIDO acionado:   R2 e R3 ligados */
} motor_sentido_t;

/* Configura os dois pinos com o motor DESLIGADO (R1 e SENTIDO soltos).
   0 em sucesso; -ENODEV se o GPIO nao esta pronto; outro -errno do driver. */
int motor_rele_inicializar(void);

/* Liga o motor no sentido pedido, na ordem segura:
     1. se ja esta ligado no MESMO sentido: nao faz nada;
     2. se esta ligado no outro: desliga primeiro (motor_rele_desligar);
     3. espera o motor parar, contado desde a ultima vez que R1 abriu;
     4. poe SENTIDO no estado pedido e espera R2/R3 assentarem;
     5. fecha R1.
   BLOQUEIA ate ~MOTOR_TEMPO_PARADA_MS + RELE_TEMPO_COMUTACAO_MS (motor_rele.c).
   0 em sucesso; -ENODEV antes de inicializar; -EINVAL com sentido invalido;
   outro -errno do driver - e nesse caso tenta deixar tudo desligado. */
int motor_rele_ligar(motor_sentido_t sentido);

/* Desliga o motor: abre R1, espera ele abrir e solta SENTIDO.
   Soltar SENTIDO com R1 aberto nao gasta contato, e evita deixar as bobinas
   de R2 e R3 energizadas por horas com o motor parado.
   Pode ser chamada a qualquer momento, inclusive antes de inicializar
   (devolve -ENODEV). 0 em sucesso; -errno do driver em falha. */
int motor_rele_desligar(void);

/* true enquanto R1 estiver fechado (motor energizado). */
bool motor_rele_ligado(void);

#endif /* MOTOR_RELE_H */
