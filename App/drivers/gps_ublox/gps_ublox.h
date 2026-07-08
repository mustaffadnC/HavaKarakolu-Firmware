#ifndef HK_GPS_UBLOX_H
#define HK_GPS_UBLOX_H

#include <stdint.h>

#include "bus/uart_if.h"
#include "common/status.h"
#include "drivers/gps_ublox/nmea.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * u-blox MAX-M10S driver: consumes the UART byte stream, parses NMEA
 * (RMC/GGA) into a latest-fix snapshot, and can emit UBX config frames.
 */
typedef struct {
    const hk_uart_t *uart;
    hk_nmea_t        nmea;
    hk_gps_fix_t     fix;
    uint32_t         sentences;   /* total parsed (diagnostics) */
} hk_gps_t;

hk_status_t hk_gps_init(hk_gps_t *dev, const hk_uart_t *uart);

/* Drain available RX bytes through the parser. Returns sentences parsed now. */
int hk_gps_poll(hk_gps_t *dev);

/* Copy the latest fix snapshot. */
void hk_gps_get_fix(const hk_gps_t *dev, hk_gps_fix_t *out);

/* Build and transmit a UBX frame (adds sync bytes + Fletcher checksum). */
hk_status_t hk_gps_send_ubx(hk_gps_t *dev, uint8_t msg_class, uint8_t msg_id,
                            const uint8_t *payload, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* HK_GPS_UBLOX_H */
