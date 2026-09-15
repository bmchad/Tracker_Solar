# Arquitetura e ordem de execução

Esta pasta é **spec, não código**: descreve as alterações a fazer para os 4 LDRs. Nada aqui está
implementado ainda.

Ordem de implementação, e o porquê dela, em
[../01-project_vision/02-upgrade-para-4-ldr.md](../01-project_vision/02-upgrade-para-4-ldr.md).

## Grafo de módulos, depois do upgrade

```
                    ┌──────────────┐
                    │  src/main.c  │   laço de controle, e o único main()
                    └──────┬───────┘
         ┌─────────────┬───┴────┬─────────────┬──────────────┐
         ▼             ▼        ▼             ▼              ▼
    ┌────────┐  ┌───────────┐ ┌──────┐  ┌──────────┐  ┌───────────┐
    │ lib/ldr│  │lib/controle│ │lib/sol│ │lib/servo │  │lib/init_  │
    │        │  │  (novo)   │ │(renom)│ │ (renom)  │  │   hora    │
    └───┬────┘  └───────────┘ └──────┘  └────┬─────┘  └───────────┘
        │        puro: mV → graus            │
        ▼        sem hardware, testável      ▼
    Zephyr ADC                          ┌──────────┐
    (subsistema)                        │ lib/pwm  │  driver TPM, origem terceira
                                        └──────────┘
```

Três renomeações/criações:

| Diretório hoje | Vira | Por quê |
|---|---|---|
| `lib/angulo_incidência_solar/` | `lib/sol/` | remove o **acento** do caminho (D-32) e o escopo deixa de ser só um ângulo |
| `lib/servo_motor/` (`sevo_motor.c`) | `lib/servo/` (`servo.c`) | ⚠️ renomeado pela metade em 2026-09-15: o `tenpo` saiu, o **`sevo`** ficou (D-32) |
| — | `lib/controle/` | a mistura dos quadrantes e a decisão de passo, hoje inline no `main.c` |

⭐ **`lib/controle/` existe para ser puro.** Ele recebe quatro valores em mV e a posição atual, e
devolve os deltas de ângulo. Não toca ADC, não toca PWM, não chama `k_msleep`. É a única parte da
lógica que pode ser conferida sem placa na mesa — e num projeto sem framework de teste, "raciocinar
sobre" é a única verificação disponível, o que só funciona se a função não tiver efeito colateral.

`lib/pwm/pwm_z42.c` é **dependência**, não código do projeto: origem terceira, 2017. Tem bugs
conhecidos (D-20, D-21) que a decisão é não corrigir — ver
[../20-decisoes.md](../20-decisoes.md).

## Ordem de boot

```
1. servo_inicializar()      → pwm_tpm_Init(TPM0, ...) + os 2 canais
2. ldr_inicializar()        → os 4 canais de ADC; aborta se algum falhar
3. relogio_init()
4. posição de partida       → feedforward do lib/sol, NÃO 0° (ver spec 04)
5. [opcional] testes        → só com -DRODAR_TESTES_NO_BOOT (ver spec 06)
6. laço de controle
```

## Regra nova: `main()` é o único lugar que inicializa hardware

Hoje `teste_motores()` e `teste_ldr()` **reinicializam TPM0 e o ADC por conta própria** — as
mesmas cinco chamadas aparecem copiadas em três arquivos, então em todo boot o hardware é
inicializado três vezes (**D-02**).

Isso não é só redundância. `pwm_tpm_Init` faz `tpm->SC |= TPM_SC_CMOD(...) | TPM_SC_PS(...)` — um
**OR**, não uma escrita. Reinicializar com parâmetros diferentes **acumula bits** em vez de
substituí-los, e o prescaler resultante não é nenhum dos dois pedidos. Hoje passa porque os três
lugares pedem exatamente os mesmos valores; é uma armadilha armada, esperando o primeiro que mudar
um deles.

Depois do upgrade: as funções de teste **recebem** o hardware já pronto e nunca chamam `*_Init`.

## Contratos entre módulos

Três acoplamentos que não são visíveis no código e que quebram em silêncio se um lado mudar:

| Contrato | Os dois lados | O que acontece se divergirem |
|---|---|---|
| **Ordem de `io-channels`** | `zephyr/frdm_kl25z.overlay` ↔ `enum ldr_id_t` em `lib/ldr/ldr.h` | os sensores trocam de nome; o tracker vira ao contrário sem erro nenhum |
| **Sinal do erro** | topologia do divisor — pull-down, **P-03** fechada ↔ a mistura em `lib/controle/` | o painel foge do sol e para no batente |
| **Unidade da leitura** | `lib/ldr/` devolve mV ↔ `MARGEM_MV` em `lib/controle/` | os limiares deixam de significar o que o nome diz — é o que acontece hoje, D-03 |

Cada um está documentado na spec do módulo dono, com um comentário no próprio código apontando
para o outro lado.
