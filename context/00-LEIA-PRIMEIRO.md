# LEIA PRIMEIRO — contexto do Tracker Solar

Tracker solar de **dois eixos** rodando **Zephyr** numa **FRDM-KL25Z**. Dois servos MG90S movem
uma placa: um no eixo horizontal (azimute), um no vertical (inclinação). LDRs ligados ao `ADC0`
dizem para onde está o sol.

O protótipo está passando de **2 LDRs para 4**. Nesta virada o eixo vertical deixa de ser malha
aberta (hoje ele vem de um cálculo astronômico) e passa a ser **malha fechada** pelo par
cima/baixo, com o cálculo virando *feedforward*.

⛔ **CHEGANDO AGORA? COMECE POR [CONTINUAR-AQUI.md](CONTINUAR-AQUI.md).** Há uma investigação aberta
e sem conclusão — **os servos não se movem**, e três saídas independentes estão mudas (serial, LED e
servo). Aquele arquivo tem o que foi medido, o que já foi **descartado com evidência** e qual é o
próximo experimento. Sem ele, você vai regastar hipóteses que custaram horas.

⚠️ **Este projeto nasceu de um carrinho seguidor de linha, e isso deixou marcas.** O projeto CMake
ainda se chama `carrinho1` (`zephyr/CMakeLists.txt`), e existe um repositório git órfão dentro de
`src/.git`. Está catalogado em
[10-debts_and_pending/04-dividas-de-projeto-e-repositorio.md](10-debts_and_pending/04-dividas-de-projeto-e-repositorio.md).
⭐ O `README.md` da raiz **já foi reescrito** e descreve o tracker — essa parte do **D-33** está
resolvida, e o **D-30** (código fora do git) também — tudo o que havia de pendente entrou em
`a411ae6`.

---

## Por onde começar

| Se você quer… | Leia |
|---|---|
| ⛔ **retomar a investigação em aberto** | [CONTINUAR-AQUI.md](CONTINUAR-AQUI.md) |
| entender o que o produto é | [01-project_vision/01-o-que-e-o-tracker.md](01-project_vision/01-o-que-e-o-tracker.md) |
| saber o que muda com 4 LDRs | [01-project_vision/02-upgrade-para-4-ldr.md](01-project_vision/02-upgrade-para-4-ldr.md) |
| **escrever o código** do upgrade | [02-software_specs/](02-software_specs/), na ordem 00 → 06 |
| ligar fio / escolher pino | [03-hardware_specs/01-plataforma-e-pinagem.md](03-hardware_specs/01-plataforma-e-pinagem.md) |
| saber o que já está quebrado | [10-debts_and_pending/00-indice.md](10-debts_and_pending/00-indice.md) |
| entender **por que** foi assim | [20-decisoes.md](20-decisoes.md) |

## Mapa

```
context/
├── 00-LEIA-PRIMEIRO.md          este arquivo
├── 20-decisoes.md               o porquê de cada escolha, com o rejeitado ao lado
├── 01-project_vision/           o que o tracker é, e o escopo desta virada
├── 02-software_specs/           as alterações de código para os 4 LDRs (spec, não código)
├── 03-hardware_specs/           pinagem, sensores, atuadores, alimentação
└── 10-debts_and_pending/        bugs achados na varredura + o que falta decidir
```

## Regra de dono único

**Um fato mora em um arquivo só.** Foi assim que o `docs/` de outros projetos apodreceu: o mesmo
número em dois lugares, um deles envelhecendo em silêncio.

Na prática:

- **Pinagem física** (qual fio em qual pino) mora em `03-hardware_specs/01-plataforma-e-pinagem.md`.
  O overlay do devicetree que traduz isso é descrito em `02-software_specs/01-devicetree-adc-4-canais.md`,
  que **referencia** aquela tabela em vez de repeti-la.
- **Bug** mora em `10-debts_and_pending/`, com um ID `D-nn` estável. As specs citam **pelo ID**,
  nunca redescrevendo o bug.
- **Pendência** (algo a decidir, não algo quebrado) mora em
  `10-debts_and_pending/05-pendencias-de-hardware.md`, com ID `P-nn`. Todo `A DEFINIR` espalhado
  pelos outros arquivos tem um `P-nn` correspondente.
- **Constante numérica** (`375`, `7500`, `1.21`, `1.47`) é definida em um arquivo e citada nos
  outros.

Mudou um fato? Corrija **no dono**, não onde você o encontrou.

## Estado do repositório

- Remoto: `https://github.com/bmchad/Tracker_Solar.git`, branch `main`.
- Último commit: `d7e4db2` — *"Código antigo, usando apenas 2 LDR's e servo motores sg90 simples"*.
- Árvore suja: `src/main.c` modificado, `lib/servo_motor/` e `lib/testes/` **untracked** (D-30).
- O histórico mostra a origem: `carrinho_seguindo_linha` → `carrinho_tracker_3_sensores` → tracker
  solar. Referência a "carrinho" no repositório é resquício, não recado.
