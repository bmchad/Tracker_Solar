# Cálculo do ângulo da placa: potenciômetro e acelerômetro

Responde ao pedido de [prompt.md](prompt.md) com as especificações de [info.md](info.md):

- como medir o ângulo **horizontal** com potenciômetro, incluindo a especificação do potenciômetro;
- duas maneiras de medir o ângulo **vertical**: por potenciômetro e por acelerômetro (vetor
  gravidade).

Este documento cobre **só a medição do ângulo.** Pontos de outras áreas que apareceram no caminho
estão listados, sem planejamento, na seção 10.

---

## 0. Premissas

| Item | Valor | Origem |
|---|---|---|
| Placa | ZTROON 60 W (ZTP-060MI), 715 × 445 × 25 mm | info.md |
| Motor | motor de vidro elétrico Mabuchi, 12 V, fio preto | info.md |
| Transmissão | corrente, relação **20:1** (o eixo da placa gira 1/20 do eixo do motor) | info.md |
| Eixos motorizados | **um só**, o vertical. Não há movimento horizontal | info.md |
| Nomes | **vertical = inclinação** (rotação em torno de um eixo horizontal); **horizontal = azimute** (rotação em torno de um eixo vertical) | convenção deste documento |
| Microcontrolador / ADC | **não definidos** para o projeto real. Os exemplos usam ADC de **12 bits com referência de 3,3 V**; a seção 3.4 mostra que 10 bits também bastam | — |

**Velocidade do eixo da placa:**

```
ω_eixo [°/s] = rpm_motor × 360 / 60 / 20 = 0,3 × rpm_motor
```

⚠️ **A rpm do motor não é conhecida.** Não existe ficha técnica confiável para "motor de vidro Mabuchi
fio preto". A única fonte encontrada fala em ~92 rpm sem carga, mas os números dela são incoerentes
entre si. Os exemplos abaixo usam **80 rpm → 24 °/s no eixo**, que é só um chute de ordem de
grandeza. **Meça:** marque o eixo do motor, alimente com 12 V por 10 s e conte as voltas.

⭐ Motor de vidro elétrico tem redução interna por **rosca sem-fim**, que na prática não volta
sozinha: com o motor desligado, a placa fica onde parou. Isso importa para o acelerômetro (seção 6),
que lê melhor com a placa parada.

---

## 1. Quanta precisão o ângulo precisa

O erro de apontamento custa energia como 1 − cos(erro):

| Erro | 1° | 2° | 3° | 5° | 10° | 15° | 25° |
|---|---|---|---|---|---|---|---|
| Perda | 0,02 % | 0,06 % | 0,14 % | 0,38 % | 1,5 % | 3,4 % | 9,4 % |

⭐ **±2° é mais do que suficiente.** A escolha entre potenciômetro e acelerômetro é uma questão de
**robustez** (desgaste, vento, cabo, montagem), não de resolução. Qualquer um dos dois, bem feito,
passa folgado de ±2°.

---

## 2. Potenciômetro: onde montar e qual comprar

### 2.1 Onde montar

| Onde | Giro do potenciômetro (curso de 120° na placa) | Veredito |
|---|---|---|
| ⭐ **No eixo da placa, 1:1, na ponta do eixo** | 120° | **recomendado**: mede a placa de verdade, depois da corrente |
| No eixo do motor (antes da corrente 20:1) | 2400° ≈ 6,7 voltas → exige potenciômetro de 10 voltas | **não**: mede o motor, não a placa; folga, estiramento e dente pulado da corrente viram erro invisível |
| No eixo da placa, com engrenagem multiplicando (ex.: 1:2,5) | ~300°, usa a escala toda | **não**: ganha resolução que não faz falta (seção 1) e acrescenta folga de engrenagem |

⭐ No eixo da placa, o **corpo do potenciômetro fica preso na estrutura fixa** e só o eixo dele
gira. O cabo, portanto, **não se mexe**, o que é uma vantagem real sobre o acelerômetro (seção 6.6).

### 2.2 Especificação

