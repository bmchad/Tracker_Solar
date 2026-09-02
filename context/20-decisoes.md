# Decisões

O porquê de cada escolha, **com o que foi rejeitado ao lado**. É o que o código não conta — e o que
evita alguém "consertar" de volta uma decisão consciente.

Uma entrada por decisão. Data, escolha, alternativa recusada, motivo.

---

## DEC-1 — Divisor em X, não em `+`

**2026-09-02.** O arranjo dos 4 LDRs é 2×2 com aleta em **X**, o que faz cada quadrante apontar
para uma direção (esquerda, direita, cima, baixo).

**Rejeitado:** aleta em `+`, com quadrantes diagonais.

**Motivo:** com o X, a mistura é **uma subtração por eixo** — um sensor por direção. Com o `+`, cada
eixo é uma soma de pares, o que é matematicamente equivalente mas dobra o número de termos e
introduz a chance de somar o par errado. Como o arranjo é físico e pode mudar, o código isola a
mistura numa função única com a forma alternativa escrita no comentário: trocar de arranjo custa uma
função, não uma auditoria. Ver
[03-hardware_specs/02-sensores-ldr-e-arranjo-optico.md](03-hardware_specs/02-sensores-ldr-e-arranjo-optico.md).

---

## DEC-2 — Eixo vertical em malha fechada, com o cálculo como feedforward

**2026-09-02.** O par cima/baixo comanda o eixo vertical. `calcular_theta()` deixa de ser posição e
passa a dois papéis: **posição de partida** e **envelope de sanidade**.

**Rejeitado — manter só astronômico:** é o que existe hoje, e é o bug **D-11**. Além do erro
dimensional (θ é função de custo, não posição), o cálculo depende de um relógio que **volta para
15:00 a cada boot** (**D-17**). Um cálculo exato alimentado por hora errada é uma fonte de erro
confiante, e nada no sistema o desmente — não há sensor que discorde.

**Rejeitado — malha fechada pura, aposentando o cálculo:** perde as duas coisas que só ele sabe
fazer.

1. **Partida.** Sem feedforward a placa começa em 0°, que pode estar a 90° do sol. E do fundo do
   poço a malha do LDR **não tem sinal**: com todos os quadrantes na sombra os quatro valores são
   igualmente baixos, o erro é zero, a zona morta manda ficar parado. O tracker dorme e não acorda.
2. **Envelope.** Só o cálculo sabe onde o céu **não** está. É o que impede perseguir um poste, um
   reflexo em janela ou um farol — porque o cálculo não olha para nada, então não se ilude.

O feedforward **limita** em vez de comandar precisamente porque ele erra por relógio: o envelope é
largo o bastante para absorver hora imprecisa e montagem torta, estreito o bastante para nada fora
do céu ser perseguido. Ver
[02-software_specs/04-eixo-vertical-feedforward.md](02-software_specs/04-eixo-vertical-feedforward.md).

---

## DEC-3 — Mediana de 5 por sensor, não votação cruzada

**2026-09-02.** O filtro passa a ser mediana de 5 amostras por sensor, seguida de uma comparação
por eixo.

**Rejeitado — manter a votação quadrática:** ela é incoerente com o que o próprio log afirma
(**D-10**): 25 pares, limiar 5, e a mensagem diz *"Certeza absoluta"*. Basta **um** outlier para
disparar. E comparar cada amostra de um sensor com cada amostra do outro **mistura** a diferença
espacial (o que se quer medir) com a variação temporal de cada sensor (o que se quer filtrar).

**Rejeitado — média móvel:** média **arrasta** para o lado do outlier. Uma nuvem que corta um
quadrante por um instante desloca a média daquele sensor, e o painel se move por causa dela. A
mediana descarta o extremo sem ser influenciada por seu valor — é o filtro certo para ruído
impulsivo, que é a natureza do ruído aqui (nuvem, reflexo, transiente de corrente do servo).

Cinco amostras, número **ímpar**, para a mediana não precisar de média entre as duas centrais.

---

## DEC-4 — Quebrar a API do `lib/ldr/` sem camada de compatibilidade

**2026-09-02.** `ldr_ler(char lado)` sai; entra `ldr_ler(ldr_id_t, int32_t *)`.

**Rejeitado:** manter o despacho por `char` funcionando em paralelo.

**Motivo:** há **um** consumidor (`main.c`, mais as rotinas de teste). Camada de compatibilidade num
protótipo com um consumidor é código morto no dia em que nasce, e teria de carregar adiante os dois
defeitos que a mudança existe para matar: o erro voltando como valor (`-1.0`, **D-01**) e a escala
inventada (**D-03**).

