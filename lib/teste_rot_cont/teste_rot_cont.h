#ifndef TESTE_ROT_CONT_H
#define TESTE_ROT_CONT_H

/* Teste para servo de ROTACAO CONTINUA.

   ⚠️ Estes servos NAO tem posicao. O pulso comanda VELOCIDADE e SENTIDO:

       1,0 ms  -> velocidade maxima num sentido
       1,5 ms  -> PARADO (neutro)
       2,0 ms  -> velocidade maxima no sentido oposto

   Descoberto em 2026-09-14, girando o braco com a mao: nao ha batente nenhum.
   E a explicacao de por que o servo "nao obedecia" - o codigo mandava 0 grau
   (= 1,0 ms), e para ele isso significa "gire a toda velocidade".

   Este teste faz, em laco: gira devagar num sentido por 1 s, para por 1 s, gira
   devagar no outro por 1 s, para por 1 s. NAO RETORNA. */
void teste_rot_cont(void);

#endif /* TESTE_ROT_CONT_H */
