/* Queue-private binary message codec for the segmented disk store. */
#ifndef INCLUDED_SEGDISK_CODEC_H
#define INCLUDED_SEGDISK_CODEC_H

#include <stddef.h>
#include <stdint.h>
#include "rsyslog.h"
#include "msg.h"
#include "segdisk_format.h"

rsRetVal segdiskCodecEncode(smsg_t *msg, unsigned char **buf, size_t *len);
rsRetVal segdiskCodecDecode(const unsigned char *buf, size_t len, smsg_t **msg);

#endif
