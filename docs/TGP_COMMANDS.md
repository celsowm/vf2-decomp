# TGP commands

The words the i960 pushes into the TGP FIFO (0x884000) are calls into the TGP
program VF2 2.1 uploads: a header word `(op << 23) | (op << 8) | op` for a 7-bit
opcode, then a fixed number of argument words. The program's main loop reads
the header (TGP PC 0x35) and jumps to the handler, which reads its arguments and
pushes its reply words, if any, into the output FIFO for the i960 to read back.

`decomp/tgp/commands.csv` is the table: one row per opcode 0x00..0x6f, with
`src/hardware/tgp.c`'s `tgp_commands[]` built from it
(`vf2_tgp_command_payload_words`, `vf2_tgp_command_reply_words`,
`vf2_tgp_command_name`).

## Where the numbers come from

Two independent sources, which agree on every opcode both have:

- **Names and argument counts in Sega's PS2 port.** Sega Ages 2500 Vol.16
  Virtua Fighter 2 (SLPM-62547) runs the arcade i960 program and carries its own
  table for the TGP functions, with each `Fn_*` name, its argument count and its
  reply size (catalogued in ages-decomp, `VF2/COP/INDEX.md`). Only the names
  and counts are used here.
- **A measurement.** m2-hle2 runs the game's uploaded TGP program on an
  emulated MB86234 (after MAME's mb86233). Its TGP tap logs every word the i960
  pushes, every word the TGP program reads from its FIFO with the PC that read
  it, and every reply the i960 reads. Over 3000 attract frames (Japan region),
  72 opcodes were sent; for each, the words the program read after a header
  before its next header read are the arguments, and the words it pushed are
  the replies. Every opcode had one fixed argument count across all its calls
  (`measured_commands` is the sample size), equal to the PS2 table's. The one
  data-dependent reply is `0x4a` `Fn_osage`, 44..190 words.

Reproduce with m2-hle2's `det_digest --cop FROM:TO:FILE` and
`tools/tgp-ops.py` (m2-hle2 `VF2-DECOMP.md`). The log is ROM-derived and stays
out of this repo; only the counts are recorded.

## What this corrects

`decomp/i960/notes/mame_3d_frame_geometry_format.md` read the stream as
Model 2 geometrizer commands and framed it by header words. Both readings were
wrong for the TGP FIFO:

- `0x01`/`0x02` are `Fn_push_matrix`/`Fn_pop_matrix` (0 arguments), not
  "object" and "direct". `0x3f` is `Fn_zyx_rot` with 3 arguments, not an end
  marker.
- `0x29`, `0x37`, `0x39` are fixed (3, 2, 3), not variable.
- `0x13` `Fn_add` takes 2 arguments, not 0.
- A header test cannot frame the stream: `0x00000000` (a 0.0 argument) is a
  valid header word for opcode 0x00. 36,696 of the 36,698 measured `Fn_add`
  calls carry such a word, so header delimiting split them. The lengths that
  matched (0x0b = 12, 0x34 = 1, 0x01/0x02 = 0) were right.

The geometrizer's own command stream (`vf2_tgp_scan_geometry_stream`,
`geometry_count_words`) is a different stream and is unchanged.

## Measured opcodes

