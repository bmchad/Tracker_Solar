#ifndef TESTE_LDR_H
#define TESTE_LDR_H

/* Teste de bancada dos quatro LDRs.

   Le os quatro quadrantes em milivolts e imprime, junto, os DOIS ERROS que o
   controle vai enxergar - a mesma soma de pares do arranjo em cruz. Serve para
   duas conferencias distintas, e vale saber qual e qual:

     1. PINAGEM: cobrir UM sensor por vez e ver cair o nome certo. E o unico
        teste da ordem de io-channels contra o enum ldr_id_t - divergencia ali
        nao produz erro de compilacao nenhum;

     2. MISTURA: cobrir OS DOIS DA ESQUERDA e ver erro_h mover; os DOIS DE CIMA
        e ver erro_v. Cobrir um sensor so nao valida a soma de pares.

   ⚠️ NAO inicializa hardware: recebe o ADC ja pronto, porque quem inicializa e
   o main(). E o D-02 - antes cada rotina repetia as chamadas de init, e
   pwm_tpm_Init faz `tpm->SC |= ...`, um OR e nao uma escrita, entao
   reinicializar com parametros diferentes ACUMULA bits em vez de substituir. */
int teste_ldr(void);

#endif /* TESTE_LDR_H */
