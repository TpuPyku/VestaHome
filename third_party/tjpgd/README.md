# TJpgDec R0.03

Tiny JPEG Decompressor by ChaN, http://elm-chan.org/fsw/tjpgd/ (`tjpgd3.zip`, 2021). Used by VestaHome's
photos: baseline JPEG decoded straight to RGB565 and scaled down by 1/2, 1/4 or 1/8 while decoding, with a few
KB of working memory (a 12 MP photo never needs its full size in RAM).

The sources are as released, except `tjpgdcnf.h`: `JD_SZBUF 4096` (was 512), `JD_FORMAT 1` (RGB565, was RGB888),
`JD_FASTDECODE 1` (32-bit barrel shifter, was 0).

License (from `tjpgd.c`): free software, no warranty; no restriction on use; may be used, modified and
redistributed for personal, non-profit or commercial products; redistributions of source code must retain the
copyright notice. Copyright (C) 2021, ChaN.
