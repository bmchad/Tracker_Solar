# Eixo vertical e o feedforward astronômico

Arquivos: `lib/sol/` (renomeado de `lib/angulo_incidência_solar/`, D-32), e o trecho
`src/main.c:148-156`.

## O bug conceitual, e por que ele é o mais caro do projeto

Hoje o eixo vertical é comandado assim:

```c
theta = calcular_theta((double)(hora_atual + minuto_atual/60.0 + segundo_atual/3600.0), angulo);
theta = arredondar_para_multiplo_de_5(theta);
servo_vertical_angulo_tempo((int)theta, 2);
```

`calcular_theta()` devolve **θ, o ângulo de incidência**: o ângulo entre o raio solar e a **normal
da placa**. θ = 0 significa apontamento perfeito; θ = 90° significa que o sol está de raspão.

⛔ **θ é uma função de custo, não uma posição.** É a medida de *quanto você está errando*. Mandá-lo
para o servo é como corrigir o volante girando-o pelo tamanho do erro em vez de pela direção dele —
a malha não fecha, e não existe ponto fixo em que ela assente.

E há um segundo defeito que sela o caso: `calcular_theta()` calcula θ usando
`static double beta = 30.0` — a inclinação da placa é **fixa em 30°** dentro da função. Então o
número devolvido descreve uma placa que **não é** a que está se movendo. A cada ciclo o firmware
pergunta "quanto uma placa parada a 30° estaria errando?" e usa a resposta como posição da placa
que ele acabou de mover. É **D-11**.

Terceiro detalhe, menor mas na mesma linha: o argumento `gama_p` (azimute da placa) recebe o
**ângulo mecânico do servo**, 0 a 180 — enquanto a fórmula espera azimute na convenção Sul = 0. São
referenciais diferentes, sem nenhuma conversão entre eles.

## A correção: calcular a posição do sol

Um tracker de dois eixos quer a normal da placa apontada para o sol. Isso significa:

| Eixo | Alvo |
|---|---|
| vertical (inclinação β) | **β = 90° − α**, onde α é a **elevação solar** |
| horizontal (azimute γ) | ~~**γ = γ_s**~~ — **não será usado como comando**, ver **DEC-13** |

⚠️ **A linha do horizontal ficou teórica.** Com **DEC-13** o azimute é malha fechada pura: o par
esquerda/direita comanda o eixo sozinho. γ_s continua sendo calculado — ele sai de graça do mesmo
`atan2` — mas serve para log e para decidir dia/noite, nunca para posicionar.

Ou seja: o que faltava não era um ângulo novo, era **a posição do sol** — que a biblioteca já tem
todos os ingredientes para dar (declinação δ e ângulo horário ω já são calculados lá dentro, e
estão corretos).

```c
/* lib/sol/sol.h
   Devolve onde o sol ESTÁ. Azimute na convenção Sul = 0, positivo para Oeste.
   elevacao_out negativa = sol abaixo do horizonte. */
void calcular_posicao_solar(double HL, double *azimute_out, double *elevacao_out);
```

```c
/* Elevação: */
double sen_alpha = sen_phi * sen_delta + cos_phi * cos_delta * cos_omega;
double alpha     = rad2deg(asin(clamp_dominio(sen_alpha)));

/* Azimute, com atan2 — que resolve o quadrante sozinho.
   ⚠️ NÃO usar asin(cos_delta*sen_omega/cos_alpha): perde o quadrante e erra
   por 180 graus na parte da tarde, calado. */
double gama_s = rad2deg(atan2(sen_omega,
                              sen_phi * cos_omega - cos_phi * tan(deg2rad(delta))));
```

⚠️ **`clamp_dominio()` é obrigatório**, e é a correção de **D-04**: `asin`/`acos` fora de
[−1, +1] devolvem **NaN**, e converter NaN para `int` é comportamento **indefinido** em C. Erro de
arredondamento em soma de senos e cossenos chega a passar de 1,0 por uma fração de LSB, e o
resultado é um valor arbitrário chegando ao servo — que, no eixo vertical, só ganhou limite de
curso com a resposta de **P-06** (10°–160°, provisório até P-08). A função clampa em [−1, +1] antes de qualquer `asin`/`acos`, e os dois usos
atuais na linha do `omega_s` e do `cos_theta` também passam a usá-la.

## `calcular_theta()` fica — rebaixada a diagnóstico

Não se apaga: com β e γ **reais** (os ângulos em que a placa está de fato), θ passa a ser o que ela
sempre foi na teoria — **a medida da qualidade do apontamento**. É o número certo para logar, e o
único jeito de responder "o tracker está funcionando?" sem olhar para a placa.

A assinatura ganha os dois ângulos em vez de ler um `static`:

```c
double calcular_theta(double HL, double beta_real, double gama_real);
```

Um θ que cai ao longo de uma manhã é um tracker convergindo. Um θ que sobe com o log dizendo que
moveu é um problema de sinal (a montagem divergindo do pull-down de **P-03**) ou de calibração de
servo (**P-08**).

