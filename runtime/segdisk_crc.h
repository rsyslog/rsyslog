#ifndef INCLUDED_SEGDISK_CRC_H
#define INCLUDED_SEGDISK_CRC_H

#include <stddef.h>
#include <stdint.h>

uint32_t segdiskCrc32c(const void *buf, size_t len);

#endif
