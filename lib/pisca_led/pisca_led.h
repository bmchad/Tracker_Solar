#ifndef PISCA_LED_H
#define PISCA_LED_H

/* Os tres LEDs da placa, acionados por COR.

   ⚠️ O numero do alias NAO segue a ordem das cores na FRDM-KL25Z: o verde e o
   led0, nao o led1. Por isso esta enumeracao existe - para ninguem ter de
   lembrar o mapa.

   ⚠️ O AZUL fica em PTD1 = TPM0_CH1, mesma porta e mesmo timer dos pinos de
   servo (PTD3 = CH3, PTD0 = CH0). Como GPIO nao ha conflito, mas quem habilitar
   o canal 1 do TPM0 um dia vai disputar esse pino. Verde (PTB19) e vermelho
   (PTB18) estao no port B, longe de tudo. */
typedef enum {
    LED_VERDE = 0,   /* led0 - PTB19 */
    LED_AZUL,        /* led1 - PTD1  */
    LED_VERMELHO,    /* led2 - PTB18 */
    LED_APAGADO,     /* nao e cor: apaga os tres. Sempre por ultimo. */
} led_cor_t;

/* ⭐ RECIBO DE GRAVACAO: troque ESTA cor a cada build/upload.

   Se a placa NAO mudar de cor depois de gravar, o upload nao chegou ao alvo -
   e foi exatamente assim que se perdeu o dia 2026-09-14 inteiro, medindo
   sintomas de um firmware que nunca subiu. Nenhuma mensagem do gravador prova
   que o alvo foi reprogramado; so algo que mude visivelmente nele prova.

   Uma linha so para editar, e a placa inteira muda de cor: e o que mantem o
   ritual barato o bastante para ser usado em vez de ignorado.
   Ver context/02-software_specs/06-bringup-e-testes.md. */
#define COR_DESTA_GRAVACAO  LED_AZUL

/* Acende a cor e SEGURA - nao pisca, nao bloqueia, nao imprime.

   Aciona os tres pinos em toda chamada: o escolhido aceso, os outros dois
   apagados. Sem isso, trocar de cor deixaria a anterior acesa junto.

   LED_APAGADO (ou qualquer valor invalido) apaga os tres.

   ⭐ Segurar o nivel, e nao piscar, e o que permite usar o LED como CRONOMETRO
   de um protocolo de bancada: em teste_rot_cont_medir_omega() o LED apagado
   significa "marque a posicao agora" por 8 s e o aceso significa "girando" por
   5 s. Uma piscada nao consegue dizer "continue marcando". */
void led(led_cor_t cor);

#endif /* PISCA_LED_H */
