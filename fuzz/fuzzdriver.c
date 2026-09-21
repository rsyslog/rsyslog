#include "config.h"

#include <stdio.h>

#include "fuzzapi.h"
#include "fuzzdriver.h"

int rs_fuzz_driver_init(void) {
    const rsRetVal ret = rs_fuzz_init();
    if (ret != RS_RET_OK) {
        fprintf(stderr, "rsyslog fuzz initialization failed: %d\n", ret);
        return -1;
    }
    return 0;
}
