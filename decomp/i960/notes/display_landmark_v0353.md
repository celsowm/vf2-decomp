# Entrada em tela — landmark cold-boot / attract (v0353)

## Definição operacional

“Entrar na tela” = o runtime provar, com evidência medida, um estado de
**tile-plane / paleta** produzido pela trajetória natural de cold-boot, com
fronteira diferencial ou unit-test pinada em C nativo.

Não se afirma que o logo SEGA 3D já está desenhado: nenhuma string ASCII
`SEGA` aparece nos ROMs nem no tile-plane medido desta trajetória.

## Landmarks medidos

| ID | Onde | Evidência | Nativo |
| --- | --- | --- | --- |
| L0a | `0x0004aff8` (checkpoint pós-boot, 2 985 244 insns desde stage1) | Tile `0x0008aa`: `BACKUP RAM IS BROKEN.` / `0x000934`: `INITIALIZED.` | `execute_post_boot_backup_restore` dest `0x010008aa` via thunk; unit test `test_post_boot_backup_broken_screen` |
| L0b | Continuação pós-boot (park `out/park-after-irq.vf2snap`, IP `0x9fa8`, flags `0x80000000`) | Tile `0x000c28`: `I/O Initialize ...`, `0x000c4e`: `OK.`, `0x000ca8`: `Sound Initialize ...` | `execute_post_boot_io_init` + unit test existente (`0x01000c28` / `OK.`) |
| L1 | `out/sixth-fresh.vf2snap` (native-sixth-dispatch MATCH, IP `0x0001645c`) | selector `0x11`, `0x5000a4=0x0b`, paleta 20 páginas, texture-ram0 preenchida, tiles `TEST MENU` / `MEMORY TEST` / … / `EXIT` | Corredor `native-sixth-dispatch` MATCH |

## Fronteira Sega / attract

- O cold-boot medido **assenta em TEST MODE** (selector 17) no sixth dispatch,
  com backup SRAM vazio/inicializado — coerente com o caminho de diagnóstico
  pós-`BACKUP RAM IS BROKEN.`
- O logo SEGA **não** é tile ASCII neste trajeto. Attract poligonal (TGP) e
  EXIT TEST MODE → warm-boot → attract seguem como fronteira explícita.
- Forçar `0x5000a4=0x8b` a partir do IP de `fa_game_info` **não** redesenha a
  tela (19 insns até `0x10dcc`); o worker selector-17 exige fronteira de
  frame-dispatch (`0xa6c0` / cluster final).

## Ferramentas (não commitadas como evidência proprietária)

- `tools/python/render_tile_plane.py` — grelha 64×48 a partir de `0x80xx` no
  tile-ram + PPM grosseiro (visualização host, não pixel Model 2).
- Parks locais em `out/screen-*.txt|.ppm` (fora do git).

## Validação desta fatia

- Unit: `vf2_native_runtime_tests` com `test_post_boot_backup_broken_screen`
  (tile `0x010008aa` = `BACKUP RAM IS BROKEN.` com nibble `0x80`).
- Unit existente: post-boot I/O text em `0x01000c28`.
- ROM-backed: `native-sixth-dispatch` permanece o pin da tela TEST MENU (L1).
- CTest / build MSVC Debug reportados no commit v0353.
