# Bugs críticos — ✅ os sete corrigidos em 2026-09-02

Os que quebram, travam ou corrompem a decisão do tracker. Índice e severidades em
[00-indice.md](00-indice.md).

> ⭐ **Estado: D-01 a D-07 corrigidos.** O diagnóstico de cada um fica preservado abaixo — ele é o
> que explica *por que* a correção é essa, e impede que alguém "simplifique" de volta. Cada item
> termina com **O que foi feito**.
>
> ⚠️ **Corrigido significa "escrito e verificado por análise", não "testado no hardware".** O que
> foi e o que não foi verificado está na seção **Verificação**, no fim.
>
> ⛔ **O D-19 entrou junto**, apesar de morar em
> [02-bugs-de-controle-e-matematica.md](02-bugs-de-controle-e-matematica.md): ele é **par
> indivisível** com o D-03, e aplicar um sem o outro deixaria os sensores lendo 0 mV.

---

## D-01 — `ldr_ler` devolvia `-1.0` como se fosse leitura ✅

**Onde era:** `lib/ldr/ldr.c:65,77,84` (os três `return -1.0`), consumido em `src/main.c:90-91`.

O erro e o valor voltavam pelo **mesmo canal**, e `-1.0` é um `double` plausível de leitura baixa. O
`main.c` não checava nada — atribuía o retorno direto ao histórico.

O `-1.0` entrava no histórico e **entrava na votação**. Um sensor que falhasse na leitura passava a
"perder" todas as comparações contra o outro, e o firmware concluía que o sol estava do lado oposto
— movia a placa por causa de um erro de barramento.

**O que foi feito.** A assinatura passou a separar os dois canais:

```c
int ldr_ler(char lado, int32_t *mv_out);   /* 0 em sucesso, -errno em falha */
```

Em erro devolve `-errno` e **não toca** em `*mv_out`. No `main.c`, falha de qualquer um dos dois
sensores **descarta o ciclo inteiro** (`continue`) em vez de contaminar o histórico.

⚠️ O parâmetro continua sendo `char lado`, porque ainda há **dois** sensores. Ele vira `ldr_id_t`
no upgrade de 4 LDRs — ver
[../02-software_specs/02-api-ldr-4-sensores.md](../02-software_specs/02-api-ldr-4-sensores.md). O
defeito que o D-01 descreve — erro indistinguível de valor — está fechado independentemente disso.

---

## D-02 — TPM0 e ADC inicializados três vezes, com `|=` ✅

**Onde era:** `src/main.c:36-38,44` · `lib/testes/testes.c:19-21,27` · `lib/testes/testes.c:63-65,71`.

As mesmas cinco chamadas apareciam copiadas em três lugares, então todo boot inicializava o
hardware três vezes.

A parte que passava de redundância a armadilha está em `lib/pwm/pwm_z42.c:31`:

```c
tpm->SC |= TPM_SC_CMOD(clock_mode) | TPM_SC_PS(ps);
```

É um **OR**, não uma escrita. Reinicializar com parâmetros **diferentes** não substitui a
configuração: **acumula bits**, e o prescaler resultante não é nenhum dos dois pedidos.

**O que foi feito.** `teste_motores()` e `teste_ldr()` não inicializam mais nada — recebem o
hardware pronto, e `main()` é o único lugar que chama `pwm_tpm_Init` / `pwm_tpm_Ch_Init` /
`ldr_inicializar`. A regra está em
[../02-software_specs/00-arquitetura-e-ordem-de-execucao.md](../02-software_specs/00-arquitetura-e-ordem-de-execucao.md),
e o `testes.h` a repete no topo, para quem for mexer lá.

⚠️ **O `|=` do `pwm_z42.c` continua como está**, de propósito — **DEC-5**, não mexer na biblioteca
de origem terceira. A armadilha foi **desarmada** (não há mais reinicialização), não consertada. Se
alguém reintroduzir um segundo `pwm_tpm_Init` com parâmetros diferentes, ela volta.

---

## D-03 — A "conversão para mV" não convertia nada ✅

**Onde era:** `lib/ldr/ldr.c:88-93`, na linha `millivolts = millivolts * 0.889;`.

Três defeitos, e o comentário do próprio autor admitia o principal:

1. A variável se chamava `millivolts` e carregava **contagem bruta escalada**. Ninguém convertia
   nada.
