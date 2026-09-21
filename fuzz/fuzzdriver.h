#ifndef RSYSLOG_FUZZ_DRIVER_H
#define RSYSLOG_FUZZ_DRIVER_H

#include <stddef.h>
#include <stdint.h>

int rs_fuzz_driver_init(void);
int rs_fuzz_file_main(int argc, char **argv);
int LLVMFuzzerInitialize(int *argc, char ***argv);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

#endif /* RSYSLOG_FUZZ_DRIVER_H */
