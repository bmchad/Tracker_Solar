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
        io-channels = <&adc0 8>,   /* PTB0 = A0 — SE8  — esquerda */
                      <&adc0 12>,  /* PTB2 = A2 — SE12 — direita  */
                      <&adc0 9>,   /* PTB1 = A1 — SE9  — cima     */
                      <&adc0 13>;  /* PTB3 = A3 — SE13 — baixo    */
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

    /* Canal 9 — pino PTB1 (= A1) — LDR de cima. Pino confirmado, P-01 resolvido. */
    channel@9 {
        reg = <9>;
        zephyr,gain = "ADC_GAIN_1";
        zephyr,reference = "ADC_REF_EXTERNAL0";   /* ver D-19 */
        zephyr,vref-mv = <3300>;                  /* ver D-19 */
        zephyr,acquisition-time = <ADC_ACQ_TIME_DEFAULT>;
        zephyr,resolution = <12>;
    };

    /* Canal 13 (0xd) — pino PTB3 (= A3) — LDR de baixo. Pino confirmado, P-01 resolvido. */
    channel@d {
        reg = <13>;
        zephyr,gain = "ADC_GAIN_1";
        zephyr,reference = "ADC_REF_EXTERNAL0";   /* ver D-19 */
        zephyr,vref-mv = <3300>;                  /* ver D-19 */
        zephyr,acquisition-time = <ADC_ACQ_TIME_DEFAULT>;
        zephyr,resolution = <12>;
    };
};
```

⛔ **`zephyr,reference` e `zephyr,vref-mv` nos QUATRO nós, não só nos dois novos.** Os dois
existentes estão em `ADC_REF_INTERNAL` hoje, e é isso que faz `adc_raw_to_millivolts_dt()` devolver
**0 mV com código de sucesso** neste driver — ver **D-19**. Sem esta correção, a spec de
[02-api-ldr-4-sensores.md](02-api-ldr-4-sensores.md) transforma o tracker num aparelho parado que
não reclama.

⭐ **Por que é seguro mexer na referência:** `mcux_adc16_channel_setup` **ignora**
`channel_cfg->reference` — ele valida canal, tempo de aquisição, diferencial e ganho, e nada mais
(`drivers/adc/adc_mcux_adc16.c:102-132`). A referência de **hardware** vem de Kconfig
(`ADC_MCUX_ADC16_VREF_DEFAULT` → par de pinos VREFH/VREFL), não do devicetree. Então trocar este
campo **não muda nada no hardware**: muda só qual ramo a conversão em software toma.

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

## Pinctrl: uma premissa que não estava escrita

A dts da placa já configura o ADC, e com um grupo de pinctrl:

```dts
/* boards/nxp/frdm_kl25z/frdm_kl25z.dts */
&adc0 { status = "okay"; pinctrl-0 = <&adc0_default>; pinctrl-names = "default"; };

/* boards/nxp/frdm_kl25z/frdm_kl25z-pinctrl.dtsi */
adc0_default: adc0_default {
    group0 { pinmux = <ADC0_SE12_PTB2>; ... };   /* UM pino só */
};
```

⚠️ `adc0_default` nomeia **apenas `ADC0_SE12_PTB2`**. O PTB0 que o projeto usa hoje **não está
lá** — e funciona porque a função analógica é o **default de reset**.

⭐ **Isso está provado, não inferido.** No header de pinmux do fabricante, para o package que a placa
usa (`hal/nxp/dts/nxp/kinetis/MKL25Z128VLK4-pinctrl.h`, linhas 70/75/80/85), as quatro funções de
ADC são declaradas com `MUX` = **0**:

```c
#define ADC0_SE8_PTB0  KINETIS_MUX('B',0,0)   /* PTB0 */
#define ADC0_SE9_PTB1  KINETIS_MUX('B',1,0)   /* PTB1 */
#define ADC0_SE12_PTB2 KINETIS_MUX('B',2,0)   /* PTB2 */
#define ADC0_SE13_PTB3 KINETIS_MUX('B',3,0)   /* PTB3 */
```

E `MUX(0)` no `PORTx_PCRn` do Kinetis é "pino desabilitado (analógico)", que é o valor de reset. Por
isso os quatro canais funcionam sem entrar em grupo de pinctrl nenhum.

⚠️ **A contrapartida do `MUX(0)`:** ele não *seleciona* o ADC — ele solta o pad para o domínio
analógico. E os mesmos quatro pinos são canais de **TSI0** (`TSI0_CH0/CH6/CH7/CH8`), também em
`MUX(0)`. Não existe mux que escolha entre ADC e touch: quem fica com o pad é o bloco analógico
habilitado. Hoje o `prj.conf` não habilita TSI, então não há disputa — mas se algum dia habilitar
touch, o conflito não vai aparecer como erro de build, e sim como leitura sem sentido.

Se quiser fechar a premissa de vez em vez de depender do reset, acrescente os quatro pinos a um
grupo de pinctrl próprio no overlay. É opcional, e a spec não pede.

## Ponto único de troca dos pinos

**P-01 está resolvida** (PTB1/PTB3, ver **DEC-12**), mas a propriedade continua valendo e é o que
torna a decisão barata de reverter. Se a montagem algum dia trocar de par, o que muda é:

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
