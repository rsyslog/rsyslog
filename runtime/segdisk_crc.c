#include "config.h"
#include "segdisk_crc.h"

uint32_t segdiskCrc32c(const void *vbuf, size_t len) {
    /* A compact nibble table avoids the eight polynomial branches per byte
     * of the original bitwise implementation while remaining portable. */
    static const uint32_t table[16] = {
        0x00000000u, 0x105ec76fu, 0x20bd8edeu, 0x30e349b1u, 0x417b1dbcu, 0x5125dad3u, 0x61c69362u, 0x7198540du,
        0x82f63b78u, 0x92a8fc17u, 0xa24bb5a6u, 0xb21572c9u, 0xc38d26c4u, 0xd3d3e1abu, 0xe330a81au, 0xf36e6f75u,
    };
    const unsigned char *buf = vbuf;
    uint32_t crc = ~0u;
    for (size_t i = 0; i < len; ++i) {
        crc ^= buf[i];
        crc = table[crc & 0x0fu] ^ (crc >> 4);
        crc = table[crc & 0x0fu] ^ (crc >> 4);
    }
    return ~crc;
}