| Parâmetro | Especificação | Por quê |
|---|---|---|
| Tipo | rotativo, **1 volta** | o curso da placa cabe com folga em ~300° |
| Curva | **linear** ("B", ex.: B5K). **Nunca logarítmica** ("A") | a conversão da seção 3 assume reta |
| Resistência | **5 kΩ** (faixa boa: 5–10 kΩ; 15 kΩ ainda funciona) | o que pesa é a impedância vista pelo ADC no pior ponto (cursor no meio), **R/4**: 5 kΩ → 1,25 kΩ; 10 kΩ → 2,5 kΩ; 15 kΩ → 3,75 kΩ, já perto do limite típico de alguns kΩ dos ADCs de microcontrolador (confira a "resistência máxima da fonte analógica" no datasheet do que for usado). O 100 nF da seção 2.3 cobre os três, mas 5 kΩ dá mais margem, capta menos ruído no cabo e sofre metade da carga do pull-down. Consumo de 5 kΩ em 3,3 V: 0,66 mA e 2,2 mW, desprezível. Com 100 kΩ haveria erro de amostragem e muito mais captação de ruído |
| Ângulo elétrico | ~270–300° (tolerância típica de alguns graus) | a tolerância não importa, porque a calibração da seção 3.2 absorve |
| Linearidade | carbono ±1–2 % (= ±3–6° em 300°); **plástico condutivo ≤ ±0,5 %** | carbono só serve com calibração de vários pontos (3.3) |
| Vida rotacional | carbono: dezenas de milhares de ciclos; **plástico condutivo: milhões** | o tracker faz ~1 ciclo completo por dia mais dezenas de micro-correções, sempre nos mesmos trechos da pista |
| Batente mecânico | ⚠️ preferir **sem batente** (giro livre). Se tiver, a faixa usada fica **centrada** e longe dos batentes | o torque do eixo 20:1 arranca o batente na hora |
| Eixo | 6 mm, ligado ao eixo da placa por **acoplamento flexível** (tipo garra/aranha ou mola) | potenciômetro tem bucha, não rolamento: carga radial de desalinhamento acaba com ele |
| Ambiente | vedado (IP65) **ou** dentro de caixa vedada com retentor no eixo | o potenciômetro comum de painel não é vedado: poeira e umidade no cursor geram leitura ruidosa e circuito aberto |

⭐ **Alternativa que vale considerar: sensor magnético de ângulo (ex.: AS5600).** É um ímã
diametral colado na ponta do eixo mais um CI a 0,5–3 mm dele, **sem contato nenhum**: sem desgaste,
sem batente, 360°, 12 bits. Ele tem saída **analógica** que entra no mesmo ADC e no mesmo código da
seção 3, e pode ser configurado para a faixa **10–90 %**, de modo que fio rompido cai fora da faixa
e é detectado sozinho. É um "potenciômetro que não gasta", com o mesmo custo de módulo barato, e
resolve as três últimas linhas da tabela de uma vez.

### 2.3 Circuito

```
 Vref do ADC (ex.: 3,3 V) ───────────────────┬── extremidade 1
                                             │
                               [ potenciômetro  5 kΩ ]──cursor──[1 kΩ]──┬──── pino ADC
                                             │                         │
 GND analógico ──────────────────────────────┴── extremidade 2   100 nF ═╡   1 MΩ ═╡  (pull-down)
                                                                       │         │
                                                                      GND       GND
                         (cabo blindado de 3 vias; malha aterrada SÓ no lado da placa)
```

- ⭐ **Ratiométrico:** alimentado pela mesma tensão que serve de referência ao ADC, o potenciômetro
  dá uma leitura que é **razão**, e oscilações dessa tensão se cancelam. ⛔ Não alimente o
  potenciômetro com 5 V ou 12 V se o ADC usa 3,3 V de referência.
- **1 kΩ + 100 nF** perto do microcontrolador: filtro passa-baixa (~1,6 kHz) contra o ruído do
  motor escovado e reservatório de carga para o capacitor de amostragem do ADC. O 1 kΩ também
  protege o pino.
- **1 MΩ de pull-down no cursor:** se o cursor abrir (o modo de falha clássico), a leitura vai a
  ~0 em vez de flutuar, o que torna a falha **detectável** (seção 3.5). O efeito de carga do 1 MΩ
  é ≤ 0,22° no pior ponto com 5 kΩ (0,67° com 15 kΩ), sistemático e absorvido pela calibração.
