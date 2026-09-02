# Pendências de hardware

Perguntas em aberto, não defeitos. O contexto de cada uma está em
[../03-hardware_specs/04-alimentacao-e-perguntas-abertas.md](../03-hardware_specs/04-alimentacao-e-perguntas-abertas.md);
aqui é a lista de trabalho — **o que tocar quando a resposta chegar**.

Todo `A DEFINIR` espalhado pelo `context/` tem um `P-nn` aqui.

---

## P-01 — Quais pinos ADC para os dois LDRs novos? ✅ RESOLVIDO (2026-09-02)

**Resposta: PTB1 (`ADC0_SE9`) para o LDR de cima, PTB3 (`ADC0_SE13`) para o de baixo.** A tabela de
pinagem é dona do fato:
[../03-hardware_specs/01-plataforma-e-pinagem.md](../03-hardware_specs/01-plataforma-e-pinagem.md).
O critério da escolha está em **DEC-12**.

Conferido nas fontes em disco, não de memória:

| Verificação | Onde |
|---|---|
| `ADC0_SE9_PTB1` e `ADC0_SE13_PTB3` existem no package que a placa usa (MKL25Z128VLK4) | `hal/nxp/dts/nxp/kinetis/MKL25Z128VLK4-pinctrl.h:75,85` |
| `PTB0..PTB3 → A0..A3`, quatro pinos contíguos no header Arduino | `boards/nxp/frdm_kl25z/frdm_kl25z.dts`, nó `arduino_header` |
| Nada na placa disputa PTB1/PTB3 — LED RGB em PTB18/PTB19, I2C0 da placa em PTE24/PTE25 | mesmo dts, e `frdm_kl25z-pinctrl.dtsi` |

⭐ **A verificação rendeu um bônus:** as quatro funções de ADC são `KINETIS_MUX('B',n,0)` — ou seja,
`MUX(0)`. Isso **prova** a premissa que a spec do overlay antes só inferia: analógico é o default de
reset do Kinetis, e é por isso que PTB0 funciona hoje sem estar no grupo `adc0_default`. Ver
[../02-software_specs/01-devicetree-adc-4-canais.md](../02-software_specs/01-devicetree-adc-4-canais.md).

⚠️ **Ressalva que fica aberta como risco, não como pendência:** os quatro pads são compartilhados
com o **TSI0** (`TSI0_CH0/CH6/CH7/CH8`), no mesmo `MUX(0)`. Não há mux que escolha entre ADC e
touch — quem fica com o pad é o bloco analógico habilitado. Hoje o `prj.conf` não habilita TSI;
habilitar touch disputaria estes pinos.

**Recusado:** PTC1 (`SE15`) / PTC2 (`SE11`), que são `A4`/`A5`. Livres e válidos, mas quebram a
contiguidade — ver DEC-12.

---

## P-02 — O acoplamento servo→LDR pela rede de GND é tolerável?

⭐ **A mais importante da lista, e ela é sobre os sensores, não sobre os motores.**

Os servos **ficam no `P5V_USB` da placa por decisão** (**DEC-11**), não por omissão, com as três
mitigações de lá. A pergunta que resta é estreita e tem critério de fechamento **mecânico**:

> Com o `teste_ldr_4()` imprimindo, **`erro_h` ou `erro_v` se movem quando um servo gira?**

- **Não se movem** → fecha P-02, sem fonte externa, com a medição anexada aqui.
- **Se movem** → a fonte externa entra por DEC-11, com evidência em vez de precaução.

### O que já se sabe, e é bom saber

⭐ **A medição é ratiométrica.** O `ADC_REF_INTERNAL` da KL25Z é o par VREFH/VREFL, que na placa vai
ao mesmo rail de 3,3 V que excita os divisores. Então:

```
código = (Vdivisor / Vref) × 4095
       = (3,3 × R/(R+R_ldr)) / 3,3 × 4095
       = R/(R+R_ldr) × 4095          ← o 3,3 V desaparece
```

O 3,3 V **se cancela na razão**: o código bruto é imune, em primeira ordem, a queda do rail. Não é
só "tende a cancelar na subtração" — ele nem se move. Somado a isso, o servo no 5 V e o divisor no
3,3 V já separam o lado da alimentação: a corrente de servo **não circula no rail de 3,3 V**.

### O que não cancela, e é por isso que a pendência continua aberta

1. ⭐ **A parcela diferencial da queda IR no GND** — o único condutor compartilhado. Ver **P-10**.
   É o mecanismo principal, e nenhuma das defesas do algoritmo o cobre.
2. **O transiente.** VDDA/VREFH é ligado ao 3,3 V por um filtro (bead + capacitores) na Freedom,
   então numa borda rápida ele **não acompanha** o VDD e a razão quebra justo no arranque do servo.
   ⚠️ Não conferido no esquemático da revisão de placa — é conhecimento geral de Freedom.
3. **O valor em mV convertido**, que usa o 3,3 V **nominal** do devicetree, não o real. Fator comum
   aos quatro, então não muda o sinal do erro; muda a escala em que a zona morta é comparada.
4. **Reset do MCU no pico.** `P5V_USB` é upstream do LDO de 3,3 V; 1 A numa porta de 500 mA pode
   derrubar o MCU (**D-17** → relógio em 15:00, **D-06** → 12 min de varredura) ou o OpenSDA.
   Nenhuma razão de tensões cancela um reboot.

**Quando fechar, tocar:** esta seção com a medição, e `03-hardware_specs/04`.

---

## P-10 — Quanta queda IR **diferencial** o trilho de GND introduz?

⛔ **O mecanismo que o cancelamento por subtração não cobre**, e a razão de P-02 seguir aberta.

O cancelamento exige que a perturbação seja **igual** nos dois sensores comparados. Mas a
perturbação é queda IR **ao longo** de um condutor — e isso é, por definição, um **gradiente**: cada
ponto do trilho está num potencial diferente. Não existe "deslocamento uniforme de terra" saindo de
resistência distribuída conduzindo corrente; o mecanismo que cria o distúrbio é o mesmo que garante
que parte dele seja **diferencial**.

Os quatro LDRs estão em quatro pontos físicos distintos do trilho:

> 1 A de pico do servo × ~50 mΩ entre dois LDRs = **50 mV diferenciais**, contra
> `ZONA_MORTA_MV = 30`

⚠️ **Esses 50 mV são cálculo, não medida da sua bancada** — resistência típica de contato de
protoboard é de dezenas de mΩ, e trilho gasto chega a ohms. A ordem de grandeza é o que importa: é
maior que a zona morta.

**Como medir:** passo 3 do bringup, o teste de 3 passos de P-02.

**Mitigações já decididas em DEC-11:** capacitor de bulk no servo (ataca na fonte, reduzindo o
degrau de corrente), estrela no GND com pinos de GND separados para servo e LDRs, e não amostrar
durante o movimento.

**Quando responder, tocar:** esta seção com a medida, e a `ZONA_MORTA_MV` em `lib/controle/`
se sobrar resíduo que precise ser absorvido pelo limiar.

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
