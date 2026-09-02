# Devicetree: 4 canais de ADC

Arquivo: `zephyr/frdm_kl25z.overlay`

A pinagem física é dona de
[../03-hardware_specs/01-plataforma-e-pinagem.md](../03-hardware_specs/01-plataforma-e-pinagem.md).
Aqui só a tradução dela para o devicetree.

## O contrato: a ordem de `io-channels`

⭐ **A ordem desta lista é o índice do `enum ldr_id_t`.** É o acoplamento mais fácil de quebrar
neste projeto, porque quebrar não produz erro: trocar duas entradas troca o nome dos sensores, e o
tracker passa a virar para o lado errado sem uma única mensagem de log.

```dts
/ {
    zephyr,user {
        /* ⚠️ A ORDEM AQUI define o índice do enum ldr_id_t em lib/ldr/ldr.h:
           0 = LDR_ESQ   1 = LDR_DIR   2 = LDR_CIMA   3 = LDR_BAIXO
           Mexeu na ordem aqui, mexa no enum lá. Não há erro de compilação
           que pegue isso — o sintoma é o painel virando ao contrário. */
        io-channels = <&adc0 8>,   /* PTB0  — SE8  — esquerda */
                      <&adc0 12>,  /* PTB2  — SE12 — direita  */
                      <&adc0 9>,   /* PTB1  — SE9  — cima   (P-01) */
                      <&adc0 13>;  /* PTB3  — SE13 — baixo  (P-01) */
    };
};
```

⚠️ **Note que a ordem mudou em relação ao overlay atual**, onde a direita é o índice 0 e a
esquerda o 1. O enum novo é alfabético-por-eixo (esq, dir, cima, baixo) e o overlay o segue. Quem
implementar precisa mudar os **dois** de uma vez.

## Os dois nós novos

Copiam exatamente o padrão dos existentes. Nada de novo — a configuração de canal já era
apropriada.

```dts
&adc0 {
    status = "okay";
    #address-cells = <1>;
    #size-cells = <0>;

    /* Canal 9 — pino PTB1 — LDR de cima. Pino a confirmar: P-01. */
    channel@9 {
        reg = <9>;
        zephyr,gain = "ADC_GAIN_1";
        zephyr,reference = "ADC_REF_INTERNAL";
        zephyr,acquisition-time = <ADC_ACQ_TIME_DEFAULT>;
        zephyr,resolution = <12>;
    };

    /* Canal 13 (0xd) — pino PTB3 — LDR de baixo. Pino a confirmar: P-01. */
    channel@d {
        reg = <13>;
        zephyr,gain = "ADC_GAIN_1";
        zephyr,reference = "ADC_REF_INTERNAL";
        zephyr,acquisition-time = <ADC_ACQ_TIME_DEFAULT>;
        zephyr,resolution = <12>;
    };
};
```

⚠️ **O nome do nó é hexadecimal, o `reg` é decimal.** `channel@d` com `reg = <13>` — igual ao
`channel@c` com `reg = <12>` que já existe. Escrever `channel@13` seria o canal 19.

## Corrigir de passagem: o comentário errado

Os dois nós existentes trazem, hoje:

```dts
reg = <12>; /* Reg 0 (0xc) indica o canal analógico interno */
```

Duas coisas erradas numa linha: não é "Reg 0", e `SE12`/`SE8` **não são canais internos** — são
pinos externos (PTB2 e PTB0). Canal interno na KL25Z é o sensor de temperatura, a banda proibida,
o VREF. Quem ler isso vai procurar o LDR no lugar errado. Corrigir junto — é **D-18**.

## Ponto único de troca dos pinos

**P-01** está aberta: PTB1/PTB3 são sugestão. Se a montagem escolher outro par, o que muda é:

1. os dois `reg =` e os dois nomes de nó `channel@N` aqui;
2. os dois números na lista `io-channels` aqui;
3. a tabela de pinagem em `03-hardware_specs/01`.

**Nada no código C muda** — `lib/ldr/` acessa por índice via `ADC_DT_SPEC_GET_BY_IDX`, não por
número de canal. É por isso que o enum casa com a *posição* na lista, e não com o número do canal
do ADC: assim trocar de pino é editar o overlay e mais nada.

## Verificação

Compila e o binário carrega, mas isso não prova quase nada — o overlay pode estar sintaticamente
válido e apontar para o pino errado. A confirmação de verdade é o passo 1 do bringup: imprimir os
quatro valores em mV e **cobrir um LDR por vez com o dedo**, conferindo que o valor que se move é o
do nome certo. Ver [06-bringup-e-testes.md](06-bringup-e-testes.md).