- Cabo do sensor longe dos fios do motor, sem trechos longos em paralelo: a borda de chaveamento da
  ponte H se acopla capacitivamente em fio analógico.

---

## 3. Do ADC ao ângulo: a conta do potenciômetro (vale para qualquer eixo)

### 3.1 O modelo ideal

```
razão = N / N_max                        N = leitura do ADC; N_max = 4095 em 12 bits
θ_pot = razão × Θ_elétrico               Θ_elétrico ≈ 300°
θ_placa = (θ_pot − θ_zero) × s           s = +1 ou −1 (sentido de montagem)
```

Na prática **não use este modelo com números de catálogo.** Θ_elétrico tem tolerância de alguns
graus, θ_zero depende de como o acoplamento foi apertado e s depende do lado em que o potenciômetro
foi montado. São três incógnitas, e a calibração mede todas de uma vez.

### 3.2 Calibração de 2 pontos: o método que vale

Leve a placa a dois ângulos conhecidos, medidos com transferidor ou inclinômetro (o aplicativo de
nível do celular serve, com ±0,5° realista), e anote a leitura do ADC em cada um:

| Ponto | Ângulo da placa | Leitura |
|---|---|---|
| A | θ_A (ex.: 0°, placa nivelada) | N_A |
| B | θ_B (ex.: 60°) | N_B |

```
θ = θ_A + (N − N_A) × (θ_B − θ_A) / (N_B − N_A)
```

Isso absorve Θ_elétrico, θ_zero, a tolerância da resistência **e o sinal**: se o ângulo cresce
enquanto N diminui, N_B < N_A e a inclinação da reta sai negativa sozinha. Escolha A e B **perto das
pontas do curso**: quanto mais afastados, menos o erro de leitura do transferidor pesa na
inclinação da reta.

### 3.3 Calibração de vários pontos (potenciômetro de carbono)

Com ±2 % de linearidade, a reta de 2 pontos pode errar alguns graus no meio do curso. Levante a
curva a cada 10° (0°, 10°, …, 60°), guarde a tabela e interpole linearmente entre os dois pontos
vizinhos. O erro de linearidade é **sistemático**, então a tabela o remove. Com plástico condutivo
ou AS5600, a reta de 2 pontos já basta.

### 3.4 Resolução

| ADC | Graus por contagem (potenciômetro de 300° no eixo 1:1) | Contagens num curso de 120° |
|---|---|---|
| 12 bits | **0,073°** | ~1640 |
| 10 bits | 0,29° | ~410 |

Os dois sobram para a precisão de ±2° da seção 1. Resolução não é o gargalo.

### 3.5 Código (só inteiros)

Usa só inteiros, em **décimos de grau**, para rodar igual em microcontrolador sem FPU. O
Cortex-M0+ do protótipo, por exemplo, não tem FPU.

```c
#include <stdbool.h>
#include <stdint.h>

/* Calibracao de 2 pontos (secao 3.2). Numeros de EXEMPLO: troque pelos medidos. */
#define POT_N_A            1210   /* leitura do ADC com a placa em theta_A */
#define POT_D10_A             0   /* theta_A = 0,0 graus                   */
#define POT_N_B            2440   /* leitura do ADC com a placa em theta_B */
#define POT_D10_B           600   /* theta_B = 60,0 graus                  */

/* Faixa plausivel. Fora dela: cursor aberto (~0, pelo pull-down de 1 MOhm),
   GND rompido (~4095) ou potenciometro fora do curso montado. */
#define POT_N_MIN_VALIDO    300
#define POT_N_MAX_VALIDO   3800

/* n: MEDIANA de 5 leituras do ADC (corta os picos de ruido do motor).
   Devolve false se a leitura e falha: o angulo nao e confiavel. */
bool pot_angulo_d10(int32_t n, int32_t *d10)
{
    if (n < POT_N_MIN_VALIDO || n > POT_N_MAX_VALIDO) {
        return false;
    }
    /* (n - N_A) <= 4095 e (D10_B - D10_A) <= 1800: o produto cabe em int32 */
    *d10 = POT_D10_A
         + ((n - POT_N_A) * (POT_D10_B - POT_D10_A)) / (POT_N_B - POT_N_A);
    return true;
}
```

