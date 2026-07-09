/* Queue-private binary message codec for the segmented disk store. */
#ifndef INCLUDED_SEGDISK_CODEC_H
#define INCLUDED_SEGDISK_CODEC_H

#include <stddef.h>
#include <stdint.h>
#include "rsyslog.h"
#include "msg.h"

#define SEGDISK_CODEC_VERSION 1

rsRetVal segdiskCodecEncode(smsg_t *msg, unsigned char **buf, size_t *len);
rsRetVal segdiskCodecDecode(const unsigned char *buf, size_t len, smsg_t **msg);
uint32_t segdiskCrc32c(const void *buf, size_t len);

#endif
