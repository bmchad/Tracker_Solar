# Bugs de controle e matemática

Aqui o número **sai** — ele só está errado. Não há crash, não há erro de compilação, e nenhum
sintoma óbvio no log. É a categoria mais perigosa por isso.

---

## D-10 — O filtro quadrático é incoerente com o que o próprio log afirma

**Onde:** `src/main.c:98-130`.

```c
#define certeza_parametro 5
...
for(int k = 0; k < LDR_HISTORY_SIZE; k++){
    for(int j = 0; j < LDR_HISTORY_SIZE; j++){
        if(valor_ldr_e[k] > valor_ldr_d[j] + margem){ certeza_esquerda++; }
        if(valor_ldr_d[k] > valor_ldr_e[j] + margem){ certeza_direita++;  }
    }
}
...
if (certeza_esquerda >= certeza_parametro) {
    printk("Certeza absoluta: Sol na esquerda! ...");
```

Com `LDR_HISTORY_SIZE = 5`, o laço faz **25 comparações cruzadas** e o limiar é **5** — ou seja,
**20% dos pares** bastam, e o log chama isso de *"Certeza absoluta"*. Basta uma amostra da esquerda
ser maior que as cinco da direita para disparar: `5 >= 5`. O filtro que existe para evitar decisão
por ruído dispara com **um único outlier**.

E há o caso simétrico: os dois contadores podem passar do limiar **ao mesmo tempo** — basta o
histórico ter valores espalhados nos dois sensores, o que é o normal sob nuvem intermitente. O
`else if` então escolhe a esquerda, **sempre**, sem registrar que houve empate.

Comparar cada amostra de um sensor com cada amostra do outro também mistura duas coisas distintas:
a diferença espacial (o que se quer medir) e a variação temporal de cada sensor (o que se quer
filtrar). Um sensor oscilando sozinho gera "votos" nos dois lados.

**Correção:** mediana de 5 por sensor, depois **uma** comparação por eixo. Spec em
[../02-software_specs/03-controle-4-quadrantes.md](../02-software_specs/03-controle-4-quadrantes.md).

---

## D-11 — θ (ângulo de incidência) usado como posição do servo vertical

**Onde:** `src/main.c:149-151`, com `lib/angulo_incidência_solar/angulo_incidencia_solar.c:17,34`.

⭐ **É o bug que faz o produto não fazer o que promete.** Três erros empilhados:

1. **θ é função de custo, não posição.** `calcular_theta()` devolve o ângulo entre o raio solar e a
   **normal da placa** — a medida de *quanto se está errando*. Mandá-lo ao servo é corrigir o
   volante girando-o pelo tamanho do erro em vez de pela direção: não existe ponto fixo em que a
   malha assente.
2. **β está fixo dentro da função.** `static double beta = 30.0;` — a inclinação usada no cálculo é
   sempre 30°, **não** a da placa que está se movendo. A cada ciclo o firmware pergunta "quanto uma
   placa parada a 30° estaria errando?" e usa a resposta como posição da placa que ele acabou de
   mover.
3. **`gama_p` recebe o referencial errado.** O argumento é o azimute da placa, na convenção
   Sul = 0 (o próprio comentário do arquivo diz *"Sul=0, Norte=180/-180"*), e o que chega é o
   **ângulo mecânico do servo**, 0 a 180. Sem nenhuma conversão. Não existe no código o offset de
   montagem que ligaria os dois referenciais.

A fórmula da eq. 9.15 em si está **correta** — foi conferida termo a termo contra a forma padrão
(Duffie & Beckman). O erro não é de matemática, é de **o que se faz com o resultado**.

**Correção:** calcular a posição solar (elevação e azimute) e usá-la como feedforward, mantendo θ
como diagnóstico. Spec em
[../02-software_specs/04-eixo-vertical-feedforward.md](../02-software_specs/04-eixo-vertical-feedforward.md).

---

## D-12 — Clamp antes do teste, e recomando no limite

**Onde:** `src/main.c:132-142`.

```c
if (angulo >= 170) { angulo = 170; } else if (angulo <= 10) { angulo = 10; }

if (deve_mover && angulo <= 170 && angulo >= 10) {
    servo_horizontal_angulo_tempo(angulo, 2);
}
```

O clamp roda **antes**, então as duas condições seguintes são **sempre verdadeiras** — código morto
que parece proteção e não protege nada.

O efeito real é o outro: ao **bater no limite**, `deve_mover` continua verdadeiro, então o servo é
recomandado ao ângulo em que já está. Todo ciclo, para sempre, pagando os 2 s de espera bloqueante
(D-14) sem movimento nenhum. Um painel encostado no fim de curso às 17h fica assim até anoitecer.

**Correção:** saturar, e mover só se a posição mudou. Ver
[../02-software_specs/03-controle-4-quadrantes.md](../02-software_specs/03-controle-4-quadrantes.md).

---

## D-13 — O modo noturno usa a variável do horizontal nos dois eixos

**Onde:** `src/main.c:79-89`.

