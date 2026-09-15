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

/* ⭐ RITUAL DE MEDICAO DE OMEGA - o numero de que a odometria da camada de servo
   depende. Gira 5 s num sentido, para 8 s para voce marcar e medir, e repete
   nos dois sentidos de cada eixo. Quatro numeros no fim.

   O LED diz o que esta acontecendo, porque a serial pode estar muda:
       LED ACESO   -> GIRANDO (nao meca agora)
       LED APAGADO -> parado: marque a posicao, ou meca o varrido

   ⛔ Usa CnV CRU, de proposito. Medir omega NAO pode passar pela camada de
   servo: ela satura a duracao pelos limites de curso, e os limites sao
   calculados COM omega. Passar por ela seria medir a propria suposicao. */
void teste_rot_cont_medir_omega(void);

#endif /* TESTE_ROT_CONT_H */
