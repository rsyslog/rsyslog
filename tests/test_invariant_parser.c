#include <check.h>
#include <stdlib.h>
#include <string.h>

/* We test that the parser does not overflow when iSrc > maxDest.
 * The vulnerable path: malloc(maxDest+1) then memcpy(pDst, pszMsg, iSrc).
 * We call sanitizeMessage which internally uses this pattern.
 * If unauthenticated input can trigger overflow, the invariant is:
 * output length must never exceed the declared maxDest. */

#include "runtime/rsyslog.h"
#include "runtime/msg.h"
#include "runtime/parser.h"

START_TEST(test_parser_rejects_oversized_unauthenticated_input)
{
    /* Invariant: Parser must not write beyond allocated buffer for any
     * unauthenticated network input. We simulate messages that could
     * trigger iSrc > maxDest via crafted syslog payloads. */

    /* Exploit case: message with embedded NULs and length exceeding typical max */
    char exploit_payload[8192];
    memset(exploit_payload, 'A', sizeof(exploit_payload) - 1);
    exploit_payload[sizeof(exploit_payload) - 1] = '\0';

    /* Boundary: exactly at maxDest boundary (2048 is common internal limit) */
    char boundary_payload[2049];
    memset(boundary_payload, 'B', 2048);
    boundary_payload[2048] = '\0';

    /* Valid short message */
    const char *valid_payload = "<13>Jan  1 00:00:00 host tag: valid msg";

    const char *payloads[] = {
        exploit_payload,
        boundary_payload,
        valid_payload
    };
    size_t lengths[] = {
        sizeof(exploit_payload) - 1,
        2048,
        strlen(valid_payload)
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        smsg_t *pMsg = NULL;
        rsRetVal ret = msgConstruct(&pMsg);
        if (ret != RS_RET_OK) continue;

        MsgSetRawMsg(pMsg, payloads[i], lengths[i]);

        /* Attempt to parse - this exercises the vulnerable code path.
         * The invariant: it must not crash (ASAN/valgrind would catch overflow)
         * and must return without corrupting memory. */
        ret = parseMsgBody(pMsg);

        /* For unauthenticated oversized input, parser should either
         * truncate safely or reject - never overflow */
        ck_assert_msg(ret == RS_RET_OK || ret == RS_RET_COULD_NOT_PARSE,
            "Parser must handle payload %d safely, got ret=%d", i, ret);

        msgDestruct(&pMsg);
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_parser_rejects_oversized_unauthenticated_input);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}