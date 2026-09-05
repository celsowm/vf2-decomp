# v0257 bare pure-bit21 fix — 0x00208140 etc 112 masks

Bare middle-high with only bit21 (0x00200000) and no low (0x16==0) and
no other middle was 0/36 DIFF -3/-5 via the 7 middle-high predicates
(8140 -3/-5, C140 +2/+5, 4140 +2/+4, 14140 +2/+4, 10140 +4/+8+bit11,
18140 +4/+9+bit11, 1C140 +2/+5, MIDDLE=0x1BFE3EA9, mask ~0xFFFE3EBF).
Reference is 0 excess for bare pure-bit21 (no low, no extra middle);
the helper's 0x17b68 bit23 bridge is not taken, so the 31-instruction
baseline is correct. Low 0x00208142 etc already 36/36; bare
0x00808140 (bit23 alone) correctly stays 36/36 via the same predicates.

Guard bare with ((low & 0x16)!=0 || (middle & 0x1BDE3EA9)!=0) where
0x1BDE3EA9 = 0x1BFE3EA9 & ~0x00200000 (any middle except pure bit21).
Pure-bit21 bare now falls through to native 0 and is 36/36
(7 bases ×16 outer =112 masks). Representative 0x00208140,
0x0020C140, 0x00204140, 0x00214140, 0x00210140, 0x00218140, 0x0021C140
plus outer 0x40208140 etc all 36/36, ctest 55/55.

Total positive masks 1895→2007. Remaining 0x17b68 branches
(runtime 0x00508000 bit5, position vs 0x0050a00c, 0x61c/0x804/0x618)
stay fail-closed.
