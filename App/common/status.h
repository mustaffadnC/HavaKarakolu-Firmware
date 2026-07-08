#ifndef HK_STATUS_H
#define HK_STATUS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Project-wide return codes. 0 == success, negative == error. */
typedef enum {
    HK_OK            =  0,
    HK_ERR           = -1,  /* generic failure */
    HK_ERR_TIMEOUT   = -2,
    HK_ERR_NACK      = -3,  /* I2C device did not ACK */
    HK_ERR_PARAM     = -4,  /* bad argument */
    HK_ERR_CRC       = -5,  /* checksum mismatch */
    HK_ERR_NOT_FOUND = -6,  /* device/whoami mismatch */
    HK_ERR_BUSY      = -7,
    HK_ERR_IO        = -8,  /* low-level transport error */
    HK_ERR_STATE     = -9   /* operation invalid in current state */
} hk_status_t;

static inline const char *hk_status_str(hk_status_t s)
{
    switch (s) {
    case HK_OK:            return "OK";
    case HK_ERR:           return "ERR";
    case HK_ERR_TIMEOUT:   return "TIMEOUT";
    case HK_ERR_NACK:      return "NACK";
    case HK_ERR_PARAM:     return "PARAM";
    case HK_ERR_CRC:       return "CRC";
    case HK_ERR_NOT_FOUND: return "NOT_FOUND";
    case HK_ERR_BUSY:      return "BUSY";
    case HK_ERR_IO:        return "IO";
    case HK_ERR_STATE:     return "STATE";
    default:               return "?";
    }
}

#ifdef __cplusplus
}
#endif

#endif /* HK_STATUS_H */
