# v0371 — Status-tail modes no oráculo (ROM real) + thunk attract

## 1. Status-tail `0x4d25c` — oracle park attract

Fonte: park `out/attr-fs/all0-ready1.vf2snap` (sel=3, phase14,
ready forçado 1 na entrada do probe, counters 0, board bit9 clear).

ROM maincpu nas sources do tail (bytes reais):

| Addr | Payload |
| --- | --- |
| `0x0004d28c` | **15× `0x20` + NUL** (special / dest `0x010040e2`) |
| `0x0004d2ac` | **15× `0x20` + NUL** (common / dest `0x010000e2`) |
| `0x0004d2e8` | `TEX` + espaços (outra linha texture) |

`0x7fc0` / `copy_diagnostic_text`: byte fonte → halfword
`0x8000 | byte` no tile dest.

### Medidas oracle (`--set-ip 0x4d25c --until 0x4d2bc`)

| Mode `0x50002b` | Passos | Tile dests escritos | Glyphs |
| --- | ---: | --- | --- |
| **0x03** (park natural) | **156** | só `0x010000e2` | `20 80` ×N (space) |
| **0x0c** | **305** | `0x010040e2` **e** `0x010000e2` | spaces ambos |
| **0x0d** | **307** | idem 0x0c | spaces ambos |

- Delta **0x0d − 0x0c = +2** instruções — coincide com o pin C
  (`selector13 == selector12 + 2` em `test_texture_status_tail.c`).
- Continuar `0x4d29c→0x4d2bc` após special (mode0c): +**151** passos,
  blit common em `0x010000e2`.
- `ready` no park **não** é limpo pelo tail (entra 1 e permanece 1);
  o clear de `0x550000` é o final-status `0x4bf90` (v0368/v0370).

C híbrido: `execute_texture_status_tail_dispatch` (mode 0x0c/0x0d →
dois thunks special+common) e `execute_texture_status_tail` (outros
modes → common only). Unit nova `run_common_only_spaces` pinna
mode=3: só common dest com `0x8020`; special intacto.

## 2. Thunk attract phase14

| Probe | Passos | Resultado |
| --- | ---: | --- |
| `0xc0a4 → 0x9444` (ctr=0, ready≠1) | **11** | sem tile writes (só `0x500068`) |
| `0x9444 → 0x9468` | **15** | sem tile writes neste park |

ROM phase14 not-ready: `g9=0x01000ef4`, `balx 0x9444`. O blit em
`0x7fc0` usa `g0` como fonte C-string; neste park a fonte herdada
não gera writes de tile observáveis até `0x9468` (stream `bx`
indireto segue). **Não** é witness de logo 3D.

## 3. Fail-closed / logo

- Logo 3D: **não** witness (tail/thunk = texto tile `0x80xx`).
- Phase14 not-ready e phase15 masks especiais: fail-closed v0369.
- Status-tail 0x0c/0x0d: **recuperados** no dispatch C + pins unit
  (synthetic + espaços mode3); contagens oracle documentadas.

## Tools

- `tools/python/measure_status_tail_thunk.py`

## Validação observada

- `vf2_texture_status_tail` **Passed** (inclui `run_common_only_spaces`).
- Focused CTest Debug: **15/15 Passed** (orchestrator_*, texture_*,
  native_runtime, player_270d4, first_dispatch, hybrid_first,
  native_sixth, texture_bridge_differential).
- Logo 3D: **não** witness.

## Fronteiras irmãs

- Differential ROM-backed byte-exact do tail (glyphs `2080` vs C).
- Handoff completo thunk phase14 (`g0`/stream).
- Quem grava phase após vídeo no hardware real (não visto em 80k).
- phase15 clusters (v0369).
- `phase17_zero` CC — separada.
