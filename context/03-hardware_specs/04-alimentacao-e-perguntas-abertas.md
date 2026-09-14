# Alimentação e perguntas abertas de hardware

As perguntas que o código-fonte não responde. Cada uma está catalogada como `P-nn` em
[../10-debts_and_pending/05-pendencias-de-hardware.md](../10-debts_and_pending/05-pendencias-de-hardware.md),
com o arquivo e a linha a tocar quando a resposta chegar.

⭐ **Depois de 2026-09-14 sobrou uma pergunta aberta nesta página: P-02.** As outras foram
respondidas (P-01, P-03, P-04, P-06) ou encerradas por decisão de escopo (P-05 → DEC-14,
P-07 → DEC-15, P-09 → DEC-13). Fora desta página continuam abertas **P-08** (faixa de pulso do
MG90S) e **P-10** (queda IR diferencial no GND).

---

## P-02 — O acoplamento servo→LDR pela rede de GND é tolerável?

⭐ **É a pergunta mais importante desta página, e ela não é sobre os motores — é sobre os
sensores.**

**Decidido (DEC-11):** os servos **ficam no `P5V_USB` da placa**, com três mitigações — capacitor
de bulk de 470–1000 µF no servo, estrela no GND com pinos de GND separados para servo e LDRs, e não
amostrar durante o movimento. A pergunta que resta é se isso basta, e o critério de fechamento é
uma **medição**: o teste de 3 passos de
[../02-software_specs/06-bringup-e-testes.md](../02-software_specs/06-bringup-e-testes.md).

Dois MG90S em movimento puxam **mais de 1 A de pico** (catálogo, não medido), e mais ainda em
travamento. Uma porta USB 2.0
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

⭐ **Decidido em 2026-09-14 (DEC-16): a fonte externa entrou, sem esperar o teste.** Os servos saíram
do `P5V_USB`. A montagem está na seção abaixo.

---

## Como alimentar os servos — a montagem (DEC-16)

1. **Carregador USB-A comum de parede, 5 V, ≥ 2 A.** Um carregador antigo de celular serve, e é o
   mais previsível: sem negociação de tensão, ele fica em 5 V fixos. ⚠️ Não usar gatilho USB-C/PD.
2. **De onde saem os fios:** cabo USB sacrificado ou breakout USB fêmea. Por convenção **vermelho =
   +5 V, preto = GND**; verde e branco são dados e ficam sem uso.
3. **Ligação:** V+ dos dois servos no vermelho, GND dos dois no preto, **e um fio do preto até um
   pino de GND da placa** — um ponto só, que é a estrela da **DEC-10**.

   ⭐ **Os dois servos em paralelo no mesmo 5 V é o normal, e não queima nada:** em paralelo o que é
   comum é a **tensão**; a corrente cada carga puxa a sua. Os "2 A" do carregador são teto de
   capacidade, não corrente imposta — fonte com folga é melhor que fonte no limite.

   ⛔ **O que queima de verdade:** tensão acima de ~6 V (um gatilho USB-C/PD negociando 9 V mata os
   dois na hora) e polaridade invertida. Por isso o passo obrigatório antes de encostar o servo é
   **medir a saída do carregador com o multímetro** (~5,0–5,2 V) e **confirmar qual fio é o
   positivo** — cabo USB barato nem sempre respeita a convenção de cores.
4. ⛔ **O vermelho não encosta em `P5V_USB` nem em `P3V3`.** Injetar 5 V no rail da placa realimenta
   a porta USB do PC. O único condutor compartilhado é o **GND**.
5. **Capacitores nos terminais de alimentação do servo — são dois papéis, não um:**

   | Peça | Papel | Situação |
   |---|---|---|
   | **470 nF** cerâmico | desacoplamento de **alta frequência**: mata a borda rápida do chaveamento da ponte H, que é o que se acopla nos fios analógicos dos LDRs (**P-10**) | ✅ na bancada — usar no lugar do 100 nF que a DEC-11 pedia, com pernas curtas |
   | **470–1000 µF** eletrolítico (≥ 10 V) | **reservatório**: entrega o degrau de partida do motor sem puxá-lo pelo fio da fonte | ⚠️ **item de compra**, não existe |

   ⛔ **Os dois não se substituem, e a conta mostra a distância.** A corrente de partida dura
   **milissegundos**. Para segurar 1 A com queda de meio volt, 1000 µF duram ~500 µs; **470 nF duram
   ~235 ns** — quatro ordens de grandeza abaixo. O nanofarad é filtro; o microfarad é reserva.

   Com a fonte externa de 2 A o eletrolítico deixa de ser crítico (a folga passa a vir da fonte),
   mas continua sendo o que suaviza o degrau na origem.
6. **Fios de servo longe dos analógicos dos LDRs**, sem trechos longos em paralelo (**P-10**).

⚠️ **Segurança:** é rede elétrica. Carregador de procedência, sem emenda exposta, e bancada
energizada não fica sozinha.

### ⭐ A medição que aposenta o catálogo

Com o multímetro **em série** na alimentação dos servos (escala de 10 A, ponta na entrada de
corrente), durante a varredura: anotar o valor típico em movimento e o pico na partida. É o número
que fecha o lado do consumo de **P-02** e **P-07**, e que substitui os valores de catálogo da tabela
de [03-atuadores-mg90s.md](03-atuadores-mg90s.md).

---

