#include "drivers/gps_ublox/gps_ublox.h"

#include <string.h>

hk_status_t hk_gps_init(hk_gps_t *dev, const hk_uart_t *uart)
{
    if (dev == NULL || uart == NULL) {
        return HK_ERR_PARAM;
    }
    dev->uart      = uart;
    dev->sentences = 0;
    memset(&dev->fix, 0, sizeof(dev->fix));
    hk_nmea_init(&dev->nmea);
    return HK_OK;
}

int hk_gps_poll(hk_gps_t *dev)
{
    int     parsed = 0;
    uint8_t chunk[64];
    size_t  n;
    while ((n = hk_uart_read(dev->uart, chunk, sizeof(chunk))) > 0) {
        for (size_t i = 0; i < n; ++i) {
            hk_nmea_type_t t = hk_nmea_feed(&dev->nmea, (char)chunk[i], &dev->fix);
            if (t == HK_NMEA_GGA || t == HK_NMEA_RMC) {
                dev->sentences++;
                parsed++;
            }
        }
        if (n < sizeof(chunk)) {
            break; /* drained */
        }
    }
    return parsed;
}

void hk_gps_get_fix(const hk_gps_t *dev, hk_gps_fix_t *out)
{
    *out = dev->fix;
}

hk_status_t hk_gps_send_ubx(hk_gps_t *dev, uint8_t msg_class, uint8_t msg_id,
                            const uint8_t *payload, uint16_t len)
{
    uint8_t hdr[6];
    hdr[0] = 0xB5;
    hdr[1] = 0x62;
    hdr[2] = msg_class;
    hdr[3] = msg_id;
    hdr[4] = (uint8_t)(len & 0xFF);
    hdr[5] = (uint8_t)(len >> 8);

    /* Fletcher checksum over class, id, len, payload. */
    uint8_t ck_a = 0, ck_b = 0;
    for (int i = 2; i < 6; ++i) { ck_a = (uint8_t)(ck_a + hdr[i]); ck_b = (uint8_t)(ck_b + ck_a); }
    for (uint16_t i = 0; i < len; ++i) { ck_a = (uint8_t)(ck_a + payload[i]); ck_b = (uint8_t)(ck_b + ck_a); }

    if (hk_uart_write(dev->uart, hdr, sizeof(hdr)) < 0) {
        return HK_ERR_IO;
    }
    if (len > 0 && payload != NULL) {
        if (hk_uart_write(dev->uart, payload, len) < 0) {
            return HK_ERR_IO;
        }
    }
    uint8_t ck[2] = { ck_a, ck_b };
    if (hk_uart_write(dev->uart, ck, 2) < 0) {
        return HK_ERR_IO;
    }
    return HK_OK;
}
