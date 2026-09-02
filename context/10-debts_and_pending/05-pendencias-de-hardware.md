# Pendências de hardware

Perguntas em aberto, não defeitos. O contexto de cada uma está em
[../03-hardware_specs/04-alimentacao-e-perguntas-abertas.md](../03-hardware_specs/04-alimentacao-e-perguntas-abertas.md);
aqui é a lista de trabalho — **o que tocar quando a resposta chegar**.

Todo `A DEFINIR` espalhado pelo `context/` tem um `P-nn` aqui.

---

## P-01 — Quais pinos ADC para os dois LDRs novos?

**Sugestão:** PTB1 (`ADC0_SE9`) e PTB3 (`ADC0_SE13`) — vizinhos diretos dos dois já usados,
contíguos no header J10, livres no projeto. **Alternativa:** PTC1 (`SE15`) e PTC2 (`SE11`).

**Quando responder, tocar:**

| Arquivo | O que |
|---|---|
| `zephyr/frdm_kl25z.overlay` | os dois `reg =`, os dois nomes de nó `channel@N`, e os dois números em `io-channels` |
| `context/03-hardware_specs/01-plataforma-e-pinagem.md` | as duas últimas linhas da tabela |

⭐ **Nenhum arquivo `.c` muda** — `lib/ldr/` acessa por índice (`ADC_DT_SPEC_GET_BY_IDX`), não por
número de canal. É por isso que o enum casa com a **posição** na lista e não com o canal do ADC.
Ver [../02-software_specs/01-devicetree-adc-4-canais.md](../02-software_specs/01-devicetree-adc-4-canais.md).

---

## P-02 — De onde sai a alimentação dos servos?

⭐ **A mais importante da lista, e ela é sobre os sensores, não sobre os motores.**

`ADC_REF_INTERNAL` na KL25Z é o próprio VDD. Se os servos saem da placa, cada movimento afunda o
VDD e **as quatro leituras caem juntas** sem a luz ter mudado — ruído correlacionado com o próprio
movimento, indistinguível de um problema de algoritmo no log.

**Como medir:** é o passo 2 do bringup. Com as leituras de LDR imprimindo, girar um servo e olhar se
os quatro valores se mexem.

**Quando responder:**

- se **fonte externa com terra comum** → registrar em `03-hardware_specs/04` e fechar;
- se **alimentado pela placa** → é mudança de hardware, e nenhum ajuste de software a compensa.
  Enquanto não mudar, todo valor de `ZONA_MORTA_MV` está compensando um problema elétrico, e isso
  precisa estar escrito ao lado da constante.

---

## P-03 — O divisor é pull-up ou pull-down, e com que resistor?

⭐ **Define o SINAL do erro.** Se estiver invertido, o tracker foge do sol e para no batente.

**Como medir:** passo 1 do bringup — iluminar um quadrante e ver se o valor sobe (pull-down) ou cai
(pull-up).

**Quando responder, tocar:**

| Arquivo | O que |
|---|---|
| `lib/controle/controle.c` | o sinal das duas subtrações em `controle_erros()` |
| `context/03-hardware_specs/02-sensores-ldr-e-arranjo-optico.md` | a seção "Circuito" |

O código já assume **pull-down** (mais luz = mais tensão) e diz isso no comentário da função de
mistura, justamente para que inverter seja trocar dois sinais num lugar.

---

## P-04 — Qual a faixa real de mV que os LDRs entregam no local?

Quatro medições, todas no passo 1 do bringup, com a placa travada à mão:

| Medir | Calibra |
|---|---|
| sombra total, e o **espalhamento** entre os quatro | o casamento dos sensores (erro constante) |
| no alvo, e quanto **oscila** parado | ⭐ o piso de ruído → `ZONA_MORTA_MV` tem de ficar acima |
| desequilíbrio de ~10° | `GANHO_MV_POR_GRAU` |
| a partir de que ângulo um quadrante satura | a faixa em que o passo proporcional funciona |

**Quando responder, tocar:** as quatro constantes em `lib/controle/controle.c` —
`ZONA_MORTA_MV`, `GANHO_MV_POR_GRAU`, `PASSO_MIN_GRAUS`, `PASSO_MAX_GRAUS` — e a seção de
calibração de
[../02-software_specs/03-controle-4-quadrantes.md](../02-software_specs/03-controle-4-quadrantes.md),
onde os valores atuais estão marcados como chute inicial.

⚠️ **Só é medível porque `lib/ldr/` passa a devolver milivolts de verdade** (D-03). Com a escala
arbitrária de hoje, não há multímetro que confira o número.

---

## P-05 — Qual a altura da aleta do X em relação ao espaçamento dos sensores?

