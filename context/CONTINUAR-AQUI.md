# CONTINUAR AQUI — 2026-09-15

Retrato da investigação para a próxima sessão. **Não é spec nem decisão** — quando o assunto fechar,
o que sobrar de durável migra para `10-debts_and_pending/` ou `20-decisoes.md` e este arquivo é
apagado.

## Estado do repositório

⭐ **Os dois dias de trabalho ESTÃO commitados**, em `a411ae6` — camada de servo nova, arranjo em
cruz, quatro testes de bancada e todo este `context/`. (Este arquivo dizia o contrário, apontando
para `91b27a9`; ficou desatualizado no instante do commit.)

Fora do git continuam só as mudanças em curso: `lib/pisca_led/` (a antiga `lib/teste_portas/`,
reduzida a uma função `led(cor)`) e `lib/teste_console/`.

---

## ✅ O laço de tracking foi portado (2026-09-15)

`src/main.c` não tem mais `#if 0`: o tracker é código vivo, compila sem aviso, e fecha cinco dívidas
(**D-10**, **D-11**, **D-12**, **D-15**, e **D-13** pela metade).

A forma dele, em seis passos: `ldr_ler_todos()` numa chamada → erro por **soma de pares** → buffer
circular de 5 **erros já misturados** → **mediana** por eixo → passo FIXO se o erro passar da margem
→ move, zera o buffer e espera assentar.

⭐ **A decisão de desenho que vale entender antes de mexer:** o passo é **fixo**, não proporcional ao
erro. Proporcional chegou a ser implementado e foi revertido no mesmo dia — o erro em milivolts
escala com a **intensidade** da luz, não só com o ângulo, então não existe "mV por grau" a calibrar.
Raciocínio completo em [20-decisoes.md](20-decisoes.md) **DEC-17**.

⚠️ **Dois parâmetros, os dois empíricos:** `MARGEM_MV = 60` (tem de ficar acima do piso de ruído) e
`PASSO_MS = 200` (~12° com ω = 60 °/s). ⛔ **O zig-zag não está resolvido por construção:** se 12°
levarem o erro a passar da margem do outro lado, o tracker oscila. Ajusta-se na bancada — sobe a
margem, ou baixa o passo até o piso do `BUILD_ASSERT`.

---

## 🎯 O PRÓXIMO PASSO: a bancada, nesta ordem

Nada mais depende de código. Os quatro LDRs estão ligados (pull-down de 10 kΩ, confirmado na
montagem), o laço está escrito, e daqui em diante o que falta são **medições** — cada uma
desbloqueando a seguinte.

1. ⚠️ **O servo se mexe?** `VELOCIDADE_PCT = 10` são 50 µs de desvio do neutro, e a zona morta típica
   de rotação contínua é ±45 a ±90 µs. **Pode não girar.** Se não girar, suba o número em
   `sevo_motor.c` — e **o primeiro valor em que ele anda é a zona morta medida**.
   ⭐ Subir a velocidade não quebra nada no controle: ω muda, e `PASSO_MS` passa a valer mais graus.
   Nenhum número precisa ser refeito — é a vantagem do passo fixo (**DEC-17**). Só a odometria se
   importa, e ela já lê ω do `eixo_t`.
2. **Medir ω**, quatro números (dois eixos × dois sentidos), com `teste_rot_cont_medir_omega()`.
   Rotação contínua é assimétrica, e um ω só embute erro que **acumula** em vez de se cancelar.
3. **Passo 1 do bringup**, agora possível: `teste_ldr()` com a placa parada. Saem o **piso de
   ruído**, que é o que calibra `MARGEM_MV`, e o **espalhamento dos quatro na sombra** (o offset
   estático dos resistores de 5 %, que com soma de pares se somam).
4. **Teste do dedo:** cobrir um sensor confere a ordem de `io-channels` contra o enum — é o **único**
   teste dela, e nada mais pega uma troca.
5. **Malha fechada, com lanterna.** O eixo segue a luz? Para quando ela para? ⭐ E o teste que pega
   oscilação: luz de frente, equilibrada, um minuto parado — o eixo tem de ficar **imóvel**, LED
   verde.

## ✅ O problema do passo mínimo FECHOU, e por medição (2026-09-15)

Ele era o obstáculo aberto desde o começo: o menor movimento possível é `SERVO_MS_MIN × ω`, e com o
chute de 150 ms isso dava **12°** enquanto o sol anda 15°/h — grosso demais para acompanhar.

⭐ **Mas 150 ms nunca foi medido, e o comentário dele no código pedia exatamente o experimento que
faltava**: *"comandar 50, 100, 200 ms e ver de onde em diante o movimento é proporcional ao tempo"*.
Foi feito na bancada, degrau a degrau: **100 ms funcionou, 50 ms funcionou, 25 ms não.** O piso real
está entre 25 e 50 ms — cerca de **três vezes mais baixo** que o chute.

⭐ **Consequência: a correção mais fina passou de 12° para ~3°.** O sol anda 15°/h, ou seja 3° a cada
12 minutos: o tracker corrige cinco vezes por hora, com folga. As duas saídas que estavam na mesa —
**redução mecânica** e **servo posicional** — deixaram de ser necessárias.

⚠️ **O que continua verdade:** existe um piso, e abaixo dele o movimento deixa de ser proporcional ao
tempo enquanto a odometria conta o que não aconteceu. É a mesma drift que produz o **limite falso**.
O `BUILD_ASSERT` em `main.c` barra o build se `PASSO_MS` descer abaixo de `SERVO_MS_MIN`.

⚠️ Os valores em vigor não ficam anotados aqui nem em comentário de código — eles estão em ajuste
ativo, e o histórico de cada tentativa fica no log de bancada.

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
fisicamente o inferior esquerdo). Mistura por **soma de pares**, e `MARGEM_MV` foi reescalada ×2
porque o erro dobrou de magnitude. A **DEC-1** ficou revertida pelo hardware.

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

## Uma pendência de bancada que a lista acima não cobre

**Conferir a odometria:** na medição de posição (`teste_medicao_posicao_*`), se o eixo não voltar ao
ponto marcado, a diferença **é** o erro acumulado. É a única evidência disponível de quanto ω ainda
precisa ser medido — não há encoder que desminta a odometria de outro jeito.

## Armadilhas do ambiente que continuam valendo

- **D-40**: build travado por cache com duas grafias do mesmo caminho — resolvido, mas a receita é
  apagar `.pio/` inteiro, não `fullclean`;
- **D-43**: mexer no overlay **não** regenera o devicetree. Apague `.pio/`;
- ⭐ **recibo de gravação**: `COR_DESTA_GRAVACAO` em `lib/pisca_led/pisca_led.h` — troque a cor a
  cada build; `main()` acende o LED nela como primeira instrução. Se a
  placa não mudar de cor, o upload não chegou. Numa das máquinas nenhum upload chegava, com o
  terminal dizendo sucesso.
