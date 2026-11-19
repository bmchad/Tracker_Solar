# Tracker solar

Desde 2026-11-19 este repositório é **dedicado ao tracker solar real**. O protótipo (servos e LDRs)
cumpriu sua função e ficou como histórico.

⭐ **Leia primeiro [mega_context/CLAUDE.md](mega_context/CLAUDE.md).** Ele explica a hierarquia entre
`context/` e `mega_context/`, como ler a documentação, as convenções e as armadilhas do ambiente.

O mínimo, caso você não abra:

- `mega_context/` é a **fonte da verdade** do projeto real. Fato novo vai para lá.
- `context/` é o **protótipo encerrado**: só histórico, não é mais atualizado, e nada de lá vale sem
  revalidar.
- O código do projeto real é `src/main.c` + `lib/motor_rele`, `lib/potenciometro` e `lib/pisca_led`. O
  resto de `lib/` é legado do protótipo e não entra no build.