Define ganho e ângulo de captura: aleta alta dá contraste e faixa estreita (o erro **satura** e o
controle só sabe "para que lado", não "quão longe"); aleta baixa dá faixa larga e pouco contraste
perto do alvo, que é onde nasce o zig-zag.

Não é ajustável em software depois de montado — é o que fixa a escala de P-04.

**Quando responder, tocar:**
[../03-hardware_specs/02-sensores-ldr-e-arranjo-optico.md](../03-hardware_specs/02-sensores-ldr-e-arranjo-optico.md),
com a medida e o ângulo de saturação observado.

---

## P-06 — Existe batente mecânico no eixo vertical, e em que ângulos?

O horizontal tem limites (10°–170°, hoje em `src/main.c:133-137`). **O vertical não tem nenhum** — e
isso combina mal com D-04 (NaN chegando como ângulo) e D-05 (wrap em vez de clamp): o modo de falha
é mecânico.

**Quando responder, tocar:** a tabela de limites em `lib/servo/servo.c` (a spec já prevê
`servo_saturar()` com limites por eixo) e
[../03-hardware_specs/03-atuadores-sg90s.md](../03-hardware_specs/03-atuadores-sg90s.md).

Até então o vertical usa um limite conservador, com o comentário registrando que é provisório.

---

## P-07 — Há painel fotovoltaico real, ou só uma chapa?

Um SG90 entrega cerca de 1,8 kgf-cm. Com painel real, o eixo vertical pode não sustentar a placa
fora do centro de massa — e servo que não vence a carga consome corrente sem mover, o que
realimenta **P-02**.

Também muda o que "funcionar" significa: com painel real dá para medir a corrente gerada e validar
o apontamento pelo que ele **produz**, em vez de acreditar no log. Seria o primeiro critério de
sucesso independente do próprio firmware — hoje o tracker é juiz de si mesmo.

---

## P-08 — Qual a faixa real de pulso do SG90S?

Os fatores `1.21` (horizontal) e `1.47` (vertical) são correções empíricas **encadeadas**, nunca
medidas com goniômetro, e **diferentes entre dois servos do mesmo modelo** — o que sugere que parte
deles compensa folga de montagem, não característica elétrica.

**Hipótese a testar:** o SG90 costuma querer **0,5–2,4 ms**, não os 1–2 ms que o código assume. Isso
explicaria naturalmente o *"pedi 180, moveu 90"* registrado em comentário — e nesse caso o certo é
mudar `CNT_PULSO_MIN`/`CNT_PULSO_MAX`, não multiplicar o ângulo.

**Como medir:** varrer `CnV` de 300 a 800 em passos de 10 e anotar com goniômetro onde o servo
começa e para de se mover. São os dois extremos reais.

**Quando responder, tocar:** `CNT_PULSO_MIN`, `CNT_PULSO_MAX` e os dois `FATOR_*` em
`lib/servo/servo.c`, mais
[../03-hardware_specs/03-atuadores-sg90s.md](../03-hardware_specs/03-atuadores-sg90s.md).

⛔ **Não "consertar" os fatores antes de medir.** Eles estão empiricamente ajustados ao hardware
montado; uma fórmula limpa sem medição troca um erro conhecido e compensado por um desconhecido.

---

## P-09 — Para onde aponta o servo horizontal em 0°, e que sentido aumenta o ângulo?

⭐ **É o que liga o ângulo mecânico do servo ao azimute do céu**, e não existe em lugar nenhum do
código hoje — é a raiz da terceira parte de **D-11**, onde `gama_p` recebe 0–180 mecânico enquanto a
fórmula espera azimute na convenção Sul = 0.

Sem essa resposta o feedforward não pode ser implementado: ele calcula "o sol está no azimute
−30°" e não tem como traduzir isso em "vá para 60° de servo".

```c
#define AZIMUTE_DO_SERVO_ZERO  (-90.0)   /* -90 = Leste. Chute; confirmar. */
#define SENTIDO_SERVO          (+1)      /* +1 se aumentar o angulo vai para Oeste */
```

**Chute atual:** servo em 0° apontando para o **Leste**, 180° para o **Oeste** — a montagem natural
para um tracker, porque um dia inteiro cabe no curso. Provavelmente é a que está na bancada.

**Como medir:** passo 2 do bringup, com uma bússola (ou o sol ao meio-dia, que aponta ao Norte no
hemisfério sul). Comandar 0°, anotar para onde a placa olha; comandar 180°, anotar de novo.

⛔ **Se `SENTIDO_SERVO` estiver invertido, o feedforward manda a placa para o lado oposto do sol na
partida** — e, como a malha do LDR não tem sinal na sombra (ver DEC-2), ela fica lá.

**Quando responder, tocar:** as duas constantes em `lib/sol/` ou `src/main.c`, e
[../02-software_specs/04-eixo-vertical-feedforward.md](../02-software_specs/04-eixo-vertical-feedforward.md).
