#ifndef TESTE_ROT_CONT_H
#define TESTE_ROT_CONT_H

/* Testes de bancada para servo de ROTACAO CONTINUA.

   ⚠️ Estes servos NAO tem posicao. O pulso comanda VELOCIDADE e SENTIDO:

       1,0 ms  -> velocidade maxima num sentido
       1,5 ms  -> PARADO (neutro)
       2,0 ms  -> velocidade maxima no sentido oposto

   Descoberto em 2026-09-14, girando o braco com a mao: nao ha batente nenhum.
   E a explicacao de por que o servo "nao obedecia" - o codigo mandava 0 grau
   (= 1,0 ms), e para ele isso significa "gire a toda velocidade".

   ⭐ OS TRES TESTES DAQUI SE DISTINGUEM POR UMA COISA SO: quem passa pela
   camada de servo e quem nao passa.

     teste_rot_cont()          CnV cru     o servo gira nos dois sentidos?
     teste_rot_cont_medir_omega()  CnV cru     quanto vale omega?
     teste_medicao_posicao_*() PELA CAMADA a odometria e o limite funcionam?

   ⛔ E a distincao NAO e estilo. medir_omega TEM de usar CnV cru: a camada
   satura a duracao usando o proprio omega, entao medir por ela seria medir a
   suposicao em vez do servo. E medicao_posicao TEM de usar a camada, porque e
   exatamente ela que esta sob teste. */

/* ⚠️ CONVENCAO DO `n` NOS TRES TESTES CONTAVEIS DO PROJETO (os dois daqui e o
   teste_ldr): e o numero de repeticoes, e **0 significa INFINITO**, nao "nenhuma".
   Veio do teste_console(), que ja era assim. Um `0` que para num teste e roda
   para sempre noutro seria a pior armadilha possivel numa API de mesma cara. */

/* Gira devagar num sentido por 1 s, para 1 s, gira no outro, para.
   O mais cru dos tres: responde "o servo se mexe?" e nada mais.
   ⏱ ~4 s por repeticao. */
void teste_rot_cont(int n);

/* ⭐ RITUAL DE MEDICAO DE OMEGA - o numero de que a odometria da camada de servo
   depende. Gira 5 s num sentido, para 8 s para voce marcar e medir, e repete
   nos dois sentidos de cada eixo. Quatro numeros no fim.

   ⏱ ~52 s por repeticao, e o teste mais caro do projeto.
   ⚠️ UMA passada basta: ele e um ritual HUMANO, e repetir so repete a mesma
   medida. `n = 1` e quase sempre a resposta certa.

   O LED diz o que esta acontecendo, porque a serial pode estar muda:
       LED ACESO   -> GIRANDO (nao meca agora)
       LED APAGADO -> parado: marque a posicao, ou meca o varrido

   ⛔ Gira por CnV cru, sem limite de curso e sem tocar em theta. Depois dele a
   odometria NAO descreve mais a posicao fisica - quem for rodar o tracker em
   seguida tem de chamar servo_zerar_*() DEPOIS, nunca antes. */
void teste_rot_cont_medir_omega(int n);

/* Varre o eixo pela CAMADA DE SERVO: 0 -> limite superior -> 0 -> limite
   inferior -> 0, com 1 s parado entre varreduras. Retorna ao fim.

   ⏱ NAO sao infinitos, mas o tempo deles ACOMPANHA O LIMITE DE CURSO: quem para
   a varredura e o limite da camada, nao um contador, entao subir LIMITE_*_D10
   alonga o teste na mesma proporcao. Por isso nao recebem `n` - o fim e o
   limite. A conta e (LIMITE / graus por comando) x 4 varreduras, com cada
   comando valendo POSICAO_PASSO_MS * omega / 1000.

   ⛔ COM OS LIMITES EM +-360 GRAUS, cada eixo varre DUAS VOLTAS COMPLETAS, duas
   vezes. Se houver cabo preso ao painel, ele enrola - o limite de curso era a
   unica coisa impedindo, e POSICAO_MAX_PASSOS e teto de seguranca folgado
   demais para servir de segunda barreira.

   ⚠️ Terminam com theta NO EXTREMO, e zeram theta por dentro ao comecar. Rodar o
   tracker depois sem servo_zerar_*() faz ele recusar metade das correcoes desde
   o primeiro ciclo.

   ⭐ E o unico teste que exercita a ODOMETRIA e o LIMITE DE CURSO. A cada
   comando imprime o theta estimado e o tempo efetivamente aplicado - quando a
   camada devolve MENOS que o pedido, ela saturou no limite; quando devolve 0,
   nao ha mais curso.

   ⚠️ O theta e ESTIMADO por omega * t, nao medido. Se o eixo parar fisicamente
   antes ou depois do que o theta diz, a diferenca e o erro do omega - e e a
   medida de quanto teste_rot_cont_medir_omega() ainda faz falta. */
void teste_medicao_posicao_vertical(void);
void teste_medicao_posicao_horizontal(void);

#endif /* TESTE_ROT_CONT_H */
