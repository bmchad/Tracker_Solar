# Alimentação e perguntas abertas de hardware

As perguntas que o código-fonte não responde. Cada uma está catalogada como `P-nn` em
[../10-debts_and_pending/05-pendencias-de-hardware.md](../10-debts_and_pending/05-pendencias-de-hardware.md),
com o arquivo e a linha a tocar quando a resposta chegar.

---

## P-02 — O acoplamento servo→LDR pela rede de GND é tolerável?

⭐ **É a pergunta mais importante desta página, e ela não é sobre os motores — é sobre os
sensores.**

**Decidido (DEC-11):** os servos **ficam no `P5V_USB` da placa**, com três mitigações — capacitor
de bulk de 470–1000 µF no servo, estrela no GND com pinos de GND separados para servo e LDRs, e não
amostrar durante o movimento. A pergunta que resta é se isso basta, e o critério de fechamento é
uma **medição**: o teste de 3 passos de
[../02-software_specs/06-bringup-e-testes.md](../02-software_specs/06-bringup-e-testes.md).

Dois SG90 em movimento puxam **mais de 1 A de pico**, e mais ainda em stall. Uma porta USB 2.0
entrega 500 mA, e o **LDO de 3,3 V da placa é alimentado desse mesmo 5 V** — então o pico não é só
ruído: pode resetar o MCU (**D-17** → relógio volta a 15:00, **D-06** → 12 min de varredura).

⭐ **A boa notícia, e é por isso que a espera é razoável:** a medição é **ratiométrica**. O 3,3 V que
excita os divisores é o mesmo que chega a VREFH, então ele se cancela na razão `Vdivisor/Vref` e o
código bruto é imune, em primeira ordem, a queda do rail. Somado a isso, servo no 5 V e divisor no
3,3 V já separam o lado da alimentação — o único condutor compartilhado é o **GND**.

⛔ **O que sobra, e mantém a pendência:** a parcela **diferencial** da queda IR no GND (**P-10**),
que não cancela porque queda IR ao longo de um condutor é gradiente e não offset. Detalhe completo
em [../10-debts_and_pending/05-pendencias-de-hardware.md](../10-debts_and_pending/05-pendencias-de-hardware.md).

**Se o teste falhar:** fonte externa, escolhida por **ESR** e não capacidade — power bank USB 5 V ou
4× AA NiMH; alcalina 6 V tem quase 1 Ω e cai ~1 V a 1 A. ⛔ E nunca terra flutuante: o PWM é tensão
referida ao terra.

---

## P-03 — Qual o resistor do divisor, e ele é pull-up ou pull-down?

A topologia define o **sinal** do erro. Detalhado em
[02-sensores-ldr-e-arranjo-optico.md](02-sensores-ldr-e-arranjo-optico.md) — se estiver invertida,
o tracker foge do sol e vai direto ao batente.

O valor do resistor define onde a faixa útil do LDR cai dentro dos 0–3,3 V. Um valor mal escolhido
comprime toda a variação num canto da escala, e aí 12 bits de resolução não ajudam: o contraste
entre quadrantes fica da ordem do ruído.

---

## P-04 — Qual a faixa real de mV que os LDRs entregam no local de operação?

Não é curiosidade: é o que **calibra** a zona morta e o ganho da spec de controle. Números
necessários, medidos com a placa travada à mão:

| Condição | O que medir |
|---|---|
| Sombra total (os 4 cobertos) | o piso, e o **espalhamento entre os quatro** — é o casamento dos sensores |
| Alvo (luz de frente, equilibrado) | o valor típico, e quanto ele **oscila** parado, que é o piso do ruído |
| Desequilíbrio de ~10° | a diferença entre quadrantes, que é o ganho em mV por grau |
| Saturação | a partir de que ângulo um quadrante sombreia por completo e o erro **para de crescer** |

`ZONA_MORTA_MV` tem que ficar **acima** do ruído medido no alvo, e `GANHO_MV_POR_GRAU` sai direto
da terceira linha. Sem essas medições, os valores da spec de controle são chute inicial.

---

## ~~P-01 — Quais pinos analógicos para os dois LDRs novos?~~ ✅ resolvido

**PTB1 (`A1`) para o de cima, PTB3 (`A3`) para o de baixo** — os quatro LDRs em `A0`–`A3`,
contíguos. Deixou de ser pergunta aberta; a tabela em
[01-plataforma-e-pinagem.md](01-plataforma-e-pinagem.md) é dona do fato, e o critério está em
**DEC-12**.

---

## P-05 — Qual a altura da aleta do X em relação ao espaçamento dos sensores?

Define ganho e ângulo de captura da malha. Ver
[02-sensores-ldr-e-arranjo-optico.md](02-sensores-ldr-e-arranjo-optico.md).

---

## P-06 — Existe batente mecânico no eixo vertical, e em que ângulos?

Sem essa resposta o eixo vertical não tem limite de curso — ver
[03-atuadores-sg90s.md](03-atuadores-sg90s.md).

---

## P-07 — Há painel fotovoltaico real, ou só uma chapa?

Muda massa, inércia e torque exigido. Um SG90 entrega cerca de 1,8 kgf-cm: com painel real, o eixo
vertical pode não sustentar a placa fora do centro de massa — e um servo que não vence a carga
consome corrente sem mover, o que realimenta **P-02**.

Também muda o que "funcionar" significa: com painel real dá para medir corrente gerada e validar o
apontamento pelo que ele produz, em vez de acreditar no log.

---

## P-09 — Para onde aponta o servo horizontal em 0°?

O offset de montagem que liga o ângulo mecânico do servo ao azimute do céu. Sem ele o feedforward
não pode traduzir "o sol está a −30°" em "vá para 60° de servo". Ver
[../02-software_specs/04-eixo-vertical-feedforward.md](../02-software_specs/04-eixo-vertical-feedforward.md).