⭐ Diferente do acelerômetro, o potenciômetro **pode ser lido com o motor girando**. A 24 °/s, uma
leitura a cada 10 ms vê a placa andar 0,24° entre amostras.

### 3.6 Orçamento de erro (potenciômetro no eixo 1:1)

| Fonte | Ordem de grandeza | Observação |
|---|---|---|
| Referência da calibração (transferidor/celular) | ±0,5° | domina o resultado |
| Linearidade residual | ±0,5° (vários pontos) / até ±3° (carbono com 2 pontos) | |
| Folga do acoplamento flexível | ±0,2° | |
| Carga do pull-down de 1 MΩ | ≤ 0,22° com 5 kΩ, sistemático | some na calibração |
| Temperatura | desprezível | a leitura é razão de divisor: a deriva da resistência da pista se cancela |
| Ruído do ADC com o motor ligado | poucas contagens ≈ ±0,3° | mediana resolve |
| **Total realista** | **±1–2°** | dentro do que a seção 1 pede |

---

## 4. Ângulo horizontal (azimute) com potenciômetro

⚠️ **O info.md diz que não haverá movimento horizontal no projeto real.** Sem eixo horizontal
motorizado, não existe ângulo horizontal variável para medir: o azimute da estrutura é fixado uma
vez, na instalação, com bússola. Esta seção fica como referência, caso um segundo motor apareça.

**A conta é a da seção 3, sem mudar nada.** O que muda é a montagem e a referência:

- **Montagem:** potenciômetro coaxial ao eixo vertical da base giratória, com o corpo na base fixa.
  Valem a especificação e o circuito da seção 2.
- **Zero:** o ângulo 0 tem de ser um rumo geográfico (norte ou sul **verdadeiro**), fixado com
  bússola **corrigida pela declinação magnética**. Em São Paulo a agulha aponta cerca de 22° a oeste
  do norte verdadeiro (valor exato do local e do ano no calculador da NOAA), ou seja, o norte
  verdadeiro fica ~22° **à direita** de onde a agulha aponta. Sem essa correção, o azimute nasce
  errado por 22°.
- **Curso necessário:** em SP o sol nasce entre 61° (junho) e 113° (dezembro) e se põe entre 247° e
  299° (rumos a partir do norte, sentido horário), sempre passando pelo **norte**. Isso dá **~226°
  de curso**. Num potenciômetro de 300° sobram só ~37° de cada lado, o que é apertado demais se ele
  tiver batente. Aqui o **AS5600 (360°, sem batente)** deixa de ser alternativa e vira a escolha
  certa.
- ⚠️ **No verão, perto do meio-dia, o sol passa a menos de 1° do zênite em SP** (latitude ≈ trópico).
  O azimute dá um salto de ~180° em poucos minutos. Um sensor de azimute precisa acompanhar esse
  salto sem estourar a faixa.
- **Por que só potenciômetro (ou encoder) para o azimute:** o acelerômetro **não enxerga** rotação
  em torno do eixo vertical, porque girar em volta da gravidade não muda a projeção da gravidade em
  nenhum eixo do sensor. A outra opção seria um magnetômetro (bússola eletrônica), mas ele fica ao
  lado de uma estrutura de aço, dos ímãs do motor e da corrente do motor, e é distorcido pelos três.

---

## 5. Ângulo vertical, maneira 1: potenciômetro

**É a seção 3 aplicada ao eixo de inclinação.** O que é específico deste eixo:

- **Zero = placa nivelada**, conferido com nível de bolha **depois** de a estrutura estar montada no
  local definitivo.
- ⚠️ **O potenciômetro mede o ângulo RELATIVO à estrutura**, não ao horizonte. Se a estrutura estiver
  2° desnivelada e a calibração tiver sido feita na bancada, o erro é de 2°. Calibre **no local**,
  com a estrutura já fixada.
- **Curso:** ainda não definido pela mecânica. Os exemplos usam 120°. Qualquer curso até ~240° cabe
  num potenciômetro de 300° montado centrado.

---