```c
if (hora_atual >= 18 || hora_atual < 6) {
    if (angulo != 0) {
        angulo = 0;
        servo_horizontal_angulo_tempo(angulo, 2);
        servo_vertical_angulo_tempo(angulo, 2);   /* angulo é do HORIZONTAL */
    }
```

`angulo` é a posição do eixo **horizontal**. Duas consequências:

- **a guarda consulta o eixo errado.** `if (angulo != 0)` pergunta ao horizontal se o **vertical**
  precisa recolher. Se o horizontal já estiver em 0 e o vertical estiver a 60°, o vertical fica a
  60° a noite inteira;
- `theta` **não é reiniciado**, então na primeira iteração do dia seguinte o eixo vertical parte de
  um valor calculado na véspera.

Há ainda o efeito de borda com o clamp de **D-12**: durante o dia `angulo` é saturado em no mínimo
10, então ele **nunca** é 0 e a guarda sempre passa — funciona, mas por acidente, não por desenho.

E o limiar `>= 18 || < 6` é hora de calendário fixa: está errado em qualquer época do ano fora do
equinócio. A elevação solar calculada já sabe quando o sol nasce naquela data e latitude — ver a
spec do feedforward.

---

## D-14 — Servo bloqueante torna a amostragem irregular

**Onde:** `lib/servo_motor/sevo_angulo_tenpo.c:29` e `:41`.

```c
k_msleep((int)(1000 * t));   /* t em segundos INTEIROS: 1 ou 2 */
```

A espera está **dentro** da função de servo, então comandar e esperar são inseparáveis. O laço de
controle dorme:

| Situação | Tempo de ciclo |
|---|---|
| nenhum eixo move | ~1 s (o `k_msleep(1000)` do fim do laço) |
| horizontal move | ~3 s |
| ambos + a atualização vertical | ~5,2 s |

O histórico de 5 amostras cobre então um intervalo que varia por um fator de cinco, e "mediana das
últimas 5 leituras" perde significado temporal fixo. Pior: as amostras **imediatamente após um
movimento** são as mais contaminadas (o servo ainda assentando, e o transiente de corrente
afundando a referência do ADC — **P-02**), e são justamente as que entram no histórico com o mesmo
peso das outras.

Também serializa os dois eixos, que são independentes e poderiam assentar em paralelo.

**Correção:** separar `servo_ir()` de `servo_ir_e_esperar()`. Ver
[../02-software_specs/05-camada-servo-e-calibracao.md](../02-software_specs/05-camada-servo-e-calibracao.md).

---

## D-15 — `theta` é truncado **antes** de ser arredondado

**Onde:** `src/main.c:56,149-151`.

```c
int angulo = 0, correcao = 10, theta = 0;      /* theta é int */
...
theta = calcular_theta(...);                   /* double -> int: TRUNCA */
theta = arredondar_para_multiplo_de_5(theta);  /* arredonda o que já truncou */
servo_vertical_angulo_tempo((int)theta, 2);    /* cast redundante */
```

`calcular_theta` devolve `double`, mas `theta` é `int`: a parte fracionária se perde **antes** de
`arredondar_para_multiplo_de_5()` rodar. A função que existe para arredondar recebe um valor já
degradado — 42,7 vira 42, que arredonda para 40 em vez de 45.

Baixa severidade porque o passo é de 5° e o servo tem folga maior que isso. Fica registrado porque é
o mesmo padrão de **D-03**: uma conversão implícita silenciosa desfazendo o trabalho da linha
seguinte.

---

## D-16 — Dia do ano aproximado, e **literais octais** na data

**Onde:** `lib/angulo_incidência_solar/angulo_incidencia_solar.c:11-13,79-87`.

```c
static int dia = 02;   // Formato DD, ex: 06
static int mes = 07;   // Formato MM, ex: 07
```

⛔ **`02` e `07` são literais octais.** Em C, um inteiro que começa com `0` é base 8. Aqui dá certo
por acidente — 02₈ = 2 e 07₈ = 7 — mas o comentário logo acima **instrui a escrever `06`**, e:

- `08` e `09` **não compilam**: "invalid digit in octal constant". Agosto e setembro quebram o
  build;
- se um dia alguém escrever `010` para outubro, ele compila e vale **8**.

É uma armadilha na única linha do projeto que o usuário é explicitamente instruído a editar.

Separadamente, o dia do ano é uma aproximação caseira:

```c
int n = dia + (mes - 1) * 30 + cor;
```

Meses de 30 dias mais uma correção `cor` por faixa de mês, sem ano bissexto — `ano` está declarado
e nunca é usado (**D-35**). O erro é de um a dois dias, o que desloca a declinação solar em cerca de
0,4° e a equação do tempo em alguns minutos. Tolerável para um tracker com malha fechada; **não**
tolerável se o cálculo voltar a comandar algum eixo sozinho.

---

## D-17 — O relógio não tem data, e volta a uma hora fixa a cada boot

**Onde:** `lib/init_hora/init_hora.c:11-13,38-40`.

