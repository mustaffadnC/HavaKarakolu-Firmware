#ifndef HK_MOCK_I2C_H
#define HK_MOCK_I2C_H

/* Scripted I2C bus mock for host unit tests.
 *
 * A test provides an ordered array of expected transactions. Every driver
 * call (write / read / write_read) consumes exactly one step: the mock
 * checks the address and written bytes against the script, copies the
 * scripted reply into the driver's buffer and returns the scripted status.
 * Deviations are counted in `mismatch`; assert hk_mock_i2c_done() at the
 * end of the test. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bus/i2c_bus_if.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t        addr;      /* expected 7-bit device address              */
    const uint8_t *expect_w;  /* expected written bytes; NULL = skip check  */
    size_t         wlen;      /* expected write length (0 for pure reads)   */
    const uint8_t *reply;     /* bytes handed back to the driver (or NULL)  */
    size_t         rlen;      /* reply length; must equal the requested len */
    hk_status_t    ret;       /* status returned to the driver              */
} hk_mock_i2c_step_t;

typedef struct {
    const hk_mock_i2c_step_t *steps;
    size_t                    n;
    size_t                    idx;       /* next step to consume  */
    int                       mismatch;  /* expectation failures  */
} hk_mock_i2c_t;

/* Wire `bus` to this mock and arm it with the script. */
void hk_mock_i2c_init(hk_mock_i2c_t *m, hk_i2c_bus_t *bus,
                      const hk_mock_i2c_step_t *steps, size_t n);

/* True when every step was consumed and nothing mismatched. */
bool hk_mock_i2c_done(const hk_mock_i2c_t *m);

#ifdef __cplusplus
}
#endif

#endif /* HK_MOCK_I2C_H */
