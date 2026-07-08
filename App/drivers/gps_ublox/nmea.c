#include "drivers/gps_ublox/nmea.h"

#include <stdlib.h>
#include <string.h>

#define HK_KNOTS_TO_MPS 0.514444f

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

bool hk_nmea_checksum_ok(const char *s)
{
    if (s == NULL || s[0] != '$') {
        return false;
    }
    uint8_t cs = 0;
    size_t  i  = 1;
    for (; s[i] != '\0' && s[i] != '*'; ++i) {
        cs ^= (uint8_t)s[i];
    }
    if (s[i] != '*') {
        return false; /* no checksum present */
    }
    int h1 = hexval(s[i + 1]);
    int h2 = (h1 >= 0) ? hexval(s[i + 2]) : -1;
    if (h1 < 0 || h2 < 0) {
        return false;
    }
    return (uint8_t)((h1 << 4) | h2) == cs;
}

double hk_nmea_to_decimal(double ddmm, char hemi)
{
    int    deg     = (int)(ddmm / 100.0);
    double minutes = ddmm - (double)deg * 100.0;
    double dec     = (double)deg + minutes / 60.0;
    if (hemi == 'S' || hemi == 'W' || hemi == 's' || hemi == 'w') {
        dec = -dec;
    }
    return dec;
}

static void parse_time(const char *tok, hk_gps_fix_t *o)
{
    if (strlen(tok) >= 6) {
        char b[3] = {0};
        b[0] = tok[0]; b[1] = tok[1]; o->hour   = (uint8_t)atoi(b);
        b[0] = tok[2]; b[1] = tok[3]; o->minute = (uint8_t)atoi(b);
        o->second = (float)atof(tok + 4);
    }
}

static void parse_date(const char *tok, hk_gps_fix_t *o)
{
    if (strlen(tok) >= 6) {
        char b[3] = {0};
        b[0] = tok[0]; b[1] = tok[1]; o->day   = (uint8_t)atoi(b);
        b[0] = tok[2]; b[1] = tok[3]; o->month = (uint8_t)atoi(b);
        b[0] = tok[4]; b[1] = tok[5];
        int yy = atoi(b);
        o->year = (uint16_t)((yy < 80) ? (2000 + yy) : (1900 + yy));
    }
}

/* Split body (no '$', no '*XX') into tokens; empty fields become "". */
static int tokenize(char *body, char *tok[], int max)
{
    int   n     = 0;
    char *start = body;
    for (char *p = body; ; ++p) {
        if (*p == ',' || *p == '\0') {
            bool end = (*p == '\0');
            *p = '\0';
            if (n < max) {
                tok[n++] = start;
            }
            start = p + 1;
            if (end) {
                break;
            }
        }
    }
    return n;
}

hk_nmea_type_t hk_nmea_parse_sentence(const char *sentence, hk_gps_fix_t *out)
{
    if (sentence == NULL || out == NULL || sentence[0] != '$') {
        return HK_NMEA_NONE;
    }
    if (!hk_nmea_checksum_ok(sentence)) {
        return HK_NMEA_NONE;
    }

    /* Copy the body between '$' and '*' into a work buffer. */
    char   work[HK_NMEA_LINE_MAX];
    size_t j = 0;
    for (size_t i = 1; sentence[i] != '\0' && sentence[i] != '*' &&
                       j < sizeof(work) - 1; ++i) {
        work[j++] = sentence[i];
    }
    work[j] = '\0';

    char *tok[24];
    int   nt = tokenize(work, tok, 24);
    if (nt < 1) {
        return HK_NMEA_NONE;
    }

    const char *id  = tok[0];
    size_t      idl = strlen(id);
    const char *typ = (idl >= 3) ? (id + idl - 3) : id;

    if (strcmp(typ, "GGA") == 0) {
        if (nt > 1 && tok[1][0]) { parse_time(tok[1], out); }
        if (nt > 5 && tok[2][0] && tok[4][0]) {
            out->lat_deg = hk_nmea_to_decimal(atof(tok[2]), tok[3][0]);
            out->lon_deg = hk_nmea_to_decimal(atof(tok[4]), tok[5][0]);
        }
        if (nt > 6 && tok[6][0]) { out->fix_quality = (uint8_t)atoi(tok[6]); }
        if (nt > 7 && tok[7][0]) { out->satellites  = (uint8_t)atoi(tok[7]); }
        if (nt > 9 && tok[9][0]) { out->alt_m       = (float)atof(tok[9]); }
        out->valid = (out->fix_quality > 0);
        return HK_NMEA_GGA;
    }

    if (strcmp(typ, "RMC") == 0) {
        if (nt > 1 && tok[1][0]) { parse_time(tok[1], out); }
        bool active = (nt > 2 && tok[2][0] == 'A');
        if (nt > 6 && tok[3][0] && tok[5][0]) {
            out->lat_deg = hk_nmea_to_decimal(atof(tok[3]), tok[4][0]);
            out->lon_deg = hk_nmea_to_decimal(atof(tok[5]), tok[6][0]);
        }
        if (nt > 7 && tok[7][0]) { out->speed_mps  = (float)atof(tok[7]) * HK_KNOTS_TO_MPS; }
        if (nt > 8 && tok[8][0]) { out->course_deg = (float)atof(tok[8]); }
        if (nt > 9 && tok[9][0]) { parse_date(tok[9], out); }
        out->valid = active;
        return HK_NMEA_RMC;
    }

    return HK_NMEA_OTHER;
}

void hk_nmea_init(hk_nmea_t *n)
{
    n->len         = 0;
    n->in_sentence = false;
    n->line[0]     = '\0';
}

hk_nmea_type_t hk_nmea_feed(hk_nmea_t *n, char c, hk_gps_fix_t *out)
{
    if (c == '$') {
        n->in_sentence = true;
        n->len         = 0;
        n->line[n->len++] = c;
        return HK_NMEA_NONE;
    }
    if (!n->in_sentence) {
        return HK_NMEA_NONE;
    }
    if (c == '\r' || c == '\n') {
        n->line[n->len] = '\0';
        n->in_sentence  = false;
        if (n->len > 1) {
            return hk_nmea_parse_sentence(n->line, out);
        }
        return HK_NMEA_NONE;
    }
    if (n->len < HK_NMEA_LINE_MAX - 1) {
        n->line[n->len++] = c;
    } else {
        /* overflow: drop the sentence */
        n->in_sentence = false;
        n->len         = 0;
    }
    return HK_NMEA_NONE;
}
