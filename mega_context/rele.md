# Relé SRD-05VDC-SL-C: o mínimo para entender a bancada

Escrito para responder **uma** pergunta (duvidas.md, DV-18): **como os três relés da bancada estão
ligados ao motor, e essa ligação é segura?** Não é um curso de relés: só o necessário para olhar a
protoboard e saber.

---

## 1. O que é um relé

É um **interruptor movido por um eletroímã**. Tem dois lados que **não se tocam eletricamente**:

| Lado | O que é | Quem liga nele |
|---|---|---|
| **Bobina** (controle) | um eletroímã. Com corrente, ele puxa uma alavanca de metal | a placa, pelo módulo (5 V, dezenas de mA) |
| **Contatos** (potência) | a alavanca e os dois pontos em que ela pode encostar | o motor (12 V, ampères) |

A placa nunca toca nos 12 V do motor: ela só liga e desliga o eletroímã, e o eletroímã move a chave.

## 2. Os três contatos: COM, NO e NC

O SRD-05VDC-SL-C tem **uma chave de duas posições**:

- **COM** (comum) é a **alavanca**: está sempre ligada a algum dos outros dois.
- **NC** (*normally closed*, normalmente fechado) é onde a alavanca encosta **em repouso**.
- **NO** (*normally open*, normalmente aberto) é onde ela encosta **quando o relé é acionado**.

```
  RELÉ SOLTO (bobina sem corrente)          RELÉ ACIONADO (bobina com corrente)

          ● NO   aberto                               ● NO   ◄── COM encosta aqui
                                                     ╱
  COM ●───● NC   fechado                      COM ●
                                                      ● NC   aberto
```

| Estado do relé | COM ligado a | Fica aberto |
|---|---|---|
| **solto** | **NC** | NO |
| **acionado** | **NO** | NC |

⭐ **"Normalmente" quer dizer "em repouso".** E o relé fica em repouso (solto) sempre que a placa não o
aciona: placa desligada, USB fora, placa em reset, firmware travado, ou o firmware mandando desligar.
Por isso **o que está ligado no NC é o que o motor recebe quando ninguém está no controle.**

A alavanca encosta num lado **ou** no outro, nunca nos dois: dentro de um relé, NO e NC nunca se
ligam entre si.

## 3. O nome, decifrado

`SRD-05VDC-SL-C`, pela nomenclatura do fabricante (Songle):

| Parte | Quer dizer |
|---|---|
| `SRD` | a família do relé |
| `05VDC` | a **bobina** é de **5 V contínuo**. Puxa ~70 mA, e por isso não liga direto num pino da placa |
| `S` | selado (tampa fechada) |
| `L` | bobina de 0,36 W |
| `C` | contato tipo **C**: a chave de duas posições da seção 2 (COM, NO, NC) |

Na tampa costuma vir impresso algo como `10A 250VAC 10A 30VDC`. Para o motor, vale o número **em
corrente contínua (VDC)**, e ele é para carga resistiva: motor puxa um pico maior na partida (DV-03).

## 4. Os dois módulos da bancada

A bancada tem **três relés em dois módulos** (2026-11-19). Cada relé vem montado num módulo, que tem a
eletrônica que liga a bobina (optoacoplador, transistor, diodo e um LED). Você não mexe nisso.

| Módulo | Relés | Pinos de controle | Papel |
|---|---|---|---|
| módulo de 1 relé | **R1** | GND, sinal, 5V | liga / desliga |
| módulo de 2 relés | **R2** e **R3** | GND, sinal, 5V para cada relé; **os dois sinais ligados juntos** | sentido |

Cada relé tem **dois conectores**:

| Conector | O que tem | Liga em |
|---|---|---|
| **Pinos de controle** | GND, sinal (IN), 5V | a placa |
| **Bornes de parafuso** (potência) | **três**: NO, COM, NC | o motor e a alimentação de 12 V |

⚠️ A **ordem** dos três bornes muda de módulo para módulo. Leia o que está **escrito na placa** ao lado
de cada borne, ou o desenho impresso perto dele.

**Estes módulos acionam o relé quando o sinal (IN) recebe GND** (teste de 2026-11-19, DV-01). Com o
sinal em 3,3 V, em 5 V ou solto, o relé fica solto. O LED do canal acende com o relé acionado.

## 5. A ligação pretendida: R1 liga e desliga, R2 e R3 invertem

É o circuito do [circuito.md](circuito.md). R1 fica no caminho do **GND**; R2 e R3 escolhem qual fio
do motor recebe +12 V e qual recebe GND:

```
 GND ───────── COM ● R1
                   NO ●──────────────── "GND do R1" ──────┬───────────────────┐
                   NC ●  (vazio)                           │                   │
                                                           │                   │
            R2                                             │        R3         │
   NO  ●──── "GND do R1" ──────────────────────────────────┘   NC ●────────────┘
   COM ●──── M1 ══════════════[ MOTOR ]═══════════════ M2 ──── COM ●
   NC  ●──── +12 V                                             NO ●──── +12 V
```

