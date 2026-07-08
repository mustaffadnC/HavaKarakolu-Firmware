#!/usr/bin/env python3
"""HK flight-log reader: decode FL_NNNN.BIN files written by the storage
service into CSV, with CRC verification and resync past corrupted regions.

Frame format (little-endian, see App/services/storage/storage.h):
    'H' 'K' | ver(1) | type(1) | len(1) | payload(len) | crc16(2)
    crc16 = CRC-16/CCITT-FALSE over ver..payload, stored little-endian.

Usage:
    python3 hk_log_reader.py FL_0001.BIN            # decoded CSV to stdout
    python3 hk_log_reader.py FL_0001.BIN -o out.csv
    python3 hk_log_reader.py --selftest             # spec self-check (CI)
"""

from __future__ import annotations

import argparse
import struct
import sys

MAGIC = b"HK"
VERSION = 1

T_META, T_ENV, T_IMU, T_GPS, T_EVENT = 1, 2, 3, 4, 5

STATES = ["BOOT", "SELFTEST", "ATTACHED", "ARMED", "RELEASE",
          "DESCENT", "LANDED", "RECOVERY"]


def state_name(i: int) -> str:
    return STATES[i] if 0 <= i < len(STATES) else f"?{i}"


def crc16_ccitt(data: bytes, seed: int = 0xFFFF) -> int:
    crc = seed
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def parse_frames(blob: bytes):
    """Yield (type, payload) tuples; resync on garbage. Returns stats via
    StopIteration value pattern replaced by a trailing dict sentinel."""
    pos = 0
    good, bad_crc, resyncs = 0, 0, 0
    n = len(blob)
    while pos + 7 <= n:
        if blob[pos:pos + 2] != MAGIC or blob[pos + 2] != VERSION:
            pos += 1
            resyncs += 1
            continue
        plen = blob[pos + 4]
        total = 5 + plen + 2
        if pos + total > n:
            # looks like a frame but runs past the end: either a torn tail or
            # a fake magic mid-stream -- resync, do not give up on the rest
            pos += 1
            resyncs += 1
            continue
        crc_stored = blob[pos + 5 + plen] | (blob[pos + 6 + plen] << 8)
        crc_calc = crc16_ccitt(blob[pos + 2:pos + 5 + plen])
        if crc_stored != crc_calc:
            pos += 1
            bad_crc += 1
            continue
        good += 1
        yield blob[pos + 3], bytes(blob[pos + 5:pos + 5 + plen])
        pos += total
    yield None, {"good": good, "bad_crc": bad_crc, "resyncs": resyncs,
                 "tail_bytes": n - pos}


def decode(blob: bytes, out):
    rows = 0
    stats = {}
    out.write("type,t_ms,fields...\n")
    for typ, payload in parse_frames(blob):
        if typ is None:
            stats = payload
            break
        if typ == T_META:
            t, fw, rst, ver = struct.unpack("<IHBB", payload)
            out.write(f"META,{t},fw=0x{fw:04X},reset={rst},logver={ver}\n")
        elif typ == T_ENV:
            t, st = struct.unpack_from("<IB", payload)
            v = struct.unpack_from("<9f", payload, 5)
            out.write(f"ENV,{t},{state_name(st)},"
                      f"p={v[0]:.1f},alt={v[1]:.1f},t_bmp={v[2]:.2f},"
                      f"t1={v[3]:.2f},rh1={v[4]:.1f},t2={v[5]:.2f},rh2={v[6]:.1f},"
                      f"vbat={v[7]:.2f},soc={v[8]:.2f}\n")
        elif typ == T_IMU:
            t = struct.unpack_from("<I", payload)[0]
            v = struct.unpack_from("<8f", payload, 4)
            out.write(f"IMU,{t},ax={v[0]:.2f},ay={v[1]:.2f},az={v[2]:.2f},"
                      f"gx={v[3]:.3f},gy={v[4]:.3f},gz={v[5]:.3f},"
                      f"roll={v[6]:.1f},pitch={v[7]:.1f}\n")
        elif typ == T_GPS:
            t = struct.unpack_from("<I", payload)[0]
            lat, lon = struct.unpack_from("<2d", payload, 4)
            alt, spd, crs = struct.unpack_from("<3f", payload, 20)
            sats, fixq, valid = struct.unpack_from("<3B", payload, 32)
            out.write(f"GPS,{t},lat={lat:.6f},lon={lon:.6f},alt={alt:.1f},"
                      f"spd={spd:.1f},crs={crs:.1f},sats={sats},fix={fixq},"
                      f"valid={valid}\n")
        elif typ == T_EVENT:
            t, frm, to, arg = struct.unpack("<IBBH", payload)
            out.write(f"EVENT,{t},{state_name(frm)}->{state_name(to)},arg={arg}\n")
        else:
            out.write(f"UNKNOWN({typ}),?\n")
        rows += 1
    return rows, stats


# ---------------------------------------------------------------- selftest --

def _frame(typ: int, payload: bytes) -> bytes:
    body = bytes([VERSION, typ, len(payload)]) + payload
    crc = crc16_ccitt(body)
    return MAGIC + body + bytes([crc & 0xFF, crc >> 8])


def selftest() -> int:
    env = struct.pack("<IB9f", 1500, 2, 101325.0, 1.5, 25.0,
                      24.5, 40.0, 26.0, 42.0, 11.8, 0.9)
    event = struct.pack("<IBBH", 2100, 2, 3, 0)
    meta = struct.pack("<IHBB", 0, 0x0200, 3, VERSION)

    blob = (b"\xde\xad" +                    # leading garbage
            _frame(T_META, meta) +
            _frame(T_ENV, env) +
            b"HK\x01" +                      # torn header
            _frame(T_EVENT, event) +
            b"\x00\x00\x00")                 # trailing garbage

    # corrupt a COPY of the env frame and splice it in: must be skipped
    bad = bytearray(_frame(T_ENV, env))
    bad[9] ^= 0xFF
    blob += bytes(bad)

    import io
    out = io.StringIO()
    rows, stats = decode(blob, out)
    text = out.getvalue()

    ok = (rows == 3
          and stats["good"] == 3
          and stats["bad_crc"] >= 1
          and stats["resyncs"] >= 1
          and "ENV,1500,ATTACHED" in text
          and "EVENT,2100,ATTACHED->ARMED" in text
          and "META,0,fw=0x0200" in text)
    print(text)
    print(f"selftest: rows={rows} stats={stats} -> {'OK' if ok else 'FAIL'}")
    return 0 if ok else 1


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("binfile", nargs="?", help="FL_NNNN.BIN to decode")
    ap.add_argument("-o", "--output", help="write CSV here (default stdout)")
    ap.add_argument("--selftest", action="store_true",
                    help="verify the decoder against the frame spec")
    args = ap.parse_args()

    if args.selftest:
        return selftest()
    if not args.binfile:
        ap.print_help()
        return 2

    with open(args.binfile, "rb") as f:
        blob = f.read()

    out = open(args.output, "w", encoding="utf-8") if args.output else sys.stdout
    try:
        rows, stats = decode(blob, out)
    finally:
        if args.output:
            out.close()

    print(f"{rows} records; stats: {stats}", file=sys.stderr)
    return 0 if rows > 0 else 1


if __name__ == "__main__":
    sys.exit(main())
