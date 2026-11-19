# mega_context: o tracker solar real

Esta pasta é a **fonte da verdade do projeto real**: placa solar de 60 W, motor de vidro elétrico de
12 V, redução por corrente de 20:1, um eixo só (inclinação).

## Hierarquia: `context/` e `mega_context/`

O repositório passou por duas fases, e cada uma tem a sua pasta de documentação.

| Pasta | Fase | Situação |
|---|---|---|
| `context/` | **protótipo**: FRDM-KL25Z, 2 servos MG90S de rotação contínua, 4 LDRs | ⛔ **encerrado em 2026-11-19.** Só histórico |
| `mega_context/` | **projeto real** | ⭐ **em andamento.** Vale sobre qualquer outra coisa |

- **O repositório agora é dedicado ao tracker real.** O protótipo cumpriu sua função.
- `context/` serve só como referência histórica: lições, armadilhas, o que já se tentou. **Nada de lá
  vale para o projeto real sem ser revalidado**: microcontrolador, ADC, alimentação, constantes,
  decisões e bugs são do protótipo. Se `context/` e `mega_context/` divergirem, vale `mega_context/`.
- **Fato novo do projeto real vai para `mega_context/`, nunca para `context/`**, que não é mais
  atualizado.
- **O código também mudou de dono:** `src/main.c` e `lib/motor_rele`, `lib/potenciometro` e
  `lib/pisca_led` são do projeto real. As outras pastas de `lib/` são legado do protótipo e não entram
  no build, porque o PlatformIO só compila o que o `main.c` inclui (duvidas.md, DV-21).

## Como ler esta pasta

| Arquivo | O que é | Quando ler |
|---|---|---|
| [info.md](info.md) | as peças do projeto, em poucas linhas | **sempre primeiro**: é a especificação de onde tudo parte |
| [prompt.md](prompt.md) | o pedido original da área de elétrica/eletrônica | para entender por que [angulo.md](angulo.md) existe |
| [circuito.md](circuito.md) | o circuito de potência: controladora, bateria, três relés | antes de mexer em fiação ou nos relés |
| [angulo.md](angulo.md) | como medir o ângulo: potenciômetro e acelerômetro | antes de mexer no sensor ou na calibração |
| [main.md](main.md) | a **lógica** do firmware v1 e o roteiro para colocá-lo para rodar | antes de mexer no código |
| [main_adr.md](main_adr.md) | as **decisões** do firmware (ADR-NN), com o motivo e o custo de cada uma | antes de desfazer algo que parece estranho no código |
| [duvidas.md](duvidas.md) | **todas** as dúvidas em aberto (DV-NN), cada uma com o seu contexto | para saber o que falta e o que trava o quê |

**Ordem para quem chega agora:** info.md → circuito.md → main.md → main_adr.md → duvidas.md.
[angulo.md](angulo.md) é longo e técnico: leia a seção que o assunto pedir.

## Convenções

- **Ângulos em décimos de grau** (`d10`), sempre inteiros: 450 = 45,0°. O Cortex-M0+ da KL25Z não tem
  FPU, e o `printk` desta configuração não imprime float.
- **Marcadores:** ⭐ importante · ⚠️ cuidado · ⛔ proibido ou perigoso · ✅ resolvido.
- **Fato × suposição.** Valor não medido aparece como "a medir" ou "chute", nunca como fato. Os números
  do firmware dizem de onde vieram (main.md §9).
- **Dúvidas são copiadas, não movidas** (decisão do usuário, 2026-11-19). Cada dúvida continua no
  documento de origem e tem uma cópia com contexto em [duvidas.md](duvidas.md). **Ao resolver uma,
  atualize os dois lugares.** Os números DV-NN e ADR-NN não mudam: outros arquivos citam por eles.
- **Onde cada coisa mora:** fato de circuito → [circuito.md](circuito.md); fato de medição →
  [angulo.md](angulo.md); decisão de código → [main_adr.md](main_adr.md); lógica do código →
  [main.md](main.md).

## Escopo atual (2026-11-19)

Planejado: a medição do ângulo, o circuito do motor e o **firmware v1** (controle até um alvo fixo).

**Não** planejado até ser pedido: mecânica, orientação da estrutura no chão, carga e descarga da
bateria (a controladora é caixa-preta) e o alvo solar da v2 (DV-22).

## Armadilhas do ambiente

Valem para compilar e gravar o firmware. Várias vieram do protótipo e continuam verdadeiras.

- ⛔ **Mexeu no overlay (`zephyr/frdm_kl25z.overlay`)? Apague a pasta `.pio/` antes de compilar.** O
  PlatformIO não regenera o devicetree só porque o overlay mudou, e o build segue com os nós antigos.
  O sintoma é um erro de nó não encontrado em `GPIO_DT_SPEC_GET`, como o `motor_habilitar` que travou a
  sessão do laboratório.
- **Upload pelo LinkServer** (`mcuxpresso_tools.py`): grava o ELF por SWD e falha de verdade se der
  erro. O mesmo script troca as ferramentas que o Windows bloqueia (Smart App Control, Device Guard).
- **`zephyr_link_fix.py` é obrigatório:** sem `-mcpu`/`-mthumb` no link, o Cortex-M0+ cai em HardFault
  antes do `main`. O upload diz "ok", mas a placa não roda e nada sai na serial.
- **Recibo de gravação:** troque `COR_DESTA_GRAVACAO` em `lib/pisca_led/pisca_led.h` a cada gravação.
  Se a placa não mostrar a cor nova no boot, o upload não chegou.
- **Serial a 115200.** O número da porta COM muda com a máquina e a entrada USB. As portas Bluetooth do
  Windows travam ao abrir. Só um programa por vez abre a porta: feche o monitor de um projeto antes de
  abrir o do outro.
- **`platformio.ini` tem caminhos desta máquina** (`core_dir`; `monitor_port` local não commitado):
  DV-20.
- **`src/.git`** é um repositório antigo aninhado: não rode git de dentro de `src/` (DV-23).
