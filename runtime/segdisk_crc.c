#include "config.h"
#include "segdisk_crc.h"

uint32_t segdiskCrc32c(const void *vbuf, size_t len) {
    const unsigned char *buf = vbuf;
    uint32_t crc = ~0u;
    for (size_t i = 0; i < len; ++i) {
        crc ^= buf[i];
        for (unsigned int bit = 0; bit < 8; ++bit) crc = (crc & 1u) ? (crc >> 1) ^ 0x82f63b78u : crc >> 1;
    }
    return ~crc;
}
