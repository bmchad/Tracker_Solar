# CONTINUAR AQUI — estado da investigação, 2026-09-14, 18h40

Documento de passagem entre sessões. **Não é spec nem decisão** — é o retrato de uma investigação
em aberto, com o que foi medido, o que foi descartado e qual é o próximo experimento. Quando o caso
fechar, o que sobrar de durável migra para `10-debts_and_pending/` ou `20-decisoes.md` e este
arquivo é apagado.

---

## ⚠️ Antes de qualquer coisa, numa máquina nova

**`lib/teste_portas/teste_portas.c` tem alterações NÃO COMMITADAS** (o commit mais recente é
`91b27a9`). O binário que está gravado na placa foi compilado dessas alterações. Se você clonar em
outro computador, vai pegar a versão anterior do arquivo, que se comporta **diferente**: aciona um
pino de cada vez em vez dos dois juntos.

Commite antes de trocar de máquina, ou recompile sabendo dessa diferença.

---

## O sintoma

Os servos não se movem. **Três saídas independentes estão mudas:**

| Saída | Estado |
|---|---|
| Serial (COM9, 115200) | nunca chegou **um byte**, a sessão inteira |
| LED da placa | não pisca, mesmo com firmware que só faz piscar |
| Servos | não se movem em nenhum teste |

## ⭐ A medição que muda tudo: 0,16 V estáveis em PTD3 e PTD0

Medido com multímetro, DC, entre GND e cada pino de sinal: **~0,16 V, estável**, nos dois.

⭐ **Isso não é ruído nem pino flutuante — é PWM de servo em 0°.** A conta:

> 50 Hz com pulso de 1,0 ms = **5% de duty**. Multímetro lê média: 0,05 × 3,3 V = **0,165 V**.
> É exatamente `CnV = 375`, o valor de 0° na fórmula `CnV = 375 + FATOR × ang × 375/180`.

⛔ **E o firmware que nós gravamos não produz isso.** A versão atual de `teste_portas.c` roda com
`RODAR_FASES_PWM = 0`: ela **não chama `pwm_tpm_Ch_Init`**, então os canais de TPM nunca são
ligados e os pinos ficam em GPIO, alternando 3,3 V / 0 V a cada 5 s. Um valor estável de 0,165 V é
incompatível com ela.

**Quem produz exatamente esse estado é o `main.c` anterior às edições de hoje:**

```c
servo_horizontal_angulo_tempo(angulo, 1);   /* angulo = 0 -> CnV = 375 */
servo_vertical_angulo_tempo(angulo, 1);     /* idem                    */
...                                          /* testes desligados      */
return 0;                                    /* main retorna; TPM segue */
```

⭐ **E o TPM é hardware: ele continua gerando o pulso depois que a CPU para.** `main` retornar (ou
travar) não desliga o timer. Então "0,165 V estáveis + LED morto + serial muda" é a assinatura
exata de **um firmware antigo que parkou os servos em 0° e não faz mais nada**.

---

## 🥇 Suspeita principal: a placa não está executando o que gravamos

Uma única hipótese explica as quatro observações sem nenhuma suposição extra: **o KL25Z continua
rodando uma imagem anterior**, e as gravações de hoje não chegaram a ele.

O que **não** sustenta o contrário:

- a gravação é aceita: `Firmware has been successfully uploaded`, e o `E:` (DAPLink) fica **sem
  `FAIL.TXT`** — que é o arquivo que o DAPLink escreve quando recusa;
- ⚠️ **mas isso é trabalho do OpenSDA, não do alvo.** O `E:` montar, a COM9 aparecer e o arquivo ser
  consumido são tudo do **K20 do OpenSDA**, um segundo microcontrolador alimentado direto dos 5 V da
  USB. Nada disso prova que o KL25Z foi reprogramado ou resetado.

Duas sub-hipóteses, nesta ordem:

1. **O alvo não é resetado depois de gravar.** O `pio run -t upload` avisa: *"Some boards may require
   manual hard reset"*. Se o reset automático não acontecer, a CPU **continua executando a imagem
   anterior** — e o sintoma é exatamente este.
2. **A gravação não chega à flash do alvo.** O DAPLink consome o arquivo e reporta sucesso sem
   reprogramar (flash protegida, estado ruim do OpenSDA).

### ⭐ O experimento que decide, e custa uma constante

Mudar o valor de repouso para algo **inconfundível** e gravar:

```c
/* em lib/teste_portas/teste_portas.c - so para este teste */
#define RODAR_FASES_PWM  1
#define CNV_REPOUSO      3750    /* 50% de duty = ~1,65 V, impossivel de confundir */
```

Depois de gravar, **apertar o SW1/RESET** e medir PTD3:

| Leitura | Conclusão |
|---|---|
| **~1,65 V** | a gravação funciona e o alvo executa. A falha está depois — e some a suspeita principal |
| **~0,165 V** ainda | ⛔ **confirmado: a imagem nova não está rodando.** Vá para "o que tentar" abaixo |

### O que tentar se a imagem não estiver rodando

1. **Reset manual** (SW1) e ciclo de energia completo depois de cada gravação — o mais barato;
2. **Trocar o protocolo de gravação.** Hoje é o disco (`upload_protocol = mbed`, automático). Tentar
   o caminho de depuração, que é completamente diferente:
   ```ini
   upload_protocol = cmsis-dap
   ```
   (usa pyocd; pode precisar de `pio pkg install` das dependências);
3. **Conferir se a flash está protegida** — imagem ruim pode setar o byte de segurança do Kinetis e
   fazer o programador falhar em silêncio. `pyocd erase --chip` resolve, se o pyocd subir;
4. **Mass erase pelo bootloader do OpenSDA**: segurar o RESET ao plugar a USB faz o K20 montar como
   `BOOTLOADER` em vez de `DAPLINK` — dá para regravar o firmware do OpenSDA por ali.

---

## 🥈 Suspeita secundária: o rail de 3,3 V do alvo

**Nunca foi medido, e é a única medição pedida que ficou sem resposta.** Ponta preta no GND, ponta
vermelha em **`P3V3`** do header:

- **~3,3 V** → o alvo tem alimentação; a suspeita principal continua sendo a de cima;
- ⛔ **abaixo disso** → o LDO está fora, e o KL25Z **não executa nada** enquanto o OpenSDA segue vivo
  nos 5 V. Explicaria LED, serial e servo de uma vez.

É plausível porque os servos estiveram ligados à placa hoje, e um curto ou sobrecarga no 3,3 V
derruba o alvo sem derrubar o OpenSDA. **Meça isso primeiro: são 30 segundos e pode encerrar o
assunto.**

⚠️ Note que essa suspeita **conflita parcialmente** com a principal: se o alvo não tem 3,3 V, ele
não estaria gerando PWM nenhum — e os 0,165 V medidos dizem que **está** gerando. Isso empurra a
suspeita do rail para baixo, mas não a elimina: o pulso pode ser resquício de antes de o rail cair.

---

## O que já foi DESCARTADO, com evidência

Não regaste estes; custaram horas.

| Hipótese | Por que caiu |
|---|---|
| Firmware nunca gravado | gravação aceita, sem `FAIL.TXT`, arquivo consumido pelo `E:` |
| Servidor de debug segurando o alvo *halted* | nenhum processo de MCUXpresso/LinkServer/J-Link/pyocd rodando |
| Junction `.mcuxpressotools` quebrada | a junction resolve, o conteúdo é legível, o venv aponta para um Python que existe |
| MCUXpresso como causa | **nada dele participa**: build com o GCC 8.2.1 do PlatformIO, gravação por disco do DAPLink |
| Zephyr 4.2 exigir GCC 11+ | falso — o `--param=min-pagesize` é **sonda**, e falhar é o previsto. Compila com GCC 8.2.1 |
| `ldr_inicializar()` falhar sem LDR conectado | falso — o driver valida canal/tempo/ganho, nunca o que está no pino |
| Power bank | provavelmente auto-desligamento por consumo baixo; trocado por tomada, sem mudança |

---

## Estado da bancada

- **Servos:** dois **Tower Pro MG90S**, alimentados por carregador de parede 5 V, **GND comum** com
  a placa (**DEC-16**). ⚠️ O terra comum foi instruído mas **não confirmado visualmente** — vale
  reconferir com continuidade;
- **LDRs:** ⚠️ **nenhum conectado.** O bloco do ADC está comentado no `main.c`;
- **Capacitor de reservatório:** não existe. O que há é um de **470 nF**, que é filtro, não reserva;
- **Firmware gravado:** `teste_portas.c` com só a Fase 1 ativa — os dois pinos juntos, 5 s em alto e
  5 s em baixo, com o LED vermelho espelhando o estado;
- **Serial:** a COM9 enumera e é aberta pelo monitor, mas **nunca recebeu um byte**.

## Onde o resto mora

| Assunto | Arquivo |
|---|---|
| Alimentação, terra comum, montagem | `03-hardware_specs/04-alimentacao-e-perguntas-abertas.md` |
| A decisão da fonte externa | `20-decisoes.md`, **DEC-16** |
| Fatores de calibração e limites de curso | `03-hardware_specs/03-atuadores-mg90s.md` |
| Armadilhas de build (cache, devicetree) | `10-debts_and_pending/04-dividas-de-projeto-e-repositorio.md`, **D-40** e **D-43** |