## 6. Ângulo vertical, maneira 2: acelerômetro (vetor gravidade)

### 6.1 Princípio

Parado, o acelerômetro mede só a gravidade: um vetor de 1 g apontando para cima no referencial dele.
Preso à placa, ele gira junto, e a gravidade **muda de componente** entre os eixos do sensor
conforme a placa inclina. O ângulo sai da direção desse vetor.

⭐ É uma medida **absoluta**: o ângulo em relação ao horizonte verdadeiro, sem depender de como a
estrutura foi nivelada.

### 6.2 Montagem e fórmula

Fixe o sensor na **parte que gira** (o quadro ou o eixo da placa) com:

- **Y** do sensor paralelo ao eixo de rotação;
- **X** no plano da placa, perpendicular ao eixo;
- **Z** normal à placa.

```
β = atan2(a_x, a_z)
```

Com a placa nivelada, a_z = +1 g e a_x = 0, logo β = 0. Quando a borda +X do sensor sobe, a_x
cresce e β fica positivo. Se a montagem inverter o sentido, troque o sinal de a_x.

**Por que atan2 de dois eixos, e não as fórmulas comuns de tutorial:**

| Alternativa | Problema |
|---|---|
| `asin(a_x / 1g)` | depende de a escala do sensor estar certa, e o erro é amplificado perto de 90°: a 70° o ruído é multiplicado por 2,9 |
| `atan2(a_x, sqrt(a_y² + a_z²))` ("pitch" de tutorial) | mede outra coisa: o ângulo do eixo X com o horizonte. Se o eixo de rotação estiver 5° fora do nível, erra ~0,4° a 60° |
| ⭐ `atan2(a_x, a_z)` | **é exatamente a rotação em torno de Y**, mesmo com o eixo de rotação fora do nível. O módulo de g e uma escala comum aos dois eixos **se cancelam**. A sensibilidade é a mesma em qualquer ângulo |

Montagem torta também pesa pouco: se X estiver girado ψ = 5° no plano da placa em relação à
perpendicular do eixo, o erro a 60° é de 0,1°.

### 6.3 Fontes de erro

| Fonte | Tamanho | O que fazer |
|---|---|---|
| **Offset de fábrica** (zero-g) | erro ≈ atan(offset): **17 mg → 1,0°; 50 mg → 2,9°**. Módulos baratos ficam na casa das dezenas de mg (confira o datasheet do que for comprado) | calibração de bancada (6.4) |
| **Deriva com temperatura** | o offset muda alguns mg a dezenas de mg na faixa de temperatura de operação | ⚠️ **não cole o sensor no fundo da placa**, que passa de 60 °C ao sol: monte no quadro, à sombra, em caixa |
| **Aceleração dinâmica**: vento, partida e parada do motor, vibração da corrente | arbitrário enquanto dura | ler **com o motor parado** (~0,5 s após parar), média de ~1 s, e o **filtro de módulo**: se \|a\| fugir de 1 g ± 5 %, a leitura é descartada |
| Aceleração centrípeta durante o giro | atan(ω²r/g): 24 °/s com o sensor a 30 cm do eixo → 0,3° | só existe em movimento, e a leitura é feita parada |
| Desalinhamento de montagem | ψ = 5° → 0,1° | desprezível; o offset de montagem sai no passo 2 de 6.4 |

### 6.4 Calibração

1. **Na bancada, antes de montar (offset e escala de X e Z).** Sobre mesa nivelada, encostado num
   esquadro, faça a média de ~2 s em quatro posições: +X para cima, −X para cima, +Z para cima e
   −Z para cima.
   ```
   offset_x = (x₊ + x₋) / 2        escala_x = (x₊ − x₋) / 2
   offset_z = (z₊ + z₋) / 2        escala_z = (z₊ − z₋) / 2
   ```
2. **Na estrutura (offset de montagem).** Com a placa nivelada (nível de bolha), meça
   β_montagem = atan2(x, z) com os valores já corrigidos, e subtraia esse valor de toda leitura
   futura.

### 6.5 Código

