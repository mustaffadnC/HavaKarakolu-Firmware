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

#ifdef __cplusplus
}
#endif

#endif /* HK_CRC_H */
