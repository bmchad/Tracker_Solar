# Bugs críticos

Os que quebram, travam ou corrompem a decisão do tracker. Índice e severidades em
[00-indice.md](00-indice.md).

---

## D-01 — `ldr_ler` devolve `-1.0` como se fosse leitura

**Onde:** `lib/ldr/ldr.c:65,77,84` (os três `return -1.0`), consumido em `src/main.c:90-91`.

```c
} else {
    printk("Lado invalido. Use 'e' ou 'd'.\n");
    return -1.0;
}
```

O erro e o valor voltam pelo **mesmo canal**, e `-1.0` é um `double` plausível de leitura baixa. O
`main.c` não checa nada:

```c
valor_ldr_d[i] = ldr_ler('d');
valor_ldr_e[i] = ldr_ler('e');
```

O `-1.0` entra no histórico e **entra na votação**. Um sensor que falha na leitura passa a "perder"
todas as comparações contra o outro, e o firmware conclui que o sol está do lado oposto — move a
placa por causa de um erro de barramento.

**Correção:** separar erro de valor — `int ldr_ler(ldr_id_t, int32_t *mv_out)`. Spec em
[../02-software_specs/02-api-ldr-4-sensores.md](../02-software_specs/02-api-ldr-4-sensores.md).

---

## D-02 — TPM0 e ADC inicializados três vezes, com `|=`

**Onde:** `src/main.c:36-38,44` · `lib/testes/testes.c:19-21,27` · `lib/testes/testes.c:63-65,71`.

As mesmas cinco chamadas aparecem copiadas em três lugares, então todo boot inicializa o hardware
três vezes.

A parte que passa de redundância a armadilha está em `lib/pwm/pwm_z42.c:31`:

```c
tpm->SC |= TPM_SC_CMOD(clock_mode) | TPM_SC_PS(ps);
```

É um **OR**, não uma escrita. Reinicializar com parâmetros **diferentes** não substitui a
configuração: **acumula bits**, e o prescaler resultante não é nenhum dos dois pedidos —
`PS_128` (7) sobre `PS_8` (3) dá 7, mas `PS_8` sobre `PS_128` também dá 7. Hoje ninguém percebe
porque os três lugares pedem exatamente os mesmos valores. É uma armadilha armada, esperando o
primeiro que mudar um deles.

**Correção:** `main()` é o único que inicializa hardware; as rotinas de teste recebem pronto.
Regra em
[../02-software_specs/00-arquitetura-e-ordem-de-execucao.md](../02-software_specs/00-arquitetura-e-ordem-de-execucao.md).

---

## D-03 — A "conversão para mV" não converte nada

**Onde:** `lib/ldr/ldr.c:88-93`.

```c
int32_t raw_value = sample_buffer[0];
millivolts = raw_value;
millivolts = millivolts * 0.889; //0.889 : valor arbitrario
return (double)millivolts;
```

Três defeitos, e o comentário do próprio autor admite o principal:

1. A variável se chama `millivolts` e carrega **contagem bruta escalada**. Ninguém converteu nada.
2. O fator é declaradamente arbitrário. A escala correta para 12 bits sobre 3,3 V é
   3300 ÷ 4095 ≈ **0,806**, não 0,889.
3. `millivolts` é `int32_t`: a multiplicação por `double` **trunca** de volta para inteiro, então
   até o fator errado é aplicado com perda.

⛔ **A consequência é a que importa:** `src/main.c:102` declara
`double margem = 20.0; // Tolerância de millivolts para ignorar ruído`. Essa margem **não está em
milivolts**. É um número adimensional, não calibrável contra nenhuma medição de multímetro, e é o
único limiar que separa "mover" de "ficar parado". Todo ajuste de anti-zig-zag feito nele foi feito
às cegas.

**Correção:** `adc_raw_to_millivolts_dt()`, que o Zephyr já oferece e usa a referência declarada no
devicetree. Ver
[../02-software_specs/02-api-ldr-4-sensores.md](../02-software_specs/02-api-ldr-4-sensores.md).

⛔ **Leia o D-19 antes de escrever essa linha.** Com o overlay como está
(`zephyr,reference = "ADC_REF_INTERNAL"`), essa função devolve **0 mV com código de sucesso** neste
driver, e o tracker para de se mexer sem reclamar. A correção do D-03 é **par indivisível** com a
correção do overlay do D-19 — aplicar uma sozinha é pior que não aplicar nenhuma.

---

## D-04 — `acos`/`asin` sem clamp de domínio: NaN, e depois UB

**Onde:** `lib/angulo_incidência_solar/angulo_incidencia_solar.c:110` e `:134`.

```c
double cos_ws = -tan(deg2rad(phi)) * tan(deg2rad(delta));
double omega_s = rad2deg(acos(cos_ws));            /* linha 110 */
...
double cos_theta = termo1 + termo2 + termo3 + termo4 + termo5;
double theta = rad2deg(acos(cos_theta));           /* linha 134 */
```

Nenhum dos dois argumentos é garantido estar em [−1, +1]:

- **linha 110:** `-tan(φ)tan(δ)` passa de 1 em módulo em latitude alta — é o caso do sol que não
  nasce ou não se põe. Não acontece em −23,5°, mas o valor é `static` e editável.
- **linha 134:** é uma **soma de cinco produtos** de senos e cossenos. Erro de arredondamento
  acumulado passa de 1,0 por uma fração de LSB perto do apontamento perfeito — exatamente onde um
  tracker que funciona vive.

`acos` fora do domínio devolve **NaN**. E então, em `src/main.c:151`:

```c
servo_vertical_angulo_tempo((int)theta, 2);
```

⛔ **Converter NaN para `int` é comportamento indefinido em C.** No ARM o resultado costuma ser 0 ou
`INT_MIN`, mas não há garantia. Combinado com **D-05** (que envolve em vez de saturar) e com a
ausência de limite de curso no eixo vertical (**P-06**), o modo de falha é **mecânico**.

**Correção:** clampar em [−1, +1] antes de todo `asin`/`acos`. Spec em
[../02-software_specs/04-eixo-vertical-feedforward.md](../02-software_specs/04-eixo-vertical-feedforward.md).

---

## D-05 — O servo **envolve** o ângulo em vez de saturar

**Onde:** `lib/servo_motor/sevo_angulo_tenpo.c:21-26` e `:33-38` (as duas funções, idênticas).

```c
while(ang>180){ ang-=180; }
while(ang<0)  { ang+=180; }
```

Pedir **185° entrega 5°**. Pedir **−10° entrega 170°**.

Uma ordem que deveria parar no fim de curso manda a placa para o **extremo oposto**, com força
total e sem aviso. Um controle incremental como este pede ângulos fora de faixa naturalmente — é o
que acontece na borda da correção — e a resposta correta a "vá para 185°" é "vou para 170° e paro",
nunca "vou para 5°".

**Correção:** saturar. Ver
[../02-software_specs/05-camada-servo-e-calibracao.md](../02-software_specs/05-camada-servo-e-calibracao.md).

---

## D-06 — 12 min 40 s de teste bloqueante em todo boot

**Onde:** `src/main.c:69-71`, com `NUMERO_DE_TESTES 10` em `lib/testes/testes.c:14`.

```c
//teste motores
teste_motores();
//teste ldr
teste_ldr();
```

Sem condição nenhuma. `teste_motores()` faz 10 × 4 varreduras × 19 passos = **760** comandos de
servo, cada um com `k_msleep(1000)` dentro (D-14). São **12 minutos e 40 segundos** antes de a
primeira leitura de tracking acontecer, mais ~5 s de `teste_ldr()`.

Não é incômodo de desenvolvimento: um reset ao meio-dia custa 12 minutos de sol, e nesse intervalo
a placa está varrendo de 0 a 180 repetidamente, sem relação alguma com onde o sol está.

**Correção:** `#ifdef RODAR_TESTES_NO_BOOT` com a flag em `platformio.ini`, e
`NUMERO_DE_TESTES` de 10 para 1. Ver
[../02-software_specs/06-bringup-e-testes.md](../02-software_specs/06-bringup-e-testes.md).

---

## D-07 — `testes.c` chama as funções de servo sem incluir o header

**Onde:** `lib/testes/testes.c` — inclui `pwm_z42.h`, `ldr.h`, `init_hora.h`,
`angulo_incidencia_solar.h` e `testes.h`, mas **não** `servo_angulo_tempo.h`. E chama
`servo_horizontal_angulo_tempo()` e `servo_vertical_angulo_tempo()` nas linhas 40, 44, 49 e 53.

Declaração implícita de função: o compilador assume `int f()` com argumentos promovidos. Aqui passa
por sorte — os dois parâmetros são `int` e o retorno é ignorado. Se algum dia um deles virar
`double` (e `t`, que é tempo, é o candidato natural), a chamada passa os bits errados **sem erro de
compilação**.

Removida do C23, e já `-Werror=implicit-function-declaration` por padrão em compiladores recentes:
é o tipo de coisa que quebra o build numa atualização de toolchain, não numa mudança de código.

**Correção:** incluir o header. Ver
[../02-software_specs/06-bringup-e-testes.md](../02-software_specs/06-bringup-e-testes.md).