```c
static int start_hora = 15;      /* compilado no binário */
static int start_minuto = 0;
...
hora_atual = (total_s_agora / 3600) % 24;
```

Não há RTC: o relógio é `k_uptime_get()` somado a uma hora inicial **cravada no código-fonte**.
Três consequências:

1. **Toda queda de energia volta o relógio para 15:00.** Se o tracker reiniciar às 9h, ele acha que
   são 15h — e o feedforward manda a placa para o lado errado do céu.
2. **`% 24` vira a hora à meia-noite, mas `dia`/`mes`/`ano` são `static` e nunca avançam.** Depois
   da primeira meia-noite, o cálculo solar usa o dia anterior — e continua usando para sempre.
   Operação de vários dias acumula erro de declinação sem limite.
3. **Ajustar a hora exige recompilar e regravar.** É o que torna trabalhoso o passo 4 do bringup
   (simular horas do dia).

⭐ **É este bug que justifica o feedforward ser envelope e não comando.** O cálculo astronômico é
exato; a **hora** que ele recebe não é. Um cálculo perfeito alimentado por um relógio que volta
para 15:00 a cada boot é uma fonte de erro confiante — e é por isso que quem manda no apontamento
tem de ser o sensor. Ver
[../02-software_specs/04-eixo-vertical-feedforward.md](../02-software_specs/04-eixo-vertical-feedforward.md).

---

## D-19 — `ADC_REF_INTERNAL` faz `adc_raw_to_millivolts_dt()` devolver 0 mV **com sucesso**

**Onde:** `zephyr/frdm_kl25z.overlay` (os nós de canal) mais o consumidor a ser escrito em
`lib/ldr/ldr.c`. É uma armadilha que **entra junto com a correção do D-03**, não um bug do código
atual — o código de hoje não usa a função.

⛔ **Severidade alta pelo sintoma:** os quatro sensores leem **0 mV**, os dois erros ficam sempre
zero, a zona morta nunca é excedida e **o tracker não se move** — sem erro, sem log, com todas as
chamadas retornando sucesso. Na tela é indistinguível de "está escuro".

A cadeia, verificada no framework em disco:

1. `adc_raw_to_millivolts_dt` escolhe a origem da referência pelo campo do devicetree
   (`include/zephyr/drivers/adc.h`):
   ```c
   if (channel_cfg->reference == ADC_REF_INTERNAL) {
       vref_mv = (int32_t)adc_ref_internal(spec->dev);
   } else {
       vref_mv = spec->vref_mv;   /* = DT_PROP_OR(node, zephyr_vref_mv, 0) */
   }
   ```
2. `adc_ref_internal()` devolve o campo `ref_internal` da API do driver. E
   **`adc_mcux_adc16.c` nunca preenche esse campo** — `grep ref_internal` no arquivo não retorna
   nada, enquanto dezenas de outros drivers de ADC do Zephyr preenchem. Fica **0**.
3. A conversão então faz, silenciosamente:
   ```c
   int32_t adc_mv = *valp * ref_mv;         /* ref_mv = 0  ->  0 */
   int ret = adc_gain_invert(gain, &adc_mv); /* ADC_GAIN_1 -> SUCESSO, 0 intacto */
   *valp = (adc_mv >> resolution);           /* 0 */
   return ret;                               /* 0 = SUCESSO */
   ```

**Correção:** nos **quatro** nós de canal, `zephyr,vref-mv = <3300>;` e `zephyr,reference` diferente
de `ADC_REF_INTERNAL` (`"ADC_REF_EXTERNAL0"` é honesto, porque VREFH *é* pino de referência
externa). Seguro porque `mcux_adc16_channel_setup` **ignora** `channel_cfg->reference` — valida
canal, tempo de aquisição, diferencial e ganho, e nada mais; a referência de hardware vem de Kconfig
(`ADC_MCUX_ADC16_VREF_DEFAULT` → VREFH/VREFL). Ver
[../02-software_specs/01-devicetree-adc-4-canais.md](../02-software_specs/01-devicetree-adc-4-canais.md).

⭐ **Alternativa que não pode falhar em silêncio:** `mv = (raw * 3300) / 4096` à mão. Perde o
devicetree como fonte da escala, mas é imune a esta classe de erro.

---

## D-18 — Comentário do overlay chama pino externo de canal interno

**Onde:** `zephyr/frdm_kl25z.overlay`, nos dois nós de canal.

```dts
reg = <12>; /* Reg 0 (0xc) indica o canal analógico interno */
```

Duas coisas erradas numa linha: não é "Reg 0", e `ADC0_SE12`/`ADC0_SE8` **não são canais internos** —
são os pinos externos PTB2 e PTB0. Canal interno na KL25Z é o sensor de temperatura, a banda
proibida, o VREF. Quem ler isso vai procurar o LDR no lugar errado.

Custo zero de correção, e a spec do overlay já a inclui:
[../02-software_specs/01-devicetree-adc-4-canais.md](../02-software_specs/01-devicetree-adc-4-canais.md).
