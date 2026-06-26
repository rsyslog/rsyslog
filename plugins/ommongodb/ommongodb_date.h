#ifndef OMMONGODB_DATE_H
#define OMMONGODB_DATE_H

#include <stdint.h>

int ommongodbParseIsoDateMs(const char *date, int64_t *timestampMs);

#endif
