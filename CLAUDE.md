# Tracker solar: protótipo e projeto real

Este repositório guarda duas fases do mesmo projeto, e cada uma tem a sua pasta de documentação.
**Não misture as duas.**

| Pasta | Fase | O que contém |
|---|---|---|
| `context/` | **protótipo** | FRDM-KL25Z + Zephyr, 2 servos MG90S de rotação contínua, 4 LDRs. Documenta o código de `src/` e `lib/`, que também é do protótipo |
| `mega_context/` | **projeto real** | placa solar de 60 W, motor de vidro elétrico 12 V, corrente 20:1, um eixo só. As especificações estão em `mega_context/info.md` |

- ⚠️ **Exceção atual:** `src/main.c` não é mais o tracker do protótipo. É o **teste de bancada do
  potenciômetro** do projeto real (`mega_context/angulo.md`, seção 8), rodando na FRDM-KL25Z como
  instrumento e usando `lib/potenciometro/` (código do projeto real). O `main.c` do protótipo foi
  movido para `context/main.c`.
- Um fato do projeto real vai para `mega_context/`, nunca para `context/`.
- O que está em `context/` **não vale para o projeto real** sem ser revalidado: microcontrolador,
  ADC, alimentação, constantes, decisões e bugs são do protótipo.

## Escopo atual do projeto real (2026-11-19)

Por enquanto, os temas planejados em `mega_context/` são:

- o **cálculo do ângulo da placa** (`mega_context/angulo.md`): potenciômetro e acelerômetro;
- o **circuito do motor** (`mega_context/circuito.md`): controladora, bateria e três relés. A
  controladora é caixa-preta: carga e descarga da bateria não são planejadas.

Mecânica, orientação da estrutura, controle e firmware do projeto real **não** são planejados até
serem pedidos.
