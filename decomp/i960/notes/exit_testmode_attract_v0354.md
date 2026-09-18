# EXIT TEST MODE + warm-boot attract (v0354)

## Recipe medido (oracle, parks em `out/`, não commitados)

1. `native-sixth-dispatch` → `out/sixth-fresh.vf2snap` (MATCH).
2. `resume-trace` até `0x0000a6c0` → `out/park-sixth-a6c0.vf2snap` (1895 passos).
   Estado: selector `0x11`, `a4=0x0b`, TEST MENU, countdown `639`.
3. Forçar TEST-mode exit na fronteira de frame-dispatch:
   `vf2probe --snapshot park-sixth-a6c0 --set-u8 0x005000a4=0x8b --set-u8 0x005000a5=0x00 --set-u32 0x00500024=320 --until 0x0000a010`
   → **13286** insns (first-visit normal, nota selector17_index11),
   `a4=0x8b`, `a5=0xff`, countdown **320**, tile **`EXIT TEST MODE`**.
4. Terminal: `--set-ip 0xa6c0 --set-u32 0x00500024=1 --until 0x000000b0`
   → **13194** insns, IP **`0x000000b0`**, countdown 0, `0x500082=0x8000`.
   Tile-plane limpo (sem texto de menu).
5. Warm-boot pós-exit (`resume-trace` → `0xa6c0`, ~5.5M passos de máquina):
   - backup **válido** (CRC via `0x9480`, **não** `BACKUP RAM IS BROKEN`);
   - primeiro frame-dispatch: selector **`0x00`** (signature cold/attract);
   - tiles ainda com resíduo `I/O Initialize` / `Sound Initialize`.
6. Selector **0 → 2** em **exatamente 34** insns (fast path assinatura) →
   `out/park-warm-f01`.
7. Próximos frames: selector **2**, depois **`0x10`** (via `0x10a0c` →
   `0x6d478`/`0x58e90`), limpa o plano em `0x8ef0` e redesenha com `0x7fc0`.
   No frame seguinte o oracle **volta a TEST MENU** (selector `0x11`,
   `a4=0x0b`) e **arma coli** `entry=0x221e8` (slot10 `0x80000000`).
8. **Nenhuma string tile `SEGA`** em nenhum ponto medido. Geometria FIFO
   tem comandos float vivos (ex. `0x3f800000`) — possível conteúdo poligonal
   não decodificado como logo em tiles.

## Conclusão fail-closed

- Telas nativas/medidas: diagnóstico pós-boot, TEST MENU, **EXIT TEST MODE**,
  e o ciclo warm-boot que **retorna** a TEST MENU neste estado de config.
- “Tela Sega” (logo attract) **não** é witness tile-plane nesta trajetória;
  permanece fronteira TGP/attract com outro estado de jogo/config.
- C já documenta/recovera o worker `0x5ef60` (notes selector17_index11);
  contagens first-visit/terminal **reproduzidas** no oracle nesta sessão.

## Parks locais

`park-sixth-a6c0`, `park-exit-8b`, `park-exit-terminal`,
`park-warm-attract`, `park-warm-f01`, `park-warm-sel2`, `park-warm-sel3`,
`park-warm-sel3b` + dumps `out/screen-*.txt`.
