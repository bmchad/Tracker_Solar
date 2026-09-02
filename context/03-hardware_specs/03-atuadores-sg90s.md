# Atuadores — SG90S

⭐ **Este arquivo é o dono dos fatores de calibração e dos limites de curso.**

Dois **SG90S**, um por eixo, na pinagem de
[01-plataforma-e-pinagem.md](01-plataforma-e-pinagem.md) (PTD3 horizontal, PTD0 vertical). O
upgrade para 4 LDRs **não troca os servos**.

## A anomalia de curso, e o que sabemos dela

O comentário no código-fonte diz, literalmente:

> *"aparentemente a cada 180 que peço para esse ir ele só move 90. por isso vou dobrar o angulo no
> código"*

E a calibração virou uma pilha de correções empíricas encadeadas, registradas em comentário e
diferentes por eixo:

| Eixo | Fator | Histórico declarado no comentário |
|---|---|---|
| Horizontal (PTD3) | **1.21** | ×2 (dobrar), depois ×140/180, depois ×140/180 de novo |
| Vertical (PTD0) | **1.47** | ×2 (dobrar), depois ×140/180, depois ×170/180 |

A expressão em uso hoje, para cada eixo:

```c
CnV = 375 + (FATOR * ang * 375 / 180)
```

⚠️ **Nenhuma medição foi registrada.** Não há registro de ângulo medido com goniômetro contra
ângulo comandado — só o resultado final de tentativa e erro. Isso significa três coisas:

1. Não sabemos se o problema é **curso reduzido** (o servo não alcança 180°) ou **escala errada**
   (a faixa de pulso do SG90S não é 1–2 ms). SG90 na prática costuma querer **0,5–2,4 ms**, não
   1–2 ms — o que explicaria naturalmente "pedi 180 e moveu 90", e nesse caso o fator certo não é
   um multiplicador no ângulo, é **outro par de contagens mínima/máxima de pulso**.
2. Os dois fatores serem **diferentes** (1,21 contra 1,47) para dois servos do mesmo modelo sugere
   que parte deles está compensando **folga mecânica da montagem**, não característica elétrica.
3. Trocar um servo queimado invalida a calibração daquele eixo, sem aviso.

Isso é **P-08**: medir a faixa real de pulso e substituir o fator por constantes de pulso mínimo
e máximo honestas. A spec de servo já prevê essa parametrização — ver
[../02-software_specs/05-camada-servo-e-calibracao.md](../02-software_specs/05-camada-servo-e-calibracao.md).

Até P-08 ser respondida, **os fatores ficam como estão**: eles estão empiricamente ajustados ao
hardware montado, e "consertar" a matemática sem medir troca um erro conhecido por um desconhecido.

## Limites de curso

| Eixo | Limite | De onde vem |
|---|---|---|
| Horizontal | **10° a 170°** | `src/main.c:133-137`, no laço de controle |
| Vertical | **nenhum** | ⚠️ não existe — ver **P-06** |

⛔ **O eixo vertical não tem limite nenhum hoje**, e isso combina mal com dois bugs:

- **D-04**: `acos()` sem clamp de domínio pode devolver NaN, e converter NaN para `int` é
  comportamento indefinido — o servo recebe um número arbitrário;
- **D-05**: a função de servo **envolve em vez de saturar** (`while(ang>180) ang-=180`), então
  pedir 185° entrega 5°, mandando a placa para o extremo **oposto** em vez de parar no limite.

Quem define o limite vertical é o **batente mecânico da montagem**, não o software — daí **P-06**.
Enquanto não houver resposta, a spec de controle adota um envelope conservador em torno da
inclinação ideal calculada, mecanismo descrito em
[../02-software_specs/04-eixo-vertical-feedforward.md](../02-software_specs/04-eixo-vertical-feedforward.md).

Os limites do horizontal também não são arbitrários: 10° e 170° em vez de 0° e 180° dão margem
para o servo não empurrar contra o próprio fim de curso interno, onde ele consome corrente sem
mover — o que é um problema de alimentação, ver
[04-alimentacao-e-perguntas-abertas.md](04-alimentacao-e-perguntas-abertas.md).

## Tempo de movimento

O SG90 leva cerca de **0,1 s por 60°** sem carga. Com a placa montada, mais. O código atual espera
**1 ou 2 segundos inteiros** por comando (`k_msleep(1000 * t)`), o que é folgado de propósito — e
é bloqueante, o que é o bug **D-14**.

Como não há realimentação de posição, essa espera é a **única** garantia de que o servo chegou.
Reduzi-la sem medir troca uma folga segura por uma corrida silenciosa.