## ~~P-03 — Qual o resistor do divisor, e ele é pull-up ou pull-down?~~ ✅ resolvido

**Pull-down com 10 kΩ** (marrom-preto-laranja): `GND → resistor → pino do ADC → LDR → 3,3 V`. Mais
luz = mais tensão, logo as duas subtrações do controle valem como estão e **nada precisa ser
invertido**. O fato é dono de
[02-sensores-ldr-e-arranjo-optico.md](02-sensores-ldr-e-arranjo-optico.md), seção "Circuito"; o
passo 1 do bringup deixa de descobrir a topologia e passa a **confirmá-la**.

⚠️ **O que continua aberto é a outra metade da pergunta, e ela virou P-04 inteira:** o valor do
resistor decide onde fica a meia escala (LDR = 10 kΩ), não onde a faixa útil do LDR cai dentro dos
0–3,3 V no local de operação. Se a variação toda comprimir num canto da escala, 12 bits de
resolução não ajudam — o contraste entre quadrantes fica da ordem do ruído, e aí é o resistor que
muda de valor.

---

## ~~P-04 — Qual a faixa real de mV que os LDRs entregam no local de operação?~~ ✅ resolvido

**Ordem de 10² a 10³ mV** — de ~100 mV a ~1 V, num fundo de escala de 3,3 V. Dono do fato e das
consequências (em que faixa de resistência o LDR opera, quanta resolução sobra, e o que muda ao sol
aberto): [02-sensores-ldr-e-arranjo-optico.md](02-sensores-ldr-e-arranjo-optico.md), seção "A faixa
entregue".

⚠️ **O que sobra não é pergunta, é medição de bringup:** piso de ruído no alvo, espalhamento entre
os quatro na sombra, e mV por grau. São os três números que calibram `ZONA_MORTA_MV` e
`GANHO_MV_POR_GRAU`, e só saem do passo 1 de
[../02-software_specs/06-bringup-e-testes.md](../02-software_specs/06-bringup-e-testes.md), com a
placa travada à mão. Até lá as constantes da spec de controle continuam marcadas como chute
inicial.

---

## ~~P-01 — Quais pinos analógicos para os dois LDRs novos?~~ ✅ resolvido

**PTB1 (`A1`) para o de cima, PTB3 (`A3`) para o de baixo** — os quatro LDRs em `A0`–`A3`,
contíguos. Deixou de ser pergunta aberta; a tabela em
[01-plataforma-e-pinagem.md](01-plataforma-e-pinagem.md) é dona do fato, e o critério está em
**DEC-12**.

---

## ~~P-05 — Qual a altura da aleta do X em relação ao espaçamento dos sensores?~~ ❌ encerrada

**Fora de escopo por decisão (DEC-14).** O número serviria para *prever* o ganho óptico, e o ganho
vai ser **medido** no passo 1 do bringup de qualquer forma. A aleta continua sendo o parafuso
físico da malha: volta a importar se aparecer zig-zag no alvo ou erro que satura cedo. Ver
[02-sensores-ldr-e-arranjo-optico.md](02-sensores-ldr-e-arranjo-optico.md).

---

## ~~P-06 — Existe batente mecânico no eixo vertical, e em que ângulos?~~ ✅ resolvido

**Não há batente da montagem: quem para é o próprio servo, que estala passada meia volta — curso de
±90° em torno do centro.** ⚠️ Mas o limite no código **não** é 0°–180°: com o fator 1,47 do eixo
vertical, 180° comandados viram 2,47 ms de pulso, além do fim de curso do servo. O vertical satura
em **10°–160°**, provisório até **P-08** medir a faixa de pulso. A conta que liga uma coisa à outra
é dona de [03-atuadores-mg90s.md](03-atuadores-mg90s.md).

---

## ~~P-07 — Há painel fotovoltaico real, ou só uma chapa?~~ ❌ encerrada

**Fora de escopo por decisão (DEC-15).** A virada é de sensoriamento e controle, e a carga é a que
está na bancada.

⚠️ **O que se abre mão junto:** o único critério de sucesso **independente do firmware**. Sem painel
real não há corrente gerada para medir, e o tracker segue juiz de si mesmo — as únicas evidências
são o passo 1 do bringup e o θ de diagnóstico, os dois calculados pelo próprio código. E se o
vertical um dia não sustentar a carga (servo zumbindo sem mover), a massa volta a importar, porque
servo que não vence a carga consome corrente sem mover e realimenta **P-02**.

---

## ~~P-09 — Para onde aponta o servo horizontal em 0°?~~ ❌ encerrada

**O eixo horizontal fica em malha fechada pura (DEC-13):** o ajuste é dinâmico, e não haverá
tradução de azimute do céu para grau de servo. `AZIMUTE_DO_SERVO_ZERO` sai do projeto.

⚠️ **Duas coisas somem junto com ele**, e estão registradas na decisão: a **posição de partida** no
azimute (o substituto barato é uma varredura de 10° a 170° no boot, escolhendo o ponto de maior
soma dos quatro LDRs — não precisa de bússola nem de relógio) e o **envelope de sanidade
horizontal**, que era o que impedia perseguir reflexo de janela ou poste. O envelope **vertical**
continua valendo: ele depende da elevação solar, não deste offset. Ver
[../02-software_specs/04-eixo-vertical-feedforward.md](../02-software_specs/04-eixo-vertical-feedforward.md).
