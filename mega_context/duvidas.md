# Dúvidas em aberto do projeto real

Todas as dúvidas do projeto real num lugar só. Cada uma traz o contexto necessário para ser entendida
**sem abrir o documento de onde veio**.

⚠️ **Estas dúvidas são CÓPIAS** (decisão do usuário, 2026-11-19): as de [circuito.md](circuito.md) e
[angulo.md](angulo.md) continuam também lá. **Ao resolver uma, atualize aqui e na origem.** Os números
DV-NN são fixos: [main.md](main.md) e [main_adr.md](main_adr.md) citam por eles.

---

## Resumo

| ID | Pergunta | Estado | O que fica travado sem a resposta |
|---|---|---|---|
| DV-01 | O relé desliga quando a placa manda 3,3 V? | em parte | ligar o motor com segurança |
| DV-02 | R2 e R3 recebem o mesmo sinal? | ✅ resolvida | — (resta a corrente do pino) |
| DV-03 | O contato dos relés aguenta a corrente do motor? | aberta | ligar o motor com carga |
| DV-04 | A saída da controladora aguenta o motor travado? | aberta | ligar o motor com carga |
| DV-05 | A controladora chaveia o negativo da carga? | aberta | ligar os GNDs |
| DV-06 | Que proteção contra surto vai nos terminais do motor? | aberta | uso prolongado |
| DV-07 | Como ficam as chaves de fim de curso? | aberta | deixar o tracker funcionando sozinho |
| DV-08 | Qual microcontrolador, pinos e alimentação ficam no projeto? | em parte | instalação em campo |
| DV-09 | Qual sentido do motor sobe a placa? | aberta | o controle fechar a malha |
| DV-10 | Quais relés ficam em IN1 e IN2, e onde está o terceiro? | aberta | ligar os fios dos relés |
| DV-11 | Qual a velocidade real do motor com carga? | aberta | ajustar as proteções |
| DV-12 | Qual o curso mecânico do eixo? | aberta | definir curso e alvo da estrutura |
| DV-13 | Qual microcontrolador e ADC no projeto real? | em parte | — |
| DV-14 | A eletrônica fica na parte fixa ou na que gira? | aberta | passar os cabos |
| DV-15 | Potenciômetro ou acelerômetro? (experimento de bancada) | aberta | escolha definitiva do sensor |
| DV-16 | Quanto tempo o motor leva para parar? | aberta | — (há folga) |
| DV-17 | Quanto a placa anda depois de cortar o motor? | aberta | parar no alvo sem oscilar |
| DV-18 | A fiação do laboratório segue o esboço ou o circuito descrito? | aberta | **gravar a v1 no laboratório** |
| DV-19 | Quais os dois pontos de calibração do potenciômetro montado? | aberta | sair do modo calibração |
| DV-20 | Como tirar o `platformio.ini` de cada máquina do git? | aberta | compilar em outra máquina sem editar |
| DV-21 | O que fazer com as bibliotecas do protótipo em `lib/`? | aberta | — |
| DV-22 | De onde vem o alvo solar da v2? | aberta | a v2 |
| DV-23 | O que fazer com o repositório git dentro de `src/`? | aberta | — |

---

## A. Circuito e relés

Origem: [circuito.md](circuito.md), seção 4. **Contexto comum:** painel, bateria e motor ligam numa
controladora pronta (caixa-preta). O motor de vidro de 12 V passa por três relés: **R1** liga e
desliga; **R2 e R3**, comandados pelo mesmo sinal, invertem GND e VCC nos terminais do motor para mudar
o sentido. A placa (FRDM-KL25Z) comanda os relés pelo conector macho do módulo, nas entradas IN.

### DV-01 · O relé desliga quando a placa manda 3,3 V?

**Estado:** em parte resolvida · **Origem:** circuito.md §4, item 1

**Contexto.** Já se sabe duas coisas: o relé **liga quando a entrada IN recebe GND** (0 V), e o módulo
está **alimentado pelo pino 5V da FRDM-KL25Z** (2026-11-19). Para desligar, o pino da placa sobe para
3,3 V, e sobram ~1,4 a 1,7 V entre o VCC do módulo e a entrada IN, sobre o LED do optoacoplador e o
resistor dele (o 5V que vem do USB costuma ficar entre 4,7 e 5 V). ⚠️ É justamente nessa ligação que
alguns módulos **continuam ligados**. Muitos desligam normalmente; só o teste diz.

**Por que importa.** Se R1 não soltar com 3,3 V, o firmware manda parar e o motor continua girando. E
"relé solto" é o estado de repouso de toda a v1: modo calibração, parado e falha.

