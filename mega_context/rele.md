# Relé SRD-05VDC-SL-C: o mínimo para entender a bancada

Escrito para responder **uma** pergunta (duvidas.md, DV-18): **como os dois relés da bancada estão
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

## 4. O módulo de dois relés

Os dois relés estão numa placa (o **módulo**). Para cada relé, ela tem a eletrônica que liga a bobina:
um optoacoplador, um transistor, um diodo e um LED. Você não mexe nisso. O que importa são os dois
conectores:

| Conector | Pinos | Liga em |
|---|---|---|
| **Pinos macho** (controle) | VCC, GND, IN1, IN2 | a placa: VCC no 5V, GND no GND, IN1 e IN2 em pinos de sinal |
| **Bornes de parafuso** (potência) | **três por relé**: NO, COM, NC | o motor e a alimentação de 12 V |

⚠️ A **ordem** dos três bornes muda de módulo para módulo. Leia o que está **escrito na placa** ao lado
de cada borne, ou o desenho impresso perto dele.

**Este módulo aciona o relé quando a entrada IN recebe GND** (teste de 2026-11-19, DV-01). Com IN em
3,3 V, em 5 V ou solta, o relé fica solto, e o LED do canal acende quando o relé está acionado.

## 5. Como dois relés invertem um motor (a ligação clássica)

É a ligação mais comum, e é a que tem mais chance de estar na bancada. Cada relé cuida de **um fio do
motor**:

```
          RELÉ 1                                        RELÉ 2
   NO  ●──── +12 V                               NO  ●──── +12 V
   COM ●─────────── M1 ══[ MOTOR ]══ M2 ──────────── ● COM
   NC  ●──── GND                                 NC  ●──── GND
```

- **COM** de cada relé vai a um fio do motor (M1 e M2).
- **NC** dos dois vai ao **GND** (o negativo).
- **NO** dos dois vai ao **+12 V**.

| Relé 1 | Relé 2 | M1 recebe | M2 recebe | Motor |
|---|---|---|---|---|
| solto | solto | GND | GND | **parado** |
| acionado | solto | +12 V | GND | gira num sentido |
| solto | acionado | GND | +12 V | gira no outro |
| acionado | acionado | +12 V | +12 V | **parado** |

⭐ **Por que é segura:**
- Com os dois soltos (placa desligada, em reset), os dois fios do motor ficam no GND: ele fica parado.
- O +12 V nunca encontra o GND sem passar pelo motor, em nenhuma combinação. Não há curto possível.
- Ela não precisa de um terceiro relé para parar: basta os dois relés ficarem iguais.

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

Com a alimentação do motor **desconectada** e o USB fora, siga o fio de cada borne e anote para onde ele
vai: **M1** ou **M2** (um fio do motor), **+12 V**, **GND**, ou **nada**.

| Borne | Relé do IN1 vai para | Relé do IN2 vai para |
|---|---|---|
| NO | | |
| COM | | |
| NC | | |

### 6.3 Ler o resultado

Para qualquer ligação, a regra da seção 2 diz o que o motor recebe: **solto → COM ligado a NC;
acionado → COM ligado a NO.** Monte as quatro combinações (solto/solto, acionado/solto, solto/acionado,
acionado/acionado) e confira três coisas:

1. **Com os dois soltos, o motor tem de ficar parado.** Os dois fios dele recebem a mesma coisa (os dois
   GND, ou os dois +12 V), ou ficam desligados. Se um recebe +12 V e o outro GND, **o motor gira sozinho
   com a placa desligada**: essa é a ligação perigosa.
2. **Em nenhuma combinação o +12 V pode encostar no GND sem passar pelo motor.** Isso é curto.
3. **"Só um acionado" e "só o outro acionado" giram o motor em sentidos opostos.**

Atalhos para os casos mais prováveis:

| O que a tabela mostra | Conclusão |
|---|---|
| Igual à seção 5 (COM nos fios do motor, NC no GND, NO no +12 V, nos dois relés) | **ligação clássica: segura** |
| COM nos fios do motor, mas **um NC no +12 V** e o outro no GND | **perigosa:** com os relés soltos o motor gira. Troque os fios de NO e NC desse relé |
| **+12 V ou GND no COM** e o motor nos NO/NC | outra arquitetura (a do [circuito.md](circuito.md), que depende de um relé de liga/desliga): aplique a regra das quatro combinações |

## 7. O que isso explica sobre os dois códigos

**Se** a bancada tiver a ligação clássica da seção 5:

- **Código validado** (`!codigo_base/`, relés declarados como ativos em nível alto, o contrário deste
  módulo): com o motor "parado", ele põe 0 V nos dois pinos e **aciona os dois relés**. Pela tabela da
  seção 5, acionado/acionado também é parado (os dois fios em +12 V). Para andar, ele solta um dos
  relés. **Por isso funcionou**, mesmo com a polaridade invertida. Os efeitos colaterais: as duas bobinas
  ficam energizadas o tempo todo com o motor parado (~140 mA do 5V da placa), e os nomes "avançar" e
  "recuar" ficam trocados. O pino de "habilitar" (PTD4) não aciona nada, porque não existe um terceiro
  relé.
- **v1** (nível baixo, a polaridade certa): parado = os dois soltos = os dois fios no GND. É seguro,
  mas ela foi escrita para três relés (LIGA e SENTIDO) e só aciona o pino PTD2. Na ligação clássica,
  isso move o motor **num sentido só**. Ela precisa ser adaptada a "um relé por fio do motor".
