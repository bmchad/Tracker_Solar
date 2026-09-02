# Alimentação e perguntas abertas de hardware

As perguntas que o código-fonte não responde. Cada uma está catalogada como `P-nn` em
[../10-debts_and_pending/05-pendencias-de-hardware.md](../10-debts_and_pending/05-pendencias-de-hardware.md),
com o arquivo e a linha a tocar quando a resposta chegar.

---

## P-02 — De onde sai a alimentação dos servos?

⭐ **É a pergunta mais importante desta página, e ela não é sobre os motores — é sobre os
sensores.**

Dois SG90 em movimento puxam **mais de 1 A de pico**, e mais ainda em stall (empurrando contra um
batente). O regulador da FRDM-KL25Z não entrega isso.

O modo de falha não é o óbvio. Se os servos saem do 3,3 V ou do 5 V da placa, cada movimento
**afunda o VDD** — e o ADC usa `ADC_REF_INTERNAL`, que na KL25Z **é o próprio VDD**. Então:

> Quando o motor gira, a referência do ADC cai, e **as quatro leituras de LDR caem juntas** sem que
> a luz tenha mudado.

O erro é uma **diferença**, então uma queda proporcional nos quatro tende a se cancelar — mas só
tende: os quatro canais são amostrados em instantes diferentes, dentro do transiente. O resultado
é ruído correlacionado com o próprio movimento, que é exatamente a receita de oscilação
auto-sustentada. E é indistinguível, no log, de um problema de algoritmo.

**Resposta necessária:** fonte externa para os servos, com **terra comum** com a placa. Se já é o
caso, registrar aqui e fechar P-02 — porque enquanto isso não estiver escrito, todo ajuste de zona
morta está tentando compensar um problema elétrico em software.

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

## P-01 — Quais pinos analógicos para os dois LDRs novos?

Sugestão PTB1 (`ADC0_SE9`) e PTB3 (`ADC0_SE13`); alternativa PTC1 (`SE15`) e PTC2 (`SE11`).
Ver a tabela em [01-plataforma-e-pinagem.md](01-plataforma-e-pinagem.md). É decisão de montagem —
o software tem um ponto único de troca, descrito em
[../02-software_specs/01-devicetree-adc-4-canais.md](../02-software_specs/01-devicetree-adc-4-canais.md).

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