**Como resolver.** Sem o motor ligado nos relés:
1. **Teste com a própria v1.** Grave com `CALIBRACAO_MEDIDA = 0`. Nesse modo os dois pinos ficam o tempo
   todo em 3,3 V (relé desligado). Se algum relé clicar, ou o LED de algum canal acender, mesmo fraco, o
   módulo não desliga com 3,3 V.
2. **O outro lado.** Tire o fio que liga uma entrada IN ao pino da placa e encoste essa entrada no GND:
   o relé tem de clicar.

Se o teste 1 falhar: com jumper **JD-VCC** no módulo, tire o jumper e ligue o VCC (lado do opto) no 3V3
da placa e o JD-VCC (bobinas) no 5V; sem o jumper, use um transistor por entrada.

**Consumo no 5V da placa.** Cada bobina de relé de 5 V puxa da ordem de 70 a 90 mA: com três relés
acionados, ~0,25 A. Cabe no USB do computador (500 mA), mas em campo esse 5V tem de vir de outra fonte
(DV-08). Vale olhar a coluna `ruido` da serial no momento em que os relés comutam: o 3,3 V do
potenciômetro sai desse mesmo 5V.

### DV-02 · R2 e R3 recebem o mesmo sinal?

**Estado:** ✅ resolvida em 2026-11-19: **sim** · **Origem:** circuito.md §4, item 2

**Contexto.** Por isso a placa usa só duas saídas: LIGA (R1) e SENTIDO (R2 e R3 juntos).

**O que sobrou.** Um pino só aciona duas entradas de relé ao mesmo tempo, então a corrente que ele
absorve é a soma das duas. Os pinos comuns da KL25Z são especificados para cerca de 5 mA. Meça a
corrente de uma entrada IN acionada (multímetro em série); se o dobro passar disso, ponha um transistor
no sinal SENTIDO.

### DV-03 · O contato dos relés aguenta a corrente do motor?

**Estado:** aberta · **Origem:** circuito.md §4, item 3

**Contexto.** Relé vendido como "10 A" costuma valer 10 A em 30 VCC com carga **resistiva**. O motor é
indutivo, e na partida puxa perto da corrente de motor travado. Essa corrente **ainda não foi medida**;
motor de vidro travado costuma passar de 10 A, mas isso é ordem de grandeza.

**Por que importa.** Contato abaixo da corrente real se desgasta rápido ou solda fechado, e um R1
soldado deixa o motor ligado.

**Como resolver.** Medir a corrente girando com carga e a de motor travado (no máximo 1 s ligado) e
comparar com a especificação **em CC** do relé. O fusível também sai desse número.

### DV-04 · A saída da controladora aguenta o motor travado?

**Estado:** aberta · **Origem:** circuito.md §4, item 4

**Contexto.** O motor tira energia da saída da controladora (painel + bateria), que é caixa-preta. Toda
saída de controladora tem uma corrente máxima.

**Por que importa.** Se o motor travar (batente, peso, defeito), a controladora pode desarmar ou queimar.

**Como resolver.** Ler a corrente máxima da saída na etiqueta ou no manual da controladora e comparar
com a corrente de motor travado (DV-03).

### DV-05 · A controladora chaveia o negativo da carga?

**Estado:** aberta · **Origem:** circuito.md §4, item 5

