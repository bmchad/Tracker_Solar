# Atuadores — MG90S

⭐ **Este arquivo é o dono dos fatores de calibração e dos limites de curso.**

Dois **Tower Pro MG90S**, um por eixo, na pinagem de
[01-plataforma-e-pinagem.md](01-plataforma-e-pinagem.md) (PTD3 horizontal, PTD0 vertical).

⚠️ **Os servos foram trocados em 2026-09-14.** Até então eram SG90/SG90S, e todo o `context/` dizia
isso. O que a troca **não** muda é a aritmética ângulo→pulso — 375 contagens = 1 ms, `CnV(180) = 926`
= 2,47 ms com o fator 1,47 — porque ela é do **TPM da placa**, não do servo. O que ela muda é o
consumo, a robustez mecânica, e o que o servo aceita de faixa de pulso.

## O componente

⚠️ **Valores de catálogo, não medidos nesta bancada.** Estão aqui para dar ordem de grandeza; o que
vale quando houver conflito é a medição, e ela é **P-08** (pulso) e **P-02**/**P-07** (corrente).

| Grandeza | MG90S | Nota |
|---|---|---|
| Engrenagem | **metálica** | ⭐ muda o modo de falha: metal não escorrega nem espana no fim de curso. Ele segura e continua puxando corrente |
| Torque | ~1,8 kgf·cm @ 4,8 V · ~2,2 @ 6 V | praticamente o mesmo do SG90 a 4,8 V |
| Velocidade | ~0,1 s por 60° sem carga | mesma premissa que já estava escrita aqui |
| Tensão | **4,8–6,0 V** | 5 V é o meio da faixa — é o que sustenta a escolha de fonte da **DEC-16** |
| Banda morta | ~5 µs | metade da do SG90; pulso mais fino já move |
| Corrente | movimento ~200–300 mA · travamento **da ordem de 1 A** por servo | ⚠️ varia bastante por fonte. É o número que a medição em série substitui |

⛔ **A engrenagem metálica piora o pior caso, não melhora.** Num servo de plástico, empurrar o fim de
curso às vezes termina em dente espanado — falha barulhenta e final. No de metal nada cede: ele
segura a posição contra o batente e continua consumindo corrente de travamento até alguém
desligar.

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
   (a faixa de pulso real não é 1–2 ms). Servos desta classe costumam querer **0,5–2,4 ms**, não
   1–2 ms — o que explicaria naturalmente "pedi 180 e moveu 90", e nesse caso o fator certo não é
   um multiplicador no ângulo, é **outro par de contagens mínima/máxima de pulso**.
2. Os dois fatores serem **diferentes** (1,21 contra 1,47) para dois servos do mesmo modelo sugere
   que parte deles está compensando **folga mecânica da montagem**, não característica elétrica.
3. Trocar um servo queimado invalida a calibração daquele eixo, sem aviso.

⛔ **E foi exatamente isso que aconteceu:** os fatores acima foram levantados nos **SG90** antigos.
Com os MG90S eles não descrevem mais nada medido — são chute herdado de outro componente, até a
nova medição por eixo entrar.

Isso é **P-08**: medir a faixa real de pulso e substituir o fator por constantes de pulso mínimo
e máximo honestas. A spec de servo já prevê essa parametrização — ver
[../02-software_specs/05-camada-servo-e-calibracao.md](../02-software_specs/05-camada-servo-e-calibracao.md).

Até P-08 ser respondida, **os fatores ficam como estão**: eles estão empiricamente ajustados ao
hardware montado, e "consertar" a matemática sem medir troca um erro conhecido por um desconhecido.

## Limites de curso

| Eixo | Limite | De onde vem |
|---|---|---|
| Horizontal | **10° a 170°** | `src/main.c:133-137`, no laço de controle |
| Vertical | **10° a 160°**, provisório | **P-06**, respondida em 2026-09-14 — a conta está abaixo |

⚠️ O vertical passou a ter limite só depois da resposta de **P-06**, e ele é **provisório**. Ficar
sem limite combinava mal com dois bugs, e os dois continuam abertos:

- **D-04**: `acos()` sem clamp de domínio pode devolver NaN, e converter NaN para `int` é
  comportamento indefinido — o servo recebe um número arbitrário;
- **D-05**: a função de servo **envolve em vez de saturar** (`while(ang>180) ang-=180`), então
  pedir 185° entrega 5°, mandando a placa para o extremo **oposto** em vez de parar no limite.

### P-06 respondida: não há batente de montagem — quem para é o próprio servo

**O servo começa a estalar passada meia volta, e esse estalo é o fim de curso interno dele.** O
curso é ±90° em torno do centro — os 180° nominais — e a placa não esbarra em nada da montagem
antes disso. Não existe, portanto, um limite mecânico *externo* a descobrir.

⚠️ **Observado nos SG90 antigos.** Com os MG90S o comportamento deve ser o mesmo — é a mesma classe
de servo —, mas ninguém repetiu a observação depois da troca.

⛔ **Isso NÃO quer dizer que 0°–180° é seguro no código, e a aritmética mostra por quê.** O ângulo
comandado passa pelo fator empírico antes de virar pulso (`CnV = 375 + FATOR × ang × 375 / 180`), e
no vertical o fator é **1,47**:

| ang comandado | `CnV` | pulso | o que acontece |
|---|---|---|---|
| 122° | 750 | 2,00 ms | fim do curso **nominal** de 1–2 ms |
| 160° | 865 | 2,31 ms | o limite adotado: margem antes do estalo |
| 171° | 900 | 2,40 ms | teto prático **suposto** — era o número do SG90 |
| 180° | 926 | 2,47 ms | **além do fim de curso**: o servo empurra contra o batente interno |

⭐ **O estalo relatado é exatamente o que essa conta prevê**, e vira evidência a favor da hipótese
de **P-08**: o servo quer 0,5–2,4 ms, não os 1–2 ms que o código assume, e o fator 1,47 é o remendo
que alcança a faixa maior por fora. Enquanto P-08 não for medida, o vertical satura em **160°**, e o
limite é provisório de propósito — quem fixa o número honesto é a varredura de `CnV`, não mais um
valor escolhido no olho. (O horizontal, limitado a 170°, chega a 2,14 ms com o fator 1,21: está
dentro, e é por isso que ele não estalava.)

⛔ **As duas colunas da direita valem menos desde a troca de servo.** Os 2,40 ms de teto são do
SG90; para o MG90S são **suposição não verificada**, e toda conclusão que dependia desse teto herda
a ressalva — inclusive a regra prática de que `FATOR > 1,4` já pede pulso além do que o servo
aceita.

⚠️ **Servo empurrando contra o próprio batente consome corrente sem mover** — é o mesmo mecanismo
que alimenta **P-02**. O estalo não é só barulho: é carga extra no `P5V_USB`, que também alimenta o
LDO de 3,3 V de onde saem os divisores dos LDRs.

A spec de controle mantém, por cima desse limite, o envelope conservador em torno da inclinação
ideal calculada, mecanismo descrito em
[../02-software_specs/04-eixo-vertical-feedforward.md](../02-software_specs/04-eixo-vertical-feedforward.md).

Os limites do horizontal também não são arbitrários: 10° e 170° em vez de 0° e 180° dão margem
para o servo não empurrar contra o próprio fim de curso interno, onde ele consome corrente sem
mover — o que é um problema de alimentação, ver
[04-alimentacao-e-perguntas-abertas.md](04-alimentacao-e-perguntas-abertas.md).

## Tempo de movimento

O MG90S leva cerca de **0,1 s por 60°** sem carga. Com a placa montada, mais. O código atual espera
**1 ou 2 segundos inteiros** por comando (`k_msleep(1000 * t)`), o que é folgado de propósito — e
é bloqueante, o que é o bug **D-14**.

Como não há realimentação de posição, essa espera é a **única** garantia de que o servo chegou.
Reduzi-la sem medir troca uma folga segura por uma corrida silenciosa.