2. O fator era declaradamente arbitrário. A escala correta para 12 bits sobre 3,3 V é
   3300 ÷ 4095 ≈ **0,806**, não 0,889.
3. `millivolts` era `int32_t`: a multiplicação por `double` **truncava** de volta para inteiro.

⛔ **A consequência era a que importa:** a `margem` do `main.c`, comentada como "tolerância de
millivolts para ignorar ruído", **não estava em milivolts**. Era um número adimensional, não
calibrável contra nenhuma medição de multímetro, e era o único limiar que separava "mover" de
"ficar parado".

**O que foi feito.** `adc_raw_to_millivolts_dt()`, que usa a referência e o ganho declarados no
devicetree. O comentário da `margem` no `main.c` passou a registrar que agora ela **é** mV de
verdade — o valor `20.0` em si continua sendo chute até as medições do passo 1 do bringup (**P-04**
deu a faixa, 10²–10³ mV, mas não o piso de ruído, que é o que este limiar tem de superar).

⛔ **Não dava para fazer sozinho** — ver D-19, logo abaixo.

---

## D-19 — o overlay, par indivisível do D-03 ✅

Mora em [02-bugs-de-controle-e-matematica.md](02-bugs-de-controle-e-matematica.md), mas foi
corrigido nesta mesma leva porque não faz sentido separá-los.

`adc_raw_to_millivolts_dt()` com `zephyr,reference = "ADC_REF_INTERNAL"` devolvia **0 mV com código
de sucesso** neste driver, porque `adc_mcux_adc16.c` nunca preenche `ref_internal`. Aplicar só o
D-03 teria deixado os sensores lendo zero, os erros sempre nulos e o tracker parado **sem uma linha
de log** — indistinguível de "está escuro".

**O que foi feito.** Nos dois nós de canal do `zephyr/frdm_kl25z.overlay`:
`zephyr,reference = "ADC_REF_EXTERNAL0"` e `zephyr,vref-mv = <3300>`. Seguro porque
`mcux_adc16_channel_setup` **ignora** o campo `reference` — a referência de hardware vem de Kconfig
(VREFH/VREFL), não do devicetree. Corrigido de passagem o comentário que chamava PTB2/PTB0 de
"canal analógico interno" (**D-18**).

---

## D-04 — `acos` sem clamp de domínio: NaN, e depois UB ✅

**Onde era:** `lib/angulo_incidência_solar/angulo_incidencia_solar.c:110` e `:134`.

Nenhum dos dois argumentos era garantido estar em [−1, +1]:

- **linha 110:** `-tan(φ)tan(δ)` passa de 1 em módulo em latitude alta — o caso do sol que não
  nasce ou não se põe. Não acontece em −23,5°, mas o valor é `static` e editável.
- **linha 134:** é uma **soma de cinco produtos** de senos e cossenos. Erro de arredondamento
  acumulado passa de 1,0 por uma fração de LSB perto do apontamento perfeito — exatamente onde um
  tracker que funciona vive.

`acos` fora do domínio devolve **NaN**, e converter NaN para `int` é comportamento **indefinido**
em C. Combinado com **D-05** e com a ausência de limite de curso no vertical (**P-06**, respondida
depois: 10°–160° provisório), o modo de falha era **mecânico**.

**O que foi feito.** Uma função `clamp_dominio()` que satura em [−1, +1], aplicada nos **dois**
`acos`. Está exportada no header de propósito: a spec do feedforward vai precisar dela também no
`asin` da elevação solar — ver
[../02-software_specs/04-eixo-vertical-feedforward.md](../02-software_specs/04-eixo-vertical-feedforward.md).

---

## D-05 — O servo **envolvia** o ângulo em vez de saturar ✅

**Onde era:** `lib/servo_motor/sevo_angulo_tenpo.c:21-26` e `:33-38` (as duas funções, idênticas).

```c
while(ang>180){ ang-=180; }
while(ang<0)  { ang+=180; }
```

Pedir **185° entregava 5°**. Pedir **−10° entregava 170°**. Uma ordem que deveria parar no fim de
curso mandava a placa para o **extremo oposto**, com força total e sem aviso. Um controle
incremental como este pede ângulo fora de faixa naturalmente, na borda da correção.

**O que foi feito.** Uma `servo_saturar()` estática substituiu os dois laços, nas duas funções.