**Contexto.** Muitas controladoras solares ligam e desligam a carga pelo **negativo** ("positivo
comum"). Nesse caso o GND da saída de carga **não** é o mesmo que o negativo da bateria.

**Por que importa.** Ligar um no outro por fora da controladora (por exemplo, juntando GNDs da placa e
do motor) anula a chave da controladora, inclusive o corte por bateria fraca.

**Como resolver.** Manual da controladora, ou medir continuidade entre o negativo da bateria e o negativo
da saída de carga com a saída desligada.

### DV-06 · Que proteção contra surto vai nos terminais do motor?

**Estado:** aberta · **Origem:** circuito.md §4, item 6

**Contexto.** Motor é carga indutiva: ao abrir R1 aparece um pico de tensão e arco no contato. O diodo
de roda-livre comum não serve aqui, porque o motor inverte de sentido e o diodo conduziria direto numa
das polaridades.

**Por que importa.** Sem proteção, o arco come o contato de R1 mais rápido, e o pico pode chegar à
eletrônica.

**Como resolver.** Escolher entre um **TVS bidirecional** ou um **snubber RC** nos terminais do motor,
dimensionado para 12 V e para a corrente medida.

### DV-07 · Como ficam as chaves de fim de curso?

**Estado:** aberta · **Origem:** circuito.md §4, item 7 (e angulo.md §10)

**Contexto.** Com o torque de um motor de vidro passando por uma redução de 20:1, uma falha (sensor
solto, acoplamento patinando) deixa o motor girando até quebrar algo. A v1 tem proteções por software
(sentido errado, sem movimento, tempo máximo), mas elas dependem do próprio sensor e da placa
funcionarem. Uma chave em série com R1 cortaria os dois sentidos e deixaria a placa presa no batente.

**Por que importa.** É a única proteção que funciona mesmo com o firmware travado ou o sensor solto.

**Como resolver.** Uma chave **normalmente fechada** em cada ponta do curso, na linha do motor, com um
diodo em paralelo que deixa passar só o sentido que **afasta** do batente. Chaves e diodos dimensionados
pela corrente do motor (DV-03).

### DV-08 · Qual microcontrolador, pinos e alimentação ficam no projeto?

**Estado:** em parte · **Origem:** circuito.md §4, item 8

**Contexto.** A v1 usa a **FRDM-KL25Z** com LIGA em PTD4 (D2), SENTIDO em PTD2 (D11) e o potenciômetro
em PTC2 (A4) (main_adr.md, ADR-05). Hoje a placa é alimentada pelo USB do computador, e o módulo de
relés tira as bobinas do pino 5V dela (~0,25 A com três relés acionados, DV-01).

**Falta decidir.** Se a KL25Z é a placa definitiva, e como ela é alimentada em campo: a partir da
bateria/controladora, o que exige um regulador para 5 V, e com que consumo.

### DV-09 · Qual sentido do motor sobe a placa?

**Estado:** aberta · **Origem:** circuito.md §4, item 9

**Contexto.** O sinal SENTIDO tem dois estados: em repouso (sentido A) e acionado (sentido B). Qual
deles aumenta o ângulo depende de como os fios do motor entraram nos relés e de como o potenciômetro foi
montado. No firmware isso é a constante `SENTIDO_AUMENTA` em `src/main.c`, hoje um palpite
(`MOTOR_SENTIDO_A`).

**Por que importa.** Com o sentido trocado, o motor foge do alvo. A proteção "SENTIDO ERRADO" desliga
o motor depois de 1° ao contrário.

**Como resolver.** Na primeira partida com o motor (main.md §10, passo 5). Se aparecer "SENTIDO
ERRADO", troque a constante. Registre o resultado aqui e em circuito.md.

### DV-10 · Quais relés ficam em IN1 e IN2, e onde está o terceiro?

**Estado:** aberta · **Origem:** circuito.md §4, item 10

**Contexto.** O módulo ligado à placa tem duas entradas, IN1 e IN2, mas o circuito tem três relés. Uma
possibilidade: este módulo de dois canais tem R2 e R3, com IN1 e IN2 ligados juntos ao SENTIDO (PTD2),
e R1 fica em outro módulo, ligado ao LIGA (PTD4).

**Por que importa.** Define para onde vai cada fio e se o pino SENTIDO aciona uma ou duas entradas
(DV-02).

**Como resolver.** Dizer onde está R1 e qual entrada é LIGA e qual é SENTIDO.

---

## B. Medição do ângulo

Origem: [angulo.md](angulo.md), seção 9. **Contexto comum:** a placa gira num eixo só (inclinação),
movida pelo motor de vidro por uma corrente de 20:1. O ângulo é medido por um potenciômetro de uma volta
preso na ponta do eixo da placa (1:1), lido pelo ADC de 12 bits. O acelerômetro é a alternativa
estudada.

### DV-11 · Qual a velocidade real do motor com carga?

**Estado:** aberta · **Origem:** angulo.md §9, item 1

**Contexto.** Não existe ficha técnica confiável do "motor de vidro Mabuchi fio preto". Os documentos
supõem **80 rpm → 24 °/s no eixo da placa** (rpm × 0,3 com a redução de 20:1), um chute de ordem de
grandeza.

**Por que importa.** A v1 depende desse número: a leitura a cada 10 ms (0,24° por leitura), a proteção
"sem movimento" (exige 0,5° por segundo), o tempo máximo de 30 s e quanto a placa anda depois do corte.

**Como resolver.** Marcar o eixo do motor, ligar em 12 V por 10 s e contar as voltas; depois repetir com
a placa montada.

### DV-12 · Qual o curso mecânico do eixo?

**Estado:** aberta · **Origem:** angulo.md §9, item 2

**Contexto.** A estrutura ainda não definiu até onde a placa pode inclinar. Os documentos usam 120° como
exemplo. A v1 usa o curso da **bancada**: 0° a 90°, alvo em 45° (main_adr.md, ADR-10).

**Por que importa.** O curso define a faixa válida do sensor, se o potenciômetro com batente comporta o
giro e os limites `ANGULO_MIN_D10`, `ANGULO_MAX_D10` e `ALVO_D10` do firmware.

**Como resolver.** Definição da mecânica; depois, preencher os três números em `src/main.c`.

### DV-13 · Qual microcontrolador e ADC no projeto real?

**Estado:** em parte · **Origem:** angulo.md §9, item 3

**Contexto.** A conta do ângulo depende da resolução e da referência do ADC e da impedância máxima que
ele aceita na entrada. A KL25Z tem ADC de 12 bits com referência de 3,3 V, que sobra para a precisão de
±2° pedida.

**Como resolver.** Se a KL25Z ficar como placa definitiva (DV-08), esta dúvida se fecha.

### DV-14 · A eletrônica fica na parte fixa ou na que gira?

**Estado:** aberta · **Origem:** angulo.md §9, item 4

**Contexto.** Algum cabo vai flexionar a cada movimento: o do sensor (se a placa ficar fixa) ou o da
alimentação (se ela girar junto).

**Por que importa.** Define qual cabo sofre fadiga e se dá para usar I²C (o acelerômetro), que não gosta
de cabo longo ao lado do motor.

**Como resolver.** Decisão de montagem.

### DV-15 · Potenciômetro ou acelerômetro? (experimento de bancada)

**Estado:** aberta · **Origem:** angulo.md §9, item 5

**Contexto.** O experimento da angulo.md §8 mede os dois sensores juntos de 0° a 90°, em passos de 10°,
contra um transferidor. A v1 usa só o potenciômetro.

**Por que importa.** Fecha a escolha do sensor definitivo.

**Como resolver.** Fazer o experimento e anotar os erros dos dois.

---

## C. Firmware

Origem: escrita da v1 ([main.md](main.md), [main_adr.md](main_adr.md)). **Contexto comum:** a v1 leva a
placa a um alvo fixo. Liga o motor se o erro passar de 1°, lê o potenciômetro a cada 10 ms enquanto ele
gira e o desliga ao chegar ao alvo. Qualquer anomalia desliga tudo até o reset.

### DV-16 · Quanto tempo o motor leva para parar?

**Estado:** aberta · **Origem:** firmware v1 (`MOTOR_TEMPO_PARADA_MS` em `lib/motor_rele/motor_rele.c`)

**Contexto.** Antes de religar o motor ao contrário, a biblioteca espera ele parar. O valor é **300 ms,
um chute** (o esboço usava 100 ms).

**Por que importa.** Religar ao contrário com o motor ainda girando puxa corrente perto da de motor
travado. Hoje pesa pouco: o firmware espera 5 s entre movimentos.

**Como resolver.** Cortar o motor e medir quanto o eixo leva para parar (vídeo em câmera lenta serve).

### DV-17 · Quanto a placa anda depois de cortar o motor?

**Estado:** aberta · **Origem:** firmware v1 (`ANTECIPACAO_D10`, `LIMIAR_PARTIDA_D10` em `src/main.c`)

**Contexto.** A v1 desliga o motor quando a placa chega ao alvo e só religa se o erro passar de 1°. A
rosca sem-fim deve frear rápido, mas ninguém mediu.

**Por que importa.** Se a placa passar do alvo mais de 1° depois do corte, ela religa ao contrário 5 s
depois e pode ficar indo e voltando.

**Como resolver.** Passo 6 de main.md §10: depois de "chegou ao alvo", ver quanto o ângulo ainda anda.
Pôr esse valor em `ANTECIPACAO_D10`, ou alargar `LIMIAR_PARTIDA_D10`.

### DV-18 · A fiação do laboratório segue o esboço ou o circuito descrito?

**Estado:** aberta · **Origem:** conflito entre o esboço (commit `79c8b99`) e a descrição do circuito

**Contexto.** A sessão do laboratório escreveu um esboço com **três relés independentes ativos em nível
alto**: avançar em PTD2, recuar em PTD3, habilitar em PTD4. Depois, o circuito foi descrito assim:
**R1 liga com GND na entrada** e **R2 e R3 recebem o mesmo sinal**. A v1 segue a descrição: dois sinais
ativos em nível baixo, LIGA em PTD4 e SENTIDO em PTD2 (main_adr.md, ADR-01, 02 e 05).

**Por que importa.** Se o laboratório estiver ligado como o esboço e os relés forem mesmo ativos em
nível alto, a v1 **liga os relés no boot**.

**Como resolver.** Antes de gravar a v1 no laboratório, conferir a fiação. Ou a fiação muda para a da
v1 (o fio do PTD3 sai), ou o overlay e a biblioteca mudam para a fiação de lá.

### DV-19 · Quais os dois pontos de calibração do potenciômetro montado?

**Estado:** aberta · **Origem:** firmware v1 (`calibracao` e `CALIBRACAO_MEDIDA` em `src/main.c`)

**Contexto.** O ângulo sai de uma reta entre dois pontos medidos: leitura N em dois ângulos conhecidos
(transferidor ou celular). Hoje a calibração é a **nominal** (N = 0 → 0°, N = 4095 → 300°), que não mede
nada de verdade. Enquanto `CALIBRACAO_MEDIDA` for 0, o firmware fica em modo calibração e o motor
**não liga**.

**Por que importa.** Sem ela, o motor não roda. E ela vale só para aquela montagem: refaça sempre que o
potenciômetro for reacoplado ao eixo.

**Como resolver.** main.md §10, passo 3.

### DV-22 · De onde vem o alvo solar da v2?

**Estado:** aberta · **Origem:** firmware v1 (o alvo fixo `ALVO_D10`) e angulo.md §10

**Contexto.** A v1 mira um ângulo fixo. Para seguir o sol, a v2 precisa de três coisas:
1. **A fórmula de um eixo só.** Com um eixo, o alvo é a projeção do vetor solar no plano de giro:
   `atan2(s_N, s_C)` com o eixo na direção leste–oeste, `atan2(s_L, s_C)` com ele na norte–sul.
2. **A orientação do eixo no chão.** Norte–sul rende cerca de +29 % sobre placa fixa; leste–oeste, cerca
   de +8 % (modelo de céu limpo, latitude −23,55°).
3. **A data e a hora**, inclusive depois de um reset ou de uma falta de energia.

O protótipo tinha `lib/angulo_incidência_solar` e `lib/init_hora`, nunca revalidados para o projeto
real.

**Como resolver.** Decidir a orientação e a fonte de hora; depois revalidar ou reescrever o cálculo.

---

## D. Repositório e ambiente

### DV-20 · Como tirar o `platformio.ini` de cada máquina do git?

**Estado:** aberta · **Origem:** resumo da sessão do laboratório e esta sessão

**Contexto.** O `platformio.ini` versionado aponta `core_dir = D:/DevTools/.platformio`, que só existe
num dos computadores. No laboratório o drive D: é a própria placa (DAPLINK), e foi preciso apagar essa
linha para compilar lá. Neste computador também há `monitor_port = COM15`, que **não foi commitado** de
propósito.

**Por que importa.** Cada máquina que compila precisa editar um arquivo versionado, e o commit de uma
quebra a outra.

**Como resolver.** A mesma solução do repositório da disciplina: tirar o `platformio.ini` do git
(`.gitignore`) e versionar um `platformio.ini.example` sem `core_dir` nem `monitor_port`; cada máquina
copia e ajusta o seu.

### DV-21 · O que fazer com as bibliotecas do protótipo em `lib/`?

**Estado:** aberta · **Origem:** decisão de dedicar o repositório ao tracker real (2026-11-19)

**Contexto.** Continuam em `lib/`: `ldr`, `servo_motor`, `pwm`, `teste_console`, `teste_ldr` e
`teste_rot_cont` (só do protótipo), além de `init_hora` e `angulo_incidência_solar` (talvez úteis na
v2, DV-22). Nenhuma entra no build, porque o PlatformIO só compila o que o `main.c` inclui. A `lib/ldr`
nem compila mais: o overlay deixou de ter os canais dos LDRs.

**Por que importa.** Quem abrir `lib/` não sabe o que é do projeto real e o que é legado.

**Como resolver.** Apagar (o git guarda o histórico), mover para uma pasta de legado, ou manter só as
duas que podem servir à v2.

### DV-23 · O que fazer com o repositório git dentro de `src/`?

**Estado:** aberta · **Origem:** revisão do repositório em 2026-11-17

**Contexto.** `src/` tem uma pasta `.git` própria, de outro repositório (`carrinho_tracker_3_sensores`).
O último commit dela apaga quase tudo e está 1 commit à frente do remoto. Ao mesmo tempo, o repositório
principal versiona `src/main.c`.

**Por que importa.** Um comando git rodado de dentro de `src/` vai para o repositório errado, e um push
de lá publicaria aquele commit destrutivo.

**Como resolver.** Apagar `src/.git` depois de confirmar que nada nele faz falta.
