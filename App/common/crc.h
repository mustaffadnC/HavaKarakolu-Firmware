#ifndef HK_CRC_H
#define HK_CRC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Sensirion CRC-8 (SHT4x, SCD4x ...): poly 0x31, init 0xFF,
 * no input/output reflection, final XOR 0x00.
 */
uint8_t hk_crc8_sensirion(const uint8_t *data, size_t len);

/*
 * CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, xorout 0x0000.
 * Used for telemetry framing.
 */
uint16_t hk_crc16_ccitt(const uint8_t *data, size_t len);

/* Streaming variant: feed one byte at a time. Seed with 0xFFFF. */
uint16_t hk_crc16_ccitt_update(uint16_t crc, uint8_t byte);

/*
 * CRC-16/XMODEM: same poly 0x1021, but init 0x0000. Used by the SD card
 * data-block CRC (CMD17/CMD24 transfers).
 */
uint16_t hk_crc16_xmodem(const uint8_t *data, size_t len);

/*
 * CRC-7 for SD/MMC command frames: poly 0x09, init 0x00, MSB-first.
 * Returns the 7-bit CRC already shifted left once with the SD end bit set
 * ((crc << 1) | 1), ready to be sent as the last command byte.
 */
uint8_t hk_crc7_sd(const uint8_t *data, size_t len);

/*
 * CRC-32/ISO-HDLC (zlib): reflected, poly 0xEDB88320, init/xorout 0xFFFFFFFF.
 * Used by the config service to validate NV records.
 */
uint32_t hk_crc32(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* HK_CRC_H */