- **R1:** COM no GND, **NO** na linha que alimenta R2 e R3 ("GND do R1"), NC vazio.
- **R2:** COM no fio M1 do motor, NC no +12 V, NO no "GND do R1".
- **R3:** COM no fio M2, NC no "GND do R1", NO no +12 V. Os papéis de NO e NC são **cruzados** em
  relação ao R2.

| R1 | R2 e R3 (juntos) | M1 recebe | M2 recebe | Motor |
|---|---|---|---|---|
| **solto** | qualquer | sem GND | sem GND | **parado** |
| acionado | soltos | +12 V | GND | gira num sentido |
| acionado | acionados | GND | +12 V | gira no outro |

⭐ **Por que é segura:**
- **Com R1 solto, o motor não tem GND e não gira.** É o estado com a placa desligada, em reset, com o
  USB fora ou com o firmware travado.
- R2 e R3 não comutam exatamente juntos. Nos poucos milissegundos em que um já trocou e o outro não, os
  dois fios do motor ficam no mesmo potencial: o motor fica parado, sem curto.
- O +12 V nunca encontra o GND sem passar pelo motor.

⛔ **A única regra que decide a segurança: a linha do R1 tem de estar no NO.** Se estiver no NC, tudo se
inverte: R1 solto passa a significar **motor energizado**.

## 6. Como descobrir a ligação da bancada

### 6.1 Ler a protoboard

```
  trilho  +  ● ● ● ● ● ● ● ● ● ● ●     cada trilho comprido da borda é um ponto só
  trilho  −  ● ● ● ● ● ● ● ● ● ● ●     (em algumas protoboards ele é cortado no meio)

             1   2   3   4   5 ...
          a  ●   ●   ●   ●   ●
          b  ●   ●   ●   ●   ●         os 5 furos de uma mesma COLUNA, de a até e,
          c  ●   ●   ●   ●   ●         são um ponto só: tudo que entra neles está ligado
          d  ●   ●   ●   ●   ●
          e  ●   ●   ●   ●   ●
            ─────── canal do meio ───────   separa as duas metades
          f  ●   ●   ●   ●   ●         de f até j é OUTRO ponto, mesmo na mesma coluna
          ...
```

"Seguir um fio" é isto: ver em que furo ele entra e o que mais está espetado no mesmo grupo de 5 furos
(ou no mesmo trilho). Na dúvida, o multímetro em **continuidade** (o modo que apita) diz se dois pontos
estão ligados. Faça isso **com tudo desligado**.

### 6.2 Preencher a tabela

Com a alimentação do motor **desconectada** e o USB fora, olhe os **bornes de parafuso** (não os pinos
de controle) e anote para onde vai o fio de cada um: **M1** ou **M2** (um fio do motor), **+12 V**,
**GND**, **o fio que vem do R1**, ou **nada**.

| Borne | R1 (módulo de 1 relé) | R2 (módulo de 2, primeiro relé) | R3 (módulo de 2, segundo relé) |
|---|---|---|---|
| NO | | | |
| COM | | | |
| NC | | | |

### 6.3 Ler o resultado

| O que a tabela mostra | Conclusão |
|---|---|
| **R1:** COM no GND (ou no +12 V) e **NO** no fio que vai para R2 e R3; NC vazio | R1 **seguro** |
| **R1:** o fio que vai para R2 e R3 está no **NC** | ⛔ **perigoso:** com R1 solto o motor recebe energia. Passe esse fio do NC para o NO |
| **R2 e R3:** COM nos fios do motor; o NC de um e o NO do outro no +12 V; os dois restantes no fio do R1 | inversão certa |
| Alguma combinação liga o +12 V direto no GND, sem passar pelo motor | **curto**: não ligue a alimentação |

Para conferir uma ligação diferente dessas, use a regra da seção 2 (**solto → COM no NC; acionado → COM
no NO**) e veja o que cada fio do motor recebe em cada linha da tabela da seção 5.

## 7. O que isso explica sobre os dois códigos

- **Código validado** (`!codigo_base/`): declara os relés como ativos em nível **alto**, o contrário
  destes módulos. Para parar o motor ele põe 0 V em todos os pinos, e com estes módulos isso **aciona
  todos os relés, R1 inclusive**. Se o motor ficava parado assim, a linha do motor no R1 está no **NC**:
  acionar o R1 abre o NC e corta o motor. A fiação estaria compensando a polaridade invertida. O preço:
  **com R1 solto** (placa desligada, em reset, USB fora, firmware travado, ou a v1 gravada) **o NC
  fecha e o motor recebe energia**, girando no sentido de R2 e R3 soltos.
- **v1:** nível baixo, a polaridade certa para estes módulos, e dois sinais (LIGA para R1, SENTIDO para
  R2 e R3), que é o mesmo desenho da bancada. Parado = todos os relés soltos. **Ela só é segura com a
  linha do R1 no NO.** Com isso, a v1 serve para a bancada como está; falta conferir os pinos: LIGA no
  PTD4 (D2) e SENTIDO no PTD2 (D11).

⚠️ **Os dois códigos pedem fiações opostas no R1:** o validado precisa do NC, a v1 precisa do NO. Depois
de passar o R1 para o NO, não grave mais o código validado sem antes trocar o overlay dele para nível
baixo.
