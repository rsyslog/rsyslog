/* Verify per-testcase legacy configuration state restoration. The oracle
 * compares the tracked ourConf fields after A->A and A->B->A dispatches, so
 * an intermediate testcase cannot affect the final observation of A. */

#include "config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fuzzapi.h"

static int same_state(const rs_fuzz_legacy_state_t *left, const rs_fuzz_legacy_state_t *right) {
    return left->main_queue_size == right->main_queue_size &&
           left->debug_print_template_list == right->debug_print_template_list &&
           left->uid_drop_priv == right->uid_drop_priv && left->gid_drop_priv == right->gid_drop_priv &&
           left->main_queue_filename_present == right->main_queue_filename_present &&
           left->main_queue_filename_length == right->main_queue_filename_length &&
           left->main_queue_filename_hash == right->main_queue_filename_hash;
}

static int capture(rs_fuzz_legacy_state_t *state, const char *where) {
    if (rs_fuzz_get_legacy_state(state) != 0) {
        fprintf(stderr, "could not capture legacy state after %s\n", where);
        return -1;
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2 || (strcmp(argv[1], "aa") != 0 && strcmp(argv[1], "aba") != 0)) {
        fprintf(stderr, "usage: %s aa|aba\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (rs_fuzz_init() != RS_RET_OK) {
        fprintf(stderr, "state regression initialization failed\n");
        return EXIT_FAILURE;
    }

    /* The leading selector chooses the legacy command while "mainmsg" makes
     * the common dispatcher include the legacy-config target. */
    static const uint8_t input_a[] = {1, 'm', 'a', 'i', 'n', 'm', 's', 'g', '-', 'a'};
    static const uint8_t input_b[] = {0, 'm', 'a', 'i', 'n', 'm', 's', 'g', '-', 'b'};
    rs_fuzz_legacy_state_t initial;
    rs_fuzz_legacy_state_t after_first_a;
    rs_fuzz_legacy_state_t final;

    if (capture(&initial, "initialization") != 0) {
        return EXIT_FAILURE;
    }
    rs_fuzz_dispatch_input(input_a, sizeof(input_a));
    if (capture(&after_first_a, "first A") != 0 || !same_state(&initial, &after_first_a)) {
        fprintf(stderr, "legacy state was not restored after first A\n");
        return EXIT_FAILURE;
    }
    if (strcmp(argv[1], "aba") == 0) {
        rs_fuzz_dispatch_input(input_b, sizeof(input_b));
        if (capture(&final, "B") != 0 || !same_state(&initial, &final)) {
            fprintf(stderr, "legacy state was not restored after B\n");
            return EXIT_FAILURE;
        }
    }
    rs_fuzz_dispatch_input(input_a, sizeof(input_a));
    if (capture(&final, "final A") != 0 || !same_state(&after_first_a, &final)) {
        fprintf(stderr, "final A depends on the preceding testcase sequence\n");
        return EXIT_FAILURE;
    }

    printf("%d:%d:%d:%d:%d:%zu:%016llx\n", final.main_queue_size, final.debug_print_template_list, final.uid_drop_priv,
           final.gid_drop_priv, final.main_queue_filename_present, final.main_queue_filename_length,
           (unsigned long long) final.main_queue_filename_hash);
    return EXIT_SUCCESS;
}
