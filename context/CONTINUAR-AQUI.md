# CONTINUAR AQUI — estado da investigação, 2026-09-14

Documento de passagem entre sessões. **Não é spec nem decisão** — é o retrato de uma investigação,
com o que foi medido, o que foi descartado e qual é o próximo experimento. Quando o caso fechar, o
que sobrar de durável migra para `10-debts_and_pending/` ou `20-decisoes.md` e este arquivo é
apagado.

---

## ✅ RESOLVIDO no fim do dia: a placa nunca recebeu as gravações

⭐ **Confirmado por comparação controlada:** o mesmo binário, na mesma placa, gravado de **outro
computador**, rodou os testes imediatamente. Nesta máquina, **nenhum upload chegou ao alvo** — apesar
de o terminal sempre dizer `Firmware has been successfully uploaded` e de o `E:` nunca ter escrito
`FAIL.TXT`.

Isso encerra de uma vez tudo o que estava em aberto abaixo:

| O que parecia problema | O que era |
|---|---|
| Servos não se movem | a placa rodava uma **imagem antiga** — a de antes das edições do dia |
| LED não pisca | idem: o firmware do LED nunca foi gravado |
| Serial muda | idem |
| 0,16 V estáveis nos pinos | a assinatura da imagem antiga: `CnV = 375`, os servos parkados em 0° |

⭐ **A leitura dos 0,16 V estava certa e foi o que apontou o caminho.** Vale guardar o raciocínio: 5%
de duty sobre 3,3 V = pulso de 1 ms a 50 Hz = 0° de servo. A técnica está em
[../02-software_specs/06-bringup-e-testes.md](02-software_specs/06-bringup-e-testes.md), seção
"Medir PWM com multímetro".

### O que sobra, e é bem mais estreito

**Por que a gravação falha silenciosamente nesta máquina** (a de Windows 11, com PlatformIO em
`D:\DevTools\.platformio`). O caminho que falha é `firmware.bin` → disco `E:` do DAPLink: o arquivo
é consumido, o DAPLink não reclama, e o alvo não é reprogramado.

⚠️ **Não é o VSCode.** Quem copia o arquivo é o `pio.exe`; o editor só hospeda o terminal.
⚠️ **E reinstalar o toolchain não ataca isso** — o toolchain compila, não grava.

**Na ordem, do mais barato ao mais caro:**

1. **Copiar o `.bin` à mão** para o `E:` pelo Explorador e resetar. Funcionou? o culpado é o
   uploader do PlatformIO. Falhou igual? é Windows/DAPLink, e reinstalar o PlatformIO não ajudaria;
2. **Trocar o protocolo** para um caminho que não usa sistema de arquivos:
   ```ini
   upload_protocol = cmsis-dap    ; via pyocd, pela interface de depuracao
   ```
   Imune a antivírus, cache de escrita e Controlled Folder Access — os suspeitos clássicos de
   "copiei e o DAPLink não programou";
3. **Antivírus / Acesso Controlado a Pastas** do Windows bloqueando escrita em unidade removível;
4. **Atualizar o firmware do DAPLink** (segurar RESET ao plugar monta o `BOOTLOADER`);
5. Só então, reinstalar.

⭐ **E enquanto isso: a outra máquina grava.** Dá para editar aqui e gravar lá sem bloquear a
bancada.

---

## ⚠️ Antes de qualquer coisa, numa máquina nova

**`lib/teste_portas/teste_portas.c` tem alterações NÃO COMMITADAS** (o commit mais recente é
`91b27a9`). O binário que está gravado na placa foi compilado dessas alterações. Se você clonar em
outro computador, vai pegar a versão anterior do arquivo, que se comporta **diferente**: aciona um
pino de cada vez em vez dos dois juntos.

Commite antes de trocar de máquina, ou recompile sabendo dessa diferença.

---

---

## ✅ A PLACA ESTÁ EXONERADA — e a suspeita mudou de lugar

**Medido na máquina que grava:** com o teste de portas rodando, **PTD3 e PTD0 alternam em sincronia
com o LED** — tensão alta enquanto o LED acende, 0 V enquanto apaga. Isso encerra, de uma vez:
firmware roda, GPIO funciona, mux está certo, jumper conduz. Nenhuma das suspeitas de placa, clock,
boot ou pino sobrevive.

❌ **Nível lógico marginal — DESCARTADO.** Chegou-se a medir **2,8 V** de nível alto, o que
levantaria a suspeita de o sinal de 3,3 V ficar perto demais do limiar de um servo alimentado a
5 V. **Não se sustentou: a placa foi trocada e a nova entrega 3,3 V cheios.** Era característica
daquela unidade, não do projeto.

**Hipóteses para o servo não obedecer, agora que a placa está descartada:**

1. **Terra comum ausente ou mal feito.** Foi instruído, nunca confirmado com continuidade. Sem
   retorno comum o servo ignora o pulso, e o sintoma é exatamente "sinal correto, servo parado" —
   ⭐ é a primeira a conferir, e conferir é encostar as pontas do multímetro em continuidade entre o
   GND da placa e o GND do servo;
2. **O próprio servo.** Testar um dos dois num servo tester ou numa fonte de pulso conhecida;
3. **Alimentação do servo** — tensão nos terminais **dele**, não na saída do carregador.

⚠️⚠️ **EXISTEM DUAS PLACAS NESTA INVESTIGAÇÃO.** A troca aconteceu em 2026-09-14, no fim do dia.
Boa parte do que está registrado abaixo — os 0,16 V estáveis, o LED que não piscava, a serial muda —
foi medido na **placa antiga**. Ao retomar, **diga sempre de qual placa é cada medição**, senão
observações de duas peças diferentes viram uma contradição que não existe.

⭐ **E o recibo de cor funcionou:** o LED daquela placa está **VERMELHO**, o que significa que ela
tem o build **anterior** ao da mudança para verde. O mecanismo faz o que prometia — diz qual imagem
está gravada.

---

# ⬇️ Daqui para baixo: o registro de COMO se chegou lá

Mantido porque a trilha de descarte vale mais que a conclusão — é ela que impede regastar hipótese.
⚠️ **Tudo abaixo descreve o estado ANTES da confirmação acima.**

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

## 🥇 Suspeita principal — ✅ **CONFIRMADA**: a placa não está executando o que gravamos

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

## 🥈 Suspeita secundária: o rail de 3,3 V do alvo — ❌ **caiu junto**

A placa rodou normalmente quando gravada do outro computador, então o rail de 3,3 V está bom. Fica
registrado o raciocínio, porque a medição continua valendo como primeiro passo em qualquer sintoma
de "o alvo não executa":

**Nunca foi medido, e era a única medição pedida que ficou sem resposta.** Ponta preta no GND, ponta
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
