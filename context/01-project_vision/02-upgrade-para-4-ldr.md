# O upgrade: 2 → 4 LDRs

## O problema com 2 LDRs

Dois sensores medem **uma** diferença, e uma diferença só resolve **um** grau de liberdade. Com
esquerda e direita, o firmware sabe se o sol está mais para um lado — e não tem informação alguma
sobre altura.

Por isso o eixo vertical hoje é **malha aberta**: ele vem de `calcular_theta()`, um cálculo
astronômico que depende de data, hora e coordenadas cravadas no código. Se qualquer um desses
estiver errado — e o relógio volta para uma hora fixa a cada boot (D-17) — o eixo vertical erra e
**nada no sistema percebe**. Não há sensor que discorde.

Pior: o número que está sendo mandado para o servo vertical é o **ângulo de incidência** θ, que é
uma medida de erro, não uma posição. Esse é o bug **D-11**, e é o mais caro do projeto.

## O que muda

| Área | Antes | Depois |
|---|---|---|
| Canais de ADC | 2 (`SE12`, `SE8`) | 4 — mais `SE9` (PTB1) e `SE13` (PTB3), **DEC-12** |
| API de leitura | `ldr_ler(char lado)` | `ldr_ler(ldr_id_t)` + `ldr_ler_todos()` |
| Unidade da leitura | contagem bruta × 0,889 (D-03) | milivolts de verdade |
| Filtro | votação quadrática, 25 pares (D-10) | mediana de 5 por sensor |
| Eixo horizontal | passo fixo de 10° | passo proporcional com zona morta |
| Eixo vertical | malha aberta com θ (D-11) | **malha fechada** cima/baixo |
| Cálculo solar | posição do servo vertical | *feedforward*: partida + envelope de sanidade |

O arranjo físico escolhido é **2×2 com divisor em X**, o que faz cada quadrante ser uma direção
(esquerda, direita, cima, baixo) e a mistura ser diferença direta. O desenho e o porquê estão em
[../03-hardware_specs/02-sensores-ldr-e-arranjo-optico.md](../03-hardware_specs/02-sensores-ldr-e-arranjo-optico.md).

## Ordem de execução

As specs em [../02-software_specs/](../02-software_specs/) estão numeradas na ordem em que devem
ser implementadas, e cada etapa é verificável sozinha:

1. **01** — overlay do devicetree com 4 canais
2. **02** — a nova API do `lib/ldr/`
3. **05** — a camada de servo (clamp e separação comandar/esperar) — **antes** do controle, porque
   o controle depende dela
4. **03** — o controle de 4 quadrantes
5. **04** — o eixo vertical e o feedforward
6. **06** — bringup e testes

⭐ A **05 vem antes da 03** de propósito: enquanto o servo bloquear 1–2 s por comando (D-14), o
período de amostragem do controle é irregular e nenhum filtro em cima dele significa o que promete.

## Critério de pronto

Não é "compila" nem "os logs parecem certos". É comportamental, e verificável na bancada com uma
lanterna:

1. **Convergência nos dois eixos.** Movendo uma fonte de luz à mão, na diagonal, a placa a segue
   em azimute **e** em inclinação.
2. **Silêncio no equilíbrio.** Com a luz parada, a placa fica parada. Sem zig-zag — que é
   exatamente o sintoma que o projeto anterior (o carrinho) tinha e nunca resolveu, e o que a zona
   morta existe para evitar.
3. **Sem NaN, sem salto.** Nenhum comando de ângulo fora dos limites de curso, em nenhuma hora
   simulada do dia. Hoje isso não é garantido: D-04 e D-05 juntos permitem que o painel vá para o
   extremo oposto.
4. **Boot em segundos.** Hoje o boot leva mais de 12 minutos de varredura de teste bloqueante
   antes de a primeira leitura de tracking acontecer (D-06).
