#include "config.h"

#include <stddef.h>
#include <stdint.h>

#include "fuzzapi.h"
#include "fuzzdriver.h"

int LLVMFuzzerInitialize(int *argc, char ***argv) {
    (void)argc;
    (void)argv;
    return rs_fuzz_driver_init();
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    rs_fuzz_dispatch_input(data, size);
    return 0;
}
