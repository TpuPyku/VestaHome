# BASS for Windows CE (ARM)

VestaHome's audio player and internet radio on the head unit (VestaHomeInstall puts these into
`\SDMMC\VestaHome\bass\`). The last Windows CE releases from Un4seen Developments, https://www.un4seen.com/
(forum topic "BASS for WinCE": "WinCE is no longer supported, but the last releases for it are still available"),
downloaded 2026-10-19:

| File | Version | From |
|---|---|---|
| bass.dll | 2.4.17 | www.un4seen.com/files/bass24-ce.zip (updated 4 Feb 2025) |
| bassflac.dll | 2.4.4 | the same |
| bassalac.dll | 2.4 | the same |
| bassopus.dll | 2.4.2 | the same |
| basswv.dll | 2.4.7 | the same |
| bass_aac.dll | 2.4.5 (MaresWEB) | www.un4seen.com/stuff/bass_aac-ce.zip |
| bass_mpc.dll | 2.4.1.3 (MaresWEB) | www.un4seen.com/stuff/bass_mpc-ce.zip |
| tags.dll | 18 | www.un4seen.com/stuff/tags-ce.zip |

All ARM (machine 0x1C0), subsystem CE 4.20, importing only COREDLL (and bass.dll). Before: BASS 2.4.12 (2016)
from `MAIN\SinglePlayer`.

License (BASS): free for non-commercial use; a commercial product needs a license from Un4seen (see bass.chm in
bass24-ce.zip). The add-ons by their authors, under the same terms. VestaHome is free.