⚠️ **Os limites são `[0, 180]`** — o curso mecânico do próprio servo. É o mínimo que fecha o
defeito: nenhum pedido fora de faixa vira o extremo oposto. Os limites **por eixo** são outra
questão: o horizontal tem `[10, 170]` no `main.c`, e o vertical passou a ter `[10, 160]` com a
resposta de **P-06** — não há batente de montagem, o batente é o fim de curso do próprio servo, e
`[0, 180]` **não** protege porque o fator 1,47 leva 180° comandados a 2,47 ms de pulso.

---

## D-06 — 12 min 40 s de teste bloqueante em todo boot ✅

**Onde era:** `src/main.c:69-71`, com `NUMERO_DE_TESTES 10` em `lib/testes/testes.c:14`.

Sem condição nenhuma. `teste_motores()` fazia 10 × 4 varreduras × 19 passos = **760** comandos de
servo, cada um com `k_msleep(1000)` dentro (D-14): **12 minutos e 40 segundos** antes da primeira
leitura de tracking. Um reset ao meio-dia custava 12 minutos de sol, varrendo de 0 a 180 sem
relação nenhuma com onde o sol está.

**O que foi feito.** Duas coisas:

1. as chamadas foram para dentro de `#ifdef RODAR_TESTES_NO_BOOT`, e a flag está **comentada** no
   `platformio.ini` — desligada por padrão, com a linha pronta para descomentar no bringup;
2. `NUMERO_DE_TESTES` caiu de 10 para **1**. Dez repetições idênticas não revelam nada que a
   primeira não revele.

Sem a flag, o boot imprime uma linha dizendo que os testes estão desligados e entra direto no laço.

---

## D-07 — `testes.c` chamava as funções de servo sem incluir o header ✅

**Onde era:** `lib/testes/testes.c` incluía `pwm_z42.h`, `ldr.h`, `init_hora.h`,
`angulo_incidencia_solar.h` e `testes.h`, mas **não** `servo_angulo_tempo.h` — e chamava
`servo_horizontal_angulo_tempo()` / `servo_vertical_angulo_tempo()` em quatro linhas.

Declaração implícita: o compilador assumia `int f()` e **parava de conferir os argumentos**. Passava
por sorte, porque os dois parâmetros são `int`. Removida do C23, e já
`-Werror=implicit-function-declaration` por padrão em compiladores recentes — é o tipo de coisa que
quebra o build numa atualização de toolchain, não numa mudança de código.

**O que foi feito.** O `#include` acrescentado, e o `testes.h` ganhou include guard de passagem
(parte do **D-34**, que continua aberto para os outros headers).

---

## Verificação

O que **foi** verificado:

| Verificação | Resultado |
|---|---|
| `arm-none-eabi-gcc -fsyntax-only` nas quatro fontes tocadas, com os **flags e includes reais** do Zephyr extraídos do `compile_commands.json` | ✅ passa |
| `angulo_incidencia_solar.c` compilado isolado, com `-Wall -Wextra` | ✅ sem erro |
| Varredura de literais de string quebrados, e balanceamento de chaves e parênteses | ✅ zero |
| `ADC_REF_EXTERNAL0` e `zephyr,vref-mv` são valores válidos da binding do ADC | ✅ conferido em `dts/bindings/adc/adc-controller.yaml` |

⛔ **O que NÃO foi verificado, e importa:**

- ~~**O projeto não linka nesta máquina.**~~ **Destravado em 2026-09-14** — era cache de build com
  duas grafias do mesmo caminho, não o Zephyr nem o código. O projeto **compila**: `firmware.elf` e
  `firmware.bin` gerados com Zephyr 4.2.0 e GCC 8.2.1. Ver **D-40**, que guarda a receita e a
  armadilha da primeira passada de configure.
- **Nada foi testado em hardware.** Nenhuma destas correções foi observada rodando.
- Os avisos que sobram são **pré-existentes e já catalogados**: `direcao` não usado em `main.c` e
  `N` não usado em `angulo_incidencia_solar.c`, ambos **D-35**.

## O que deliberadamente não entrou

- **Os bugs de controle e matemática** (D-10 a D-18) continuam abertos. Vários deles — o filtro
  quadrático, o θ usado como posição do servo — só fazem sentido corrigir junto com o upgrade de
  4 LDRs, cujas specs estão em [../02-software_specs/](../02-software_specs/).
- **O upgrade de 4 LDRs em si.** O código continua com dois sensores.
- **`pwm_z42.c`**, intocado por **DEC-5**.
