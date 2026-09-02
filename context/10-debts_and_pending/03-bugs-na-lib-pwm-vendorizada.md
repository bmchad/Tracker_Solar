# Bugs na `lib/pwm/` — biblioteca de origem terceira

`lib/pwm/pwm_z42.c` e `pwm_z42.h` são o **único** código do projeto que não foi escrito aqui. O
cabeçalho declara:

```c
/*
 * pwm.h
 * Created on: 07/01/2017
 * Author: Evandro
 */
```

Driver TPM bare-metal para o MKL25Z4, portado de macros de registrador
(`SIM_SCGC6 |= ...`) para o estilo CMSIS de ponteiro (`SIM->SCGC6 |= ...`) — os comentários
`// ANTES:` no arquivo são o rastro dessa conversão.

⚠️ **Estes bugs estão catalogados em arquivo separado porque a decisão é NÃO corrigi-los** sem
necessidade concreta. O raciocínio está em [../20-decisoes.md](../20-decisoes.md); em resumo: o
projeto usa exatamente dois canais, os dois funcionam, e mexer num driver de registrador que se
comporta troca um defeito inerte por risco de regressão no único caminho que faz o hardware se
mover.

O que **precisa** estar escrito é que a validação existe e é fictícia — para ninguém confiar no
valor de retorno.

---

## D-20 — A validação de pino usa `||` onde queria `&&`, e é inócua

**Onde:** `lib/pwm/pwm_z42.c`, em praticamente todos os guardas de faixa.

```c
if((pin>=3) || (pin<=5))                              /* linha 59  */
if( (pin>=24 || pin<=25) || (pin>=29 || pin<=31) )    /* linha 105 */
if(pin>=12 || pin<=13)                                /* linha 124 */
if(pin>=20 || pin<=21)                                /* linha 142 */
if(pin>=1 || pin<=2)                                  /* linha 159 */
if((pin>=2||pin<=3) || (pin>=18||pin<19))             /* linha 168 */
if(pin>=22 || pin<=23)                                /* linha 177 */
```

Todas são **tautologias**. `pin >= 3 || pin <= 5` é verdadeiro para qualquer `pin`: se não é ≥ 3,
então é ≤ 5. O autor queria `&&`.

⛔ **Consequência prática:** `pwm_tpm_Ch_Init()` **não valida pino nenhum**. Ela aceita qualquer
número e escreve em `PORTx->PCR[pin]` — um índice fora de faixa é escrita em memória de periférico
arbitrária. O `bool` que ela devolve dá a impressão de que houve verificação; houve verificação de
`gpio` e de `channel`, nunca de `pin`.

**Como conviver:** tratar o retorno de `pwm_tpm_Ch_Init` como "o par TPM/porta é plausível", não
como "o pino é válido". Os pinos usados (PTD0 e PTD3) estão corretos e conferidos contra o
datasheet — ver
[../03-hardware_specs/01-plataforma-e-pinagem.md](../03-hardware_specs/01-plataforma-e-pinagem.md).
Pino novo aqui é conferido **na tabela**, não pelo retorno da função.

---

## D-21 — O ramo `GPIOC` configura o registrador e depois reporta falha

**Onde:** `lib/pwm/pwm_z42.c:70-87`.

```c
else if(gpio == GPIOC)
{
    if(channel<=5)
    {
        if((pin==8) || (pin==9))
        {
            SIM->SCGC5 |= SIM_SCGC5_PORTC_MASK;
            PORTC->PCR[pin] = PORT_PCR_MUX(3);
        }
        if((pin >= 1 && pin <= 4))          /* <- if, não else if */
        {
            SIM->SCGC5 |= SIM_SCGC5_PORTC_MASK;
            PORTC->PCR[pin] = PORT_PCR_MUX(4);
        }
        else return false;
    }
```

O segundo `if` deveria ser `else if`. Como não é, para `pin == 8` ou `pin == 9` o fluxo é:

1. configura `PORTC->PCR[pin] = MUX(3)` — **o efeito colateral acontece**;
2. cai no segundo `if`, que é falso (8 e 9 não estão em 1..4);
3. entra no `else` e **`return false`**.

Ou seja: os pinos PTC8 e PTC9 são configurados e a função reporta que falhou. Quem checar o retorno
vai concluir que não configurou e tentar outro caminho — com o registrador já escrito. E o `return
false` acontece **antes** de `tpm->CONTROLS[channel].CnSC |= mode` na linha 191, então o pino fica
com MUX de TPM mas o canal do timer não é habilitado: um pino mudo, configurado pela metade.

Notar que este é o único guarda de pino do arquivo escrito **corretamente** com `&&`
(`pin >= 1 && pin <= 4`) — e é justamente ele que expõe o `else` mal encadeado. Nos outros ramos a
tautologia de D-20 esconde o problema.

**Não afeta este projeto:** só PTD0 e PTD3 são usados, no ramo `GPIOD`, que está encadeado
corretamente. Fica registrado para quem um dia precisar de um terceiro eixo em PORTC.