## Os dois papéis do feedforward

O cálculo **não** comanda o eixo. Ele faz duas coisas que a malha do LDR não sabe fazer:

### 1. Posição de partida

No boot e ao sair do modo noturno, o eixo vertical vai para `β = 90° − α` **antes** de a malha
começar. Hoje ele vai para **0°**, uma posição arbitrária que pode estar a 90° do sol — e, do fundo
do poço, a malha do LDR não tem sinal nenhum para subir: com todos os quadrantes na sombra, os
quatro valores são igualmente baixos, o erro é zero, e a zona morta manda ficar parado. **O tracker
dorme e não acorda.**

Isso é o que o feedforward resolve: ele coloca a placa perto o bastante do sol para que exista
gradiente, e a malha assume dali.

⛔ **No horizontal esse papel ficou vago, e o risco não ficou.** Com **DEC-13** não há posição
calculada de azimute, então o boot é em posição arbitrária no eixo que tem o maior curso — 160° de
incerteza. O substituto acordado, e que não precisa de bússola nem de relógio:

> **varrer 10° → 170° no boot, em passos grossos, e parar no ângulo de maior SOMA dos quatro LDRs.**

A soma é a única grandeza que serve aqui: a **diferença** entre quadrantes é zero quando o sol está
fora do ângulo de captura, mas a soma cresce monotonicamente na direção da luz. Ela usa os quatro
LDRs como um fotômetro, não como um sensor de erro. Sem essa varredura, o "dorme e não acorda" que
DEC-2 evitou no vertical volta inteiro no horizontal.

### 2. Envelope de sanidade

A malha do LDR pode se afastar no máximo `LIMITE_FF_GRAUS` da posição calculada:

```c
#define LIMITE_FF_GRAUS 25   /* quanto a malha pode discordar do céu */
```

É o que impede o painel de perseguir um poste, um reflexo em janela, ou um farol de carro. O
cálculo astronômico não erra por reflexo — ele não olha para nada. Mas erra por relógio (**D-17**,
que volta a uma hora fixa a cada boot), e é por isso que ele **limita** em vez de comandar: o
envelope é largo o suficiente para absorver relógio impreciso e montagem torta, e estreito o
suficiente para que nada fora do céu seja perseguido.

Fora da janela de sol (α < 0), a malha é desligada e a placa recolhe — e a **elevação calculada**
passa a ser o critério de dia e noite, substituindo o `hora_atual >= 18 || hora_atual < 6` cravado
hoje. Um limiar de hora fixo está errado em qualquer época do ano fora do equinócio; a elevação
solar já sabe quando o sol nasce naquela data e latitude.

## O azimute do servo NÃO ganha offset de montagem — DEC-13

Comparar a posição do servo horizontal com o azimute solar exigiria saber **para onde aponta o servo
em 0°**. Esse número nunca esteve no código, era a pergunta **P-09**, e ela foi **encerrada**: o
horizontal fica em malha fechada pura.

```c
/* NÃO EXISTE MAIS, e é decisão: ver DEC-13.
   #define AZIMUTE_DO_SERVO_ZERO (-90.0)   <- fora do projeto

   O que permanece é a convenção de sentido, que o CONTROLE precisa e que não
   depende de referencial nenhum do céu: qual sinal de passo gira para que lado. */
#define SENTIDO_SERVO  (+1)   /* sai do passo 2 do bringup, sem bússola */
```

⚠️ **O que a decisão deixa em aberto é o θ de diagnóstico.** `calcular_theta()` recebe `gama_p`, o
azimute **da placa**, e sem o offset o que se pode passar é o grau mecânico — numa fórmula que
espera a convenção Sul = 0. É a terceira parte de **D-11**, e ela não se fecha aqui. Duas saídas
honestas, e a escolha é de quem implementar:

- logar θ **só com a parcela vertical** honesta, deixando explícito que o azimute não entra;
- ou logar θ inteiro com um comentário de que ele é **indicativo**, não métrica.

⛔ **O que não é aceitável é passar grau mecânico como se fosse azimute e chamar o resultado de
"qualidade do apontamento"** — seria o mesmo erro de referencial de D-11, com nome novo.

## Bug vizinho: o modo noturno mistura os eixos

```c
if (angulo != 0) {
    angulo = 0;
    servo_horizontal_angulo_tempo(angulo, 2);
    servo_vertical_angulo_tempo(angulo, 2);   /* <- angulo é do HORIZONTAL */
}
```

A variável `angulo` é a posição do eixo **horizontal**, e está sendo usada para comandar os dois.
Funciona por acidente (ambos vão para 0), mas a condição de guarda `angulo != 0` consulta o eixo
horizontal para decidir se o **vertical** precisa recolher — então se o horizontal já estiver em 0
e o vertical não, o vertical fica onde está a noite inteira. E `theta` não é reiniciado. É **D-13**.

Depois do upgrade cada eixo tem sua variável, e a posição de descanso é explícita (uma constante
`POUSO_H` / `POUSO_V`), não o 0 implícito de uma variável emprestada.
