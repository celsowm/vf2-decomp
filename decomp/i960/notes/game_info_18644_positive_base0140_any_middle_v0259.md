# v0259 base 0x140 any-middle (Mp!=0) — ~134M masks +3/+6

ROM-backed `fighter0_state==8 && fighter1_state==8 &&
measured_matrix_distribution && shared_fighter_threshold>=0` with

- `MIDDLE = 0x1BFE3EA9` (20 bits incl. bit23), `Mp = M & ~0x00200000 = 0x1BDE3EA9` (19 bits)
- `base = (combined & ~0xFFFE3EBF) == 0x140` (bare 0x140, pure bit21 0x00200140 are 0/0)
- `Mp !=0`  (any non-empty subset, with or without bit21) -> +3 uni / +6 bi
  - Single 0x340/0x940/0x1140/0x2140 etc, double 0xB40/0x1940/0x1340, quad 0x3D40,
    high 0x60140/0x8000340, bit21+Mp 0x00200340/0x00200940/0x00260140/0x08200340 etc
    all measured DIFF +3 uni / +6 bi before, 36/36 after.
  - Low 8 variants (0x02/0x04/0x10 etc) and outer 16 uniformly same.
  - Counts: without bit21 `2^19-1=524287` *128=67,108,736,
    with bit21 `524287*128=67,108,736`, total `134,217,472` (replaces v0258 singles 2432).
- Accounting: `native_instructions -= bl?6:3` (native overcounts), `COMPARE LESS/EQUAL`, `stale_low 0x41000000`.

Total `2007 + 134,217,472 = 134,219,479` positive masks 36/36.

Next frontier: other bases (0x40, 0x340 etc with different bases) and helper runtime bit5.
