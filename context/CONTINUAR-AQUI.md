# CONTINUAR AQUI — 2026-09-15

Retrato da investigação para a próxima sessão. **Não é spec nem decisão** — quando o assunto fechar,
o que sobrar de durável migra para `10-debts_and_pending/` ou `20-decisoes.md` e este arquivo é
apagado.

## ⛔ ANTES DE QUALQUER COISA

**Nada está commitado.** O último commit é `91b27a9`. Dois dias de trabalho — camada de servo nova,
arranjo em cruz, quatro testes de bancada, e todo este `context/` — existem só na árvore de trabalho.

---

## 🎯 O PRÓXIMO PASSO: portar o `while` de `src/main.c`

O laço de tracking está **desativado com `#if 0`**, com um bloco no código explicando o porquê. Ele
**não linka**: chama `servo_horizontal_angulo_tempo()` e `servo_vertical_angulo_tempo()`, que
deixaram de existir.

⚠️ **NÃO escreva um wrapper `servo_*_angulo_tempo` chamando a camada nova.** Seria exatamente a
mentira que o refactor removeu — prometer ângulo a um dispositivo que só entende velocidade. Foram
dois dias de investigação até descobrir que era isso.

### O que precisa mudar, item a item

| Hoje, no `#if 0` | Para onde vai |
|---|---|
| `angulo += correcao` e `servo_horizontal_angulo_tempo(angulo, 2)` | `servo_horizontal_direcao_tempo(SERVO_HORARIO ou SERVO_ANTIHORARIO, ms)` |
| clamp `10..170` | **sai**: quem limita agora é a odometria, com ±120° (h) e ±50° (v), e a camada satura sozinha devolvendo menos ms |
| `ldr_ler('d'/'e')` com dois sensores | `ldr_ler_todos(mv)` com quatro quadrantes, e **soma de pares** (arranjo em cruz) |
| `calcular_theta()` como POSIÇÃO do vertical | continua sendo o **D-11**; e com rotação contínua nem existe posição para mandar |
| filtro de votação quadrática | **D-10** — a spec pede mediana de 5 (DEC-3) |

⭐ **A forma que o controle quer ter**, e que a camada já suporta:

```c
if (erro_h > ZONA_MORTA_MV)  servo_horizontal_direcao_tempo(SERVO_HORARIO, ms);
```

⛔ **E o obstáculo real, que decide o desenho:** o **passo mínimo**. `SERVO_MS_MIN = 150 ms` vezes ω
dá o menor movimento possível — **9°** com a velocidade em 10%, **27°** em 30%. O sol anda 15°/h.
Não dá para baixar a velocidade (zona morta) nem o tempo (movimento deixa de ser proporcional). As
saídas são redução mecânica, aceitar passo grosso com zona morta maior que ele, ou servo posicional.
**Isso não foi decidido.**

---

## O que mudou nestes dois dias

### ⭐ Os servos são de ROTAÇÃO CONTÍNUA

Confirmado girando o braço à mão: **não há batente nenhum**. Pulso comanda **velocidade e sentido**,
não posição — 1,0 ms gira num sentido, 1,5 ms para, 2,0 ms gira no outro. É a explicação de tudo que
se investigou por dois dias: o firmware mandava "0 grau" (= 1,0 ms) no boot, e para este servo isso
significa *gire a toda velocidade*.

### A camada de servo (`lib/servo_motor/`, arquivos renomeados)

`servo_motor.h` + `sevo_motor.c` (⚠️ o typo `sevo` sobreviveu ao rename — D-32). API:

- `servo_*_direcao_tempo(servo_dir_t, uint32_t ms)` → devolve os ms **efetivamente aplicados**;
- satura a duração no limite de curso (espírito do D-05), e **recusa** comando abaixo de 150 ms;
- odometria θ = ω·t, em décimos de grau, com `servo_zerar_*()` e `servo_theta_*_d10()`;
- **todo parâmetro de eixo mora no `eixo_t`**; o `mover()` guarda só o algoritmo.

⚠️ **ω é MODELADO, não medido:** `ω = 125·V·α`, com `V_ALIM_MV = 4800` (medido) e α = `VELOCIDADE_PCT/100`.
O modelo ignora zona morta e assimetria. **Enquanto não for medido, os limites de curso protegem
menos do que prometem.**

### Os LDRs viraram arranjo em CRUZ

Enum é `LDR_SUP_ESQ / SUP_DIR / INF_ESQ / INF_DIR` (os nomes antigos mentiam: `LDR_CIMA` era
fisicamente o inferior esquerdo). Mistura por **soma de pares**, e `ZONA_MORTA_MV`/`GANHO_MV_POR_GRAU`
foram reescalados ×2 porque o erro dobrou de magnitude. A **DEC-1** ficou revertida pelo hardware.

### Os quatro testes de bancada

⭐ A distinção que os organiza: **quem passa pela camada de servo e quem não passa.**

| Teste | Caminho | Responde |
|---|---|---|
| `teste_rot_cont()` | CnV cru | o servo gira? sai da zona morta? **não retorna** |
| `teste_rot_cont_medir_omega()` | CnV cru | quanto vale ω (4 números). **não retorna** |
| `teste_medicao_posicao_*()` | **pela camada** | odometria e limite funcionam? |
| `teste_ldr()` | — | os 4 quadrantes, e a soma de pares |

⛔ `medir_omega` **tem** de usar CnV cru: a camada satura usando o próprio ω, então medir por ela
seria medir a suposição.

---

## Pendências de bancada, em ordem

1. ⚠️ **`VELOCIDADE_PCT` está em 10%** = 50 µs de desvio, e a zona morta típica é ±45 a ±90 µs. **O
   servo pode simplesmente não girar.** Se não girar, suba o número — e o primeiro valor em que ele
   anda **é a zona morta medida**;
2. **medir ω**, quatro números, com `teste_rot_cont_medir_omega()`;
3. **conferir a odometria**: na medição de posição, se o eixo não voltar ao ponto marcado, a
   diferença é o erro acumulado;
4. **teste do dedo** nos LDRs: cobrir um sensor confere a pinagem; cobrir dois do mesmo lado confere
   a soma de pares.

## Armadilhas do ambiente que continuam valendo

- **D-40**: build travado por cache com duas grafias do mesmo caminho — resolvido, mas a receita é
  apagar `.pio/` inteiro, não `fullclean`;
- **D-43**: mexer no overlay **não** regenera o devicetree. Apague `.pio/`;
- ⭐ **recibo de gravação**: `LED_DO_TESTE` em `teste_portas.c` — troque a cor a cada build. Se a
  placa não mudar de cor, o upload não chegou. Numa das máquinas nenhum upload chegava, com o
  terminal dizendo sucesso.