O valor passa de `double` para `int32_t` na mesma leva: milivolts inteiros bastam num ADC de 12 bits
sobre 3,3 V, e o Cortex-M0+ **não tem FPU** — todo `double` aqui era soft-float pago sem retorno.

---

## DEC-5 — Não corrigir `lib/pwm/pwm_z42.c`

**2026-09-02.** Os bugs **D-20** (validação com `||` em vez de `&&`, portanto inócua) e **D-21**
(ramo `GPIOC` que configura e depois falha) ficam **registrados e não corrigidos**.

**Rejeitado:** consertar os guardas de faixa.

**Motivo:** é código de origem terceira (2017), portado à mão, que se comporta no caminho que este
projeto usa. O projeto usa exatamente **dois** canais, os dois em `GPIOD` — o único ramo encadeado
corretamente. Mexer num driver de registrador que funciona troca um defeito inerte por risco de
regressão no único caminho que faz o hardware se mover, e o benefício é validação de pinos que
ninguém vai passar errados porque estão na tabela de pinagem.

**O que a decisão exige em troca:** que esteja escrito que a validação é fictícia, para ninguém
confiar no `bool` de retorno. Está em
[10-debts_and_pending/03-bugs-na-lib-pwm-vendorizada.md](10-debts_and_pending/03-bugs-na-lib-pwm-vendorizada.md).

Reabrir se e quando um terceiro eixo precisar de PORTC.

---

## DEC-6 — Testes atrás de flag de compilação, não removidos

**2026-09-02.** `teste_motores()` e `teste_ldr_4()` passam a rodar só com
`-DRODAR_TESTES_NO_BOOT`, e `NUMERO_DE_TESTES` cai de 10 para 1.

**Rejeitado — apagar as rotinas:** elas são o **único** instrumento de verificação do projeto. Não
há `ztest`, não há CI, não há `test/` com nada dentro. Todo bringup depende delas, e a spec de
bringup as usa nos quatro passos.

**Rejeitado — deixar como estão:** são 12 min 40 s de sono bloqueante em **todo** boot (**D-06**).
Um reset ao meio-dia custa 12 minutos de sol, varrendo de 0 a 180 sem relação com onde o sol está.

Dez repetições idênticas não revelam nada que a primeira não revele. Se o objetivo era procurar
falha intermitente, o instrumento é um teste de resistência dedicado — não o caminho de boot do
produto.

---

## DEC-7 — `lib/controle/` é puro: sem ADC, sem PWM, sem `k_msleep`

**2026-09-02.** A mistura dos quadrantes e a decisão de passo saem do `main.c` para um módulo que
recebe quatro valores em mV e devolve deltas de ângulo.

**Rejeitado:** deixar a lógica inline no laço, como está hoje.

**Motivo:** **não há framework de teste neste projeto.** A única verificação disponível é raciocinar
sobre a função — e isso só funciona se ela não tiver efeito colateral. Uma função que lê ADC, dorme
e escreve em registrador não pode ser conferida sem placa na mesa; uma que é função dos argumentos
pode ser conferida no papel.

É a razão de a decisão existir apesar de "mover código para um arquivo novo" não corrigir nenhum bug
por si só.

---

## DEC-8 — Não mexer nos fatores de calibração antes de medir

**2026-09-02.** Os fatores `1.21` (horizontal) e `1.47` (vertical) ficam como estão. A mudança é só
**parametrizá-los** — dar nome às constantes e separar as contagens de pulso.

**Rejeitado:** substituir por uma fórmula limpa, ou pela faixa 0,5–2,4 ms que é a suspeita mais
provável (**P-08**).

**Motivo:** esses números estão empiricamente ajustados ao hardware que está montado — inclusive,
provavelmente, a folgas mecânicas dele, o que explicaria por que dois servos do mesmo modelo têm
fatores diferentes. Trocá-los por matemática correta **sem medir** substitui um erro conhecido e
compensado por um desconhecido, e o sintoma seria um painel que aponta consistentemente errado com
o código parecendo mais certo que antes.

A parametrização existe para que a medição, quando vier, seja uma edição de duas linhas. Ver
**P-08**.

---

## DEC-9 — `context/` com dono único por fato

**2026-09-02.** Cada fato mora em **um** arquivo; os outros referenciam. Bug tem ID `D-nn`,
pendência tem `P-nn`, e as specs citam pelo ID sem redescrever.

**Rejeitado:** um documento por assunto com o contexto repetido onde fosse conveniente.

**Motivo:** é como documentação apodrece. O mesmo número em dois lugares envelhece em um deles, e
quem lê o desatualizado não tem como saber. Com dono único, corrigir é um lugar — e a verificação é
mecânica: `grep` pelos IDs e pelos `A DEFINIR` fecha o ciclo.