| op | name | args | replies | calls |
|---|---|---:|---:|---:|
| `0x00` | `Fn_initialize` | 0 | 0 | 1 |
| `0x01` | `Fn_push_matrix` | 0 | 0 | 237224 |
| `0x02` | `Fn_pop_matrix` | 0 | 0 | 241252 |
| `0x03` | `Fn_base_matrix` | 0 | 0 | 8106 |
| `0x05` | `Fn_get_matrix` | 0 | 12 | 23924 |
| `0x06` | `Fn_trans` | 3 | 0 | 86553 |
| `0x07` | `Fn_scale` | 3 | 0 | 16114 |
| `0x0b` | `Fn_mul_matrix` | 12 | 0 | 4435 |
| `0x0c` | `Fn_inv_matrix` | 0 | 0 | 10514 |
| `0x0d` | `Fn_base_point` | 0 | 0 | 6381 |
| `0x0e` | `Fn_load_point` | 3 | 0 | 10512 |
| `0x0f` | `Fn_get_point` | 0 | 3 | 74586 |
| `0x11` | `Fn_load_3x3` | 9 | 0 | 8056 |
| `0x12` | `Fn_get_3x3` | 0 | 9 | 4028 |
| `0x13` | `Fn_add` | 2 | 1 | 36698 |
| `0x14` | `Fn_sub` | 2 | 1 | 1010 |
| `0x15` | `Fn_mul` | 2 | 1 | 2 |
| `0x16` | `Fn_div` | 2 | 1 | 5314 |
| `0x17` | `Fn_cvtws` | 1 | 1 | 453 |
| `0x18` | `Fn_cvtsw` | 1 | 1 | 73 |
| `0x1a` | `Fn_sqr` | 1 | 1 | 83 |
| `0x21` | `Fn_sin` | 1 | 1 | 14186 |
| `0x22` | `Fn_cos` | 1 | 1 | 12172 |
| `0x24` | `Fn_sinx` | 2 | 1 | 4680 |
| `0x25` | `Fn_cosx` | 2 | 1 | 2024 |
| `0x26` | `Fn_asin` | 1 | 1 | 2013 |
| `0x27` | `Fn_atan` | 2 | 1 | 18275 |
| `0x29` | `Fn_point_trans` | 3 | 3 | 113090 |
| `0x2b` | `Fn_get_2d_r` | 4 | 1 | 4665 |
| `0x2c` | `Fn_get_3d_r` | 6 | 1 | 2013 |
| `0x2d` | `Fn_get_2d_len` | 2 | 1 | 29268 |
| `0x2f` | `Fn_get_2d_dir` | 4 | 1 | 8121 |
| `0x30` | `Fn_regular_vector` | 3 | 3 | 2339 |
| `0x31` | `Fn_fcurve_lin` | 4 | 1 | 7129 |
| `0x32` | `Fn_fcurve_spl` | 6 | 1 | 95246 |
| `0x34` | `Fn_mov_matrix` | 1 | 1 | 179712 |
| `0x35` | `Fn_st_unit_mat` | 2 | 0 | 64448 |
| `0x36` | `Fn_ld_unit_mat` | 2 | 0 | 14399 |
| `0x37` | `Fn_mul_unit_mat` | 2 | 0 | 138015 |
| `0x38` | `Fn_coli_set_ball_adrs` | 1 | 0 | 4028 |
| `0x39` | `Fn_coli_point_trans` | 3 | 0 | 120840 |
| `0x3a` | `Fn_area_table_gen` | 0 | 0 | 1997 |
| `0x3b` | `Fn_calc_coli_flag` | 5 | 4 | 1618 |
| `0x3d` | `Fn_coli_trans_mat` | 6 | 0 | 1618 |
| `0x3e` | `Fn_coli_trans_xz` | 5 | 0 | 1997 |
| `0x3f` | `Fn_zyx_rot` | 3 | 0 | 20147 |
| `0x41` | `Fn_kage_leave_x_axis` | 0 | 0 | 48322 |
| `0x42` | `Fn_kage_leave_z_axis` | 0 | 0 | 12081 |
| `0x43` | `Fn_get_matrix_inner` | 1 | 0 | 20464 |
| `0x44` | `Fn_load_matrix_inner` | 1 | 0 | 29372 |
| `0x45` | `Fn_mul_matrix_inner` | 1 | 0 | 10512 |
| `0x46` | `Fn_mul_matrix_inner_rev` | 1 | 0 | 60403 |
| `0x4a` | `Fn_osage` | 1 | variable | 4028 |
| `0x54` | `Fn_get_sm_ang_f` | 9 | 3 | 481 |
| `0x55` | `Fn_get_sm_ang_r` | 9 | 3 | 48 |
| `0x56` | `Fn_get_x_axis` | 0 | 3 | 5668 |
| `0x57` | `Fn_get_y_axis` | 0 | 3 | 4470 |
| `0x59` | `Fn_get_inner_2d` | 4 | 1 | 11336 |
| `0x5a` | `Fn_regular_vector_2d` | 2 | 2 | 5668 |
| `0x5b` | `Fn_rot_2d` | 3 | 2 | 11591 |
| `0x5c` | `Fn_add3` | 6 | 3 | 570 |
| `0x5e` | `Fn_mul3` | 4 | 3 | 2299 |
| `0x62` | `Fn_calc_unit_hara` | 9 | 0 | 4028 |
| `0x63` | `Fn_get_loc_pos` | 7 | 3 | 50 |
| `0x67` | `Fn_st_glb_mat` | 1 | 0 | 10070 |
| `0x69` | `Fn_mul_mot_yrot` | 2 | 0 | 40280 |
| `0x6a` | `Fn_glo_to_loc` | 3 | 3 | 10217 |
| `0x6b` | `Fn_calc_unit_2_fast` | 17 | 1 | 16112 |
| `0x6c` | `Fn_x_rot_e` | 0 | 0 | 22042 |
| `0x6d` | `Fn_y_rot_e` | 0 | 0 | 58305 |
| `0x6e` | `Fn_z_rot_e` | 0 | 0 | 26949 |
| `0x6f` | `Fn_trans_e` | 0 | 0 | 32172 |

Opcodes 0x00..0x6f not listed have a PS2 name and count in the CSV but were not
seen in the run; `vf2_tgp_command_payload_words` returns -1 for them (fail
closed) and the framer falls back to header delimiting with
`length_verified = 0`.
