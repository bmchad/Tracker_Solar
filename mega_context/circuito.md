# Circuito do motor: controladora, bateria e três relés

Registra como o circuito de potência do projeto real vai funcionar (definido em 2026-11-19). As
peças estão em [info.md](info.md).

---

## 1. Visão geral

```
 Painel solar ──┐
                ├──  Controladora  ──── saída ────  3 relés  ────  Motor de vidro 12 V
 Bateria ───────┘   (já programada)                    ▲
                                                       │ sinais de controle
                                                microcontrolador
```

- O **painel solar**, a **bateria** e o **motor** ligam na **controladora**.
- A controladora já vem programada para receber energia (do painel) e fornecer energia (da bateria).
  ⭐ **Ela é caixa-preta e fica fora do escopo:** nada aqui planeja carga, descarga ou proteção de
  bateria.
- O motor não liga direto na controladora: passa por **três relés**, comandados pelo
  microcontrolador.

---

## 2. Os três relés

| Relé | Função | Como funciona |
|---|---|---|
| **R1** | liga / desliga | motor **ligado** quando R1 recebe **GND**, **desligado** quando não recebe |
| **R2** e **R3** | sentido | invertem **GND** e **VCC** nos dois terminais do motor. Recebem o **mesmo sinal**: acionam e soltam juntos |

O microcontrolador precisa, portanto, de **duas saídas**: **LIGA** (R1) e **SENTIDO** (R2 e R3
juntos).

O que o motor faz em cada combinação:

| LIGA (R1) | SENTIDO (R2 e R3) | Motor |
|---|---|---|
| não recebe GND | qualquer | **parado** |
| recebe GND | em repouso | gira num sentido (A) |
| recebe GND | acionados | gira no outro (B) |

⭐ **O estado de repouso é seguro:** sem GND em R1, o motor fica parado, seja qual for a posição dos
relés de sentido. Então, se o microcontrolador travar, resetar ou perder energia, o motor para,
**desde que** a entrada de R1 fique sem GND nessas situações. Isso tem de ser conferido na montagem
(veja a seção 4, item 1).

⭐ Com relé o motor é **liga/desliga, sem PWM**. Isso basta aqui: a redução por rosca sem-fim do
motor segura a placa parada sem energia ([angulo.md](angulo.md), seção 0), e o sol anda só 15° por
hora.

---

## 3. Regras que o firmware precisa seguir

Estas regras não são escolha de projeto: elas vêm de usar relés para chavear um motor.

### 3.1 Só troque o sentido com o motor desligado

Para inverter o sentido, nesta ordem:

1. desligue **R1**;
2. espere o motor parar (tempo **a medir** na bancada);
3. troque o sinal **SENTIDO** (R2 e R3) e espere o tempo de operação do relé (está na folha de
   dados, da ordem de 10 ms);
4. ligue **R1**.

⛔ **Trocar R2/R3 com corrente passando** faz os contatos de R2/R3 abrirem a corrente do motor com
arco elétrico. E inverter o motor ainda girando puxa uma corrente perto da de motor travado, ou
mais. Isso desgasta ou solda os contatos.

### 3.2 Só R1 abre corrente, e é ele que se gasta

Seguindo a 3.1, R2 e R3 nunca abrem corrente: eles só se gastam mecanicamente, e a vida mecânica de
relé é muito longa. Quem abre a corrente do motor, e por isso se desgasta, é **R1**.

Uma conta de ordem de grandeza: com uma correção a cada 12 min durante 12 h de sol, R1 aciona umas
**60 vezes por dia, ~22 mil por ano**. A vida elétrica típica de um relé é da ordem de **100 mil
operações** na corrente nominal, o que daria alguns anos. ⚠️ O número certo está na folha de dados
do relé escolhido, e só vale se a corrente do motor estiver dentro da nominal do contato (seção 4,
item 3).

---

## 4. O que falta definir ou verificar

Cada item tem uma cópia com contexto em [duvidas.md](duvidas.md) (DV-01 a DV-10). Ao resolver um,
atualize os dois lugares.

| # | O quê | Por que importa |
|---|---|---|
| 1 | ✅ **Resolvido (2026-11-19): o módulo liga com GND na entrada IN e desliga com 3,3 V.** A KL25Z liga no conector macho do módulo (entradas IN1 e IN2), e o módulo é alimentado pelo **5V da placa** | Teste de bancada, com a entrada IN encostada à mão: **GND → o relé liga; 3V3 → não liga; 5V → não liga.** Acionamento em nível baixo, e o nível alto de 3,3 V da KL25Z basta para deixar o relé solto. ⚠️ O overlay do código validado (`!codigo_base/boards/frdm_kl25z.overlay`) declara os relés como ativos em nível **alto**: com este módulo, ele deixaria os três relés ligados com o motor parado (duvidas.md, DV-18) |
| 2 | ✅ **Resolvido (2026-11-19): R2 e R3 recebem o mesmo sinal.** | São duas saídas no total (seção 2). Um pino só aciona as duas entradas de relé ao mesmo tempo, então some a corrente das duas e confira se cabe no pino. Na KL25Z, os pinos comuns são especificados para cerca de 5 mA; se não couber, use um transistor no sinal SENTIDO |
| 3 | Corrente de contato dos relés **em corrente contínua** × corrente do motor | Relé vendido como "10 A" costuma valer 10 A em 30 VCC com carga **resistiva**. O motor é indutivo, e a partida puxa perto da corrente de travamento, que **ainda não foi medida** |
| 4 | Corrente máxima da **saída da controladora** | Se o motor travar, a controladora pode desarmar ou queimar |
| 5 | A saída da controladora é chaveada no **negativo**? | Muitas controladoras solares chaveiam o negativo da carga. Nesse caso o GND da saída **não** é o negativo da bateria, e ligar um no outro por fora da controladora anula a chave dela |
| 6 | Proteção contra surto nos terminais do motor | Motor que inverte de sentido não aceita diodo de roda-livre simples, porque ele conduziria numa das polaridades. Opções: TVS bidirecional ou snubber RC |
| 7 | **Fim de curso por hardware** ([angulo.md](angulo.md), seção 10) | Uma chave em série com R1 cortaria os dois sentidos e deixaria a placa presa no batente. A forma que funciona: uma chave normalmente fechada por ponta, na linha do motor, com um diodo em paralelo que deixa passar só o sentido que afasta do batente |
| 8 | Microcontrolador, pinos e alimentação dele | Ainda não definido ([angulo.md](angulo.md), seção 9, item 3) |
| 9 | Qual sentido (A ou B) **sobe** e qual **desce** a placa | Só dá para saber com a estrutura montada. Registre aqui quando souber |
| 10 | ✅ **Em parte (2026-11-19): três relés em dois módulos.** R1 sozinho num módulo de um relé; R2 e R3 num módulo de dois, com os dois sinais ligados juntos ([rele.md](rele.md) §4) | Falta saber em quais pinos da placa estão os dois sinais (a v1 espera LIGA no PTD4 e SENTIDO no PTD2) |

---

## 5. Relação com angulo.md

- **Seção 2** (cabo do sensor longe dos fios do motor): continua valendo. Onde lá se fala em "ponte
  H", leia "relés": o ruído agora vem das escovas do motor e do arco de R1 ao abrir.
- **Seção 10** (fim de curso por hardware): continua necessário com relés (item 7 acima).