```c
#include <math.h>
#include <stdbool.h>

/* Calibracao (secao 6.4). Numeros de EXEMPLO. */
static const float OFFSET_X = 0.012f, ESCALA_X = 1.003f;   /* em g */
static const float OFFSET_Z = -0.020f, ESCALA_Z = 0.998f;
static const float BETA_MONTAGEM_GRAUS = 1.8f;

/* ax, ay, az em g: MEDIA de ~1 s, com o motor parado ha >= 0,5 s.
   Devolve false se havia aceleracao alem da gravidade (vento, vibracao). */
bool acel_angulo_graus(float ax, float ay, float az, float *beta)
{
    float x = (ax - OFFSET_X) / ESCALA_X;
    float z = (az - OFFSET_Z) / ESCALA_Z;
    float modulo = sqrtf(x * x + ay * ay + z * z);

    if (modulo < 0.95f || modulo > 1.05f) {
        return false;
    }
    *beta = atan2f(x, z) * (180.0f / 3.14159265f) - BETA_MONTAGEM_GRAUS;
    return true;
}
```

Sem FPU, `atan2f` em soft-float custa da ordem de dezenas a centenas de µs. Lendo poucas vezes por
segundo, isso é irrelevante.

### 6.6 Qual sensor, e o problema do cabo

| Opção | Interface | Observação |
|---|---|---|
| Módulos I²C (MPU-6050, ADXL345, LIS3DH…) | I²C | baratos e comuns; confira offset e deriva no datasheet |
| ADXL335 (módulo GY-61) | **analógico**, ratiométrico | três fios no ADC, como o potenciômetro, sem I²C no cabo |
| ⭐ MMA8451Q da FRDM-KL25Z **do protótipo** | I²C0, endereço 0x1D | já está soldado na placa do protótipo e declarado no devicetree do Zephyr (`accel0`, compatible `nxp,fxos8700`; `CONFIG_SENSOR=y` e `CONFIG_FXOS8700_MODE_ACCEL=y`). **Serve para o experimento de bancada da seção 8 sem comprar nada**, qualquer que seja o microcontrolador do projeto real |

⚠️ **O sensor fica na parte que gira, e o cabo dele flexiona a cada movimento.** Com movimento lento
e curso limitado, uma alça de folga presa nas duas pontas resolve, mas é um ponto de fadiga que o
potenciômetro não tem (2.1).

⚠️ **I²C foi feito para trilha de placa, não para cabo.** Um a dois metros funcionam com pull-ups
fortes (2,2 kΩ) e 100 kHz. Ao lado de um motor escovado chaveado por ponte H, é aposta. Por ordem de
preferência:
1. o microcontrolador perto do sensor (eletrônica na parte que gira);
2. sensor analógico (ADXL335) com cabo blindado;
3. extensor de barramento I²C.

---

## 7. Comparação e recomendação

| Critério | Potenciômetro / AS5600 no eixo | Acelerômetro |
|---|---|---|
| O que mede | rotação **relativa** à estrutura | inclinação **absoluta** em relação ao horizonte |
| Estrutura desnivelada | erra pelo desnível, se calibrado fora do local | imune |
| Leitura com o motor girando | ✅ contínua | ❌ só parado (ou grosseira em velocidade constante) |
| Vento | imune | precisa de média e filtro de módulo |
| Montagem mecânica | acoplamento coaxial na ponta do eixo, alinhamento | parafusar no quadro |
| Desgaste | potenciômetro: contato que desliza / AS5600: nenhum | nenhum |
| Cabo | estático | flexiona a cada movimento |
| Interface | 1 canal de ADC | I²C (cabo curto) ou analógico |
| Temperatura | praticamente imune (ratiométrico) | deriva de offset: montar à sombra |
| Falha | detectável por faixa (pull-down / 10–90 %) | detectável (NACK, módulo ≠ 1 g); congelado não |
| Serve para o eixo horizontal? | ✅ | ❌ |

⭐ **Recomendação para o eixo vertical:**

1. **Acelerômetro como sensor principal.** O tracker passa quase todo o tempo parado, o sol é
   lento e a precisão necessária é folgada (±2°). Com isso, a única desvantagem séria do
   acelerômetro, que é só ler bem parado, quase não custa nada: mova, pare, leia, corrija. Em troca
   vêm medida absoluta, nenhuma peça móvel, nenhum acoplamento para usinar e caixa totalmente
   vedada. Trackers comerciais de um eixo costumam usar inclinômetro (acelerômetro) no tubo de
   torque pelos mesmos motivos.
