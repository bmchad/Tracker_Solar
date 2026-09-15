#ifndef TESTE_PORTAS_H
#define TESTE_PORTAS_H

/* Escada de diagnostico dos pinos de servo (PTD3 horizontal, PTD0 vertical),
   pensada para ser lida com MULTIMETRO - nao precisa de osciloscopio.

   Quatro fases, cada uma eliminando um suspeito diferente:

     0. LED vermelho da placa pisca         -> "o firmware esta rodando?"
     1. PTD3/PTD0 como GPIO, alto e baixo   -> pino, mux de GPIO e o JUMPER
     2. PWM com duty de 0% / 50% / 100%     -> o TPM0 em si
     3. PWM nos duty reais de servo         -> o sinal que o servo consome

   ⚠️ ANDAIME DE BANCADA. Sai do main() quando o diagnostico fechar.

   Repete a escada ESCADAS_ANTES_DE_SAIR vezes (~50 s cada) e RETORNA, porque
   medir com multimetro pede repeticao - dificilmente se le seis tensoes numa
   passada so. Ao sair deixa os dois pinos em 1,0 ms (0 graus), e nao nos 2,0 ms
   em que a ultima fase termina. */
void teste_portas_servo(void);

/* ⭐ RECIBO DE GRAVACAO, isolado para poder ser chamado por qualquer teste.
   Pisca tres vezes rapido, na cor escolhida por LED_DO_TESTE no .c, e retorna
   em ~1,5 s.

   Troque a cor a cada build: se a placa nao mudar de cor, o upload NAO chegou
   ao alvo. Em 2026-09-14 um dia inteiro foi gasto medindo sintomas de firmware
   que nunca subiu, porque nao havia como distinguir "gravou" de "nao gravou".
   Ver context/CONTINUAR-AQUI.md. */
void teste_portas_recibo(void);

/* Liga/desliga o LED do teste. Existe para que um teste de bancada consiga
   dizer "estou girando AGORA" sem depender da serial - que na investigacao de
   2026-09-14 passou dois dias muda. */
void teste_portas_led(int aceso);

#endif /* TESTE_PORTAS_H */
