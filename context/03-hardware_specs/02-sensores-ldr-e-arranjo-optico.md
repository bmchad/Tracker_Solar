# Sensores LDR e arranjo óptico

## O arranjo escolhido: 2×2 com divisor em X

Quatro LDRs em quadrantes, separados por uma aleta em forma de **X** que projeta sombra. O X é
girado 45° em relação a um `+`, e essa rotação é o que define toda a matemática do controle:
**cada quadrante aponta para uma direção**, não para uma diagonal.

```
            cima
              ▲
        ╲     │     ╱
          ╲   │   ╱
            ╲ │ ╱
   esquerda   ╳   direita        ← o X é a aleta que gera sombra
            ╱ │ ╲
          ╱   │   ╲
        ╱     │     ╲
              ▼
           baixo
```

Quando o sol está à esquerda, a aleta sombreia o quadrante da direita e o LDR da esquerda recebe
mais luz. Logo:

```
erro_horizontal = ldr[ESQUERDA] - ldr[DIREITA]
erro_vertical   = ldr[CIMA]     - ldr[BAIXO]
```

Uma subtração por eixo, um sensor por direção. Simples de propósito.

⚠️ **Se a aleta virar um `+`**, os quadrantes passam a ser diagonais (sup-esq, sup-dir, inf-esq,
inf-dir) e a mistura muda para **soma de pares**:

```
erro_horizontal = (SUP_ESQ + INF_ESQ) - (SUP_DIR + INF_DIR)
erro_vertical   = (SUP_ESQ + SUP_DIR) - (INF_ESQ + INF_DIR)
```

É por isso que a spec de controle isola a mistura numa função única — ver
[../02-software_specs/03-controle-4-quadrantes.md](../02-software_specs/03-controle-4-quadrantes.md).
Trocar o arranjo físico deve custar uma função, não uma auditoria.

## A geometria óptica é o ganho da malha

A altura da aleta em relação ao espaçamento dos sensores determina o comportamento do controle, e
é o parâmetro físico que ninguém pode ajustar em software:

- **Aleta alta / sensores próximos** → muito contraste, sombra nítida, mas **ângulo de captura
  estreito**: fora de alguns graus, um único quadrante fica totalmente sombreado e o erro
  **satura** — o controle perde a noção de "quão longe" e só sabe "para que lado".
- **Aleta baixa / sensores afastados** → resposta suave e faixa larga, mas pouco contraste perto
  do equilíbrio: o erro no alvo fica da ordem do ruído, e é aí que nasce o zig-zag.

Isso não é ajustável depois: é o que fixa a escala de `GANHO_MV_POR_GRAU` e `ZONA_MORTA_MV` na
spec de controle. Por isso o primeiro passo do bringup é **medir os quatro valores em mV** com a
placa travada à mão, antes de qualquer motor girar — ver
[../02-software_specs/06-bringup-e-testes.md](../02-software_specs/06-bringup-e-testes.md).

## Circuito

Cada LDR forma um **divisor de tensão** com um resistor fixo, e o ponto médio vai ao pino do ADC.

⚠️ **A topologia do divisor define o SINAL do erro, e o código atual não a documenta.** Nada nos
arquivos-fonte diz se o LDR está do lado de VDD ou do lado do terra:

- **LDR para VDD, resistor para GND** (pull-down): mais luz → menor resistência do LDR → tensão
  **sobe**. Tensão alta = muita luz.
- **Resistor para VDD, LDR para GND** (pull-up): mais luz → tensão **cai**. Tensão alta = pouca luz,
  e as duas subtrações acima ficam **invertidas**.

Se o sinal estiver trocado, o tracker foge do sol em vez de segui-lo — e como o movimento é
incremental, ele vai direto para o batente e fica lá. É a primeira coisa a confirmar no bringup.
Ver **P-03**.

O valor do resistor e a faixa de mV que o conjunto entrega no local de operação são **P-03** e
**P-04**.

## Os quatro LDRs precisam ser o mesmo componente

O controle compara sensores **entre si**, não contra um valor absoluto — então um LDR com
sensibilidade diferente dos outros é um erro **constante** que o firmware lê como "o sol está
daquele lado" para sempre, e o painel para no batente.

Os quatro devem ser do mesmo lote, com os mesmos resistores. Casar isso em software exigiria uma
rotina de calibração em sombra total, gravando um offset por sensor — não está no escopo desta
virada, e é a razão de o casamento ser requisito de montagem.