2. **Potenciômetro (de preferência AS5600) como segunda medida, se o orçamento permitir:** dá a
   leitura durante o movimento e, comparado ao acelerômetro, funciona como **teste de
   plausibilidade**. Se os dois discordarem por mais de ~3° com o acelerômetro válido e parado, o
   acoplamento patinou ou um sensor falhou.

O potenciômetro passa à frente do acelerômetro se a placa for ficar sob **vento constante** (as
leituras do acelerômetro seriam descartadas o tempo todo) ou se for preciso medir o ângulo
**durante** o movimento. O experimento da seção 8 decide com dados.

---

## 8. Experimento de bancada

Com um transferidor e um potenciômetro de 5 kΩ do laboratório, compara os dois métodos lado a lado
sem comprar nada:

1. Uma dobradiça com o transferidor no pino. O **potenciômetro de 5 kΩ** fica coaxial ao pino, com o corpo na parte
   fixa. A **FRDM-KL25Z do protótipo** fica presa na parte que gira, para usar o MMA8451Q dela.
2. De 0° a 90°, em passos de 10°: ler o ADC do potenciômetro (mediana de 5) e o atan2 do
   acelerômetro (média de 1 s), e anotar ao lado do transferidor.
3. Calibrar o potenciômetro com 2 pontos (0° e 90°) e o acelerômetro com o passo 2 de 6.4 (0°).
   Calcular o erro de cada método em cada ponto.
4. Repetir **dando batidinhas na dobradiça** enquanto lê, para ver o que vibração faz com cada um
   e com que frequência o filtro de módulo descarta leituras.

As saídas são a linearidade real do potenciômetro barato, o offset real do acelerômetro e o
comportamento sob vibração: os três números que fecham a seção 7 com medida em vez de argumento.

---

## 9. O que falta medir ou decidir (para o ângulo)

Cada item tem uma cópia com contexto em [duvidas.md](duvidas.md) (DV-11 a DV-15). Ao resolver um,
atualize os dois lugares.

| # | O quê | Por que importa para o ângulo |
|---|---|---|
| 1 | rpm do motor na saída, com carga | define ω_eixo, a taxa de leitura do potenciômetro e quanto a placa anda depois de cortar o motor |
| 2 | curso mecânico do eixo vertical | define a faixa válida do sensor (3.5) e se cabe num potenciômetro com batente |
| 3 | microcontrolador e ADC do projeto real | resolução, referência e impedância máxima de fonte (2.2, 3.4) |
| 4 | onde fica a eletrônica (parte fixa ou parte que gira) | decide qual cabo flexiona e se dá para usar I²C (6.6) |
| 5 | resultado do experimento da seção 8 | fecha a escolha entre os métodos |

---

## 10. Fora do escopo: registrado, não planejado

Apareceram fazendo as contas acima. Não são cálculo de ângulo, ficam aqui só para não se perderem.

- **Orientação do eixo no chão.** Com um motor só, o eixo instalado na direção **norte–sul** (placa
  inclinando para leste/oeste) rende cerca de **+29 %** sobre placa fixa; na direção
  **leste–oeste** (inclinando para norte/sul), cerca de **+8 %**. Modelo de céu limpo, só radiação
  direta, latitude −23,55°: os valores absolutos são otimistas, a diferença entre os dois é o que
  importa. Nenhuma conta deste documento muda com a orientação.
- **Ângulo-alvo com um eixo só.** A fórmula β = 90° − α (α = elevação solar) só vale com dois eixos.
  Com um eixo, o alvo é a projeção do vetor solar no plano de giro: `atan2(s_N, s_C)` para o eixo
  leste–oeste e `atan2(s_L, s_C)` para o norte–sul.
- **Fim de curso por hardware.** Com o torque de um motor de vidro passando por 20:1, uma falha do
  sensor de ângulo (fio rompido, acoplamento patinando) deixa o motor girando até quebrar algo.
  Chaves de fim de curso cortando o motor por hardware são necessárias qualquer que seja o sensor.
