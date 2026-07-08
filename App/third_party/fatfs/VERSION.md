# FatFs — vendored copy

- **Source:** http://elm-chan.org/fsw/ff/arc/ff15a.zip (FatFs R0.15a, Revision ID 5380)
- **Vendored:** 2026-07-08
- **License:** BSD-style, see LICENSE.txt
- **Files:** ff.c, ff.h, diskio.h, ffconf.h (modified), ffunicode.c (kept for
  future LFN use; NOT compiled while FF_USE_LFN == 0)

## Local ffconf.h changes vs upstream defaults

| Option | Value | Why |
|---|---|---|
| `FF_USE_MKFS` | 1 | host tests format a RAM disk; unused on target |
| `FF_FS_NORTC` | 1 | no RTC; fixed timestamp (GPS-time hook possible later) |
| `FF_CODE_PAGE` | 437 | single code page, smallest footprint |
| `FF_USE_LFN` | 0 (default) | 8.3 names only (`FL_NNNN.BIN`), saves RAM |
| `FF_FS_REENTRANT` | 0 (default) | ALL FatFs calls are confined to the storage task |

Do not edit ff.c/ff.h/diskio.h locally; upgrade by re-vendoring a newer
archive and re-applying the ffconf.h table above.
