#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/* only care about first two digits */
void parseVersionNumberFromStr(char* pszIn, long* piMajor, long* piMinor, long* piSubMinor) {
    char* p = pszIn;
    while (*p) {  // While there are more characters to process...
        if (isdigit(*p) || ((*p == '-' || *p == '+') && isdigit(*(p + 1)))) {
            if (*piMajor == 0) {
                *piMajor = strtol(p, &p, 10);  // Read number
            } else if (*piMinor == 0) {
                *piMinor = strtol(p, &p, 10);  // Read number
            } else {
                *piSubMinor = strtol(p, &p, 10);  // Read number
                return;
            }
        } else {
            p++;
        }
    }
}

int main(int argc __attribute__((unused)), char* argv[] __attribute__((unused))) {
    long iRelpVerMajor = 0;
    long iRelpVerMinor = 0;
    long iRelpVerSubMinor = 0;
    long iRelpCmpMajor = 0;
    long iRelpCmpMinor = 0;
    long iRelpCmpSubMinor = 0;

#if defined(RELP_VERSION)
    parseVersionNumberFromStr(RELP_VERSION, &iRelpVerMajor, &iRelpVerMinor, &iRelpVerSubMinor);
    if (argc > 1) {
        parseVersionNumberFromStr(argv[1], &iRelpCmpMajor, &iRelpCmpMinor, &iRelpCmpSubMinor);

        // Compare Version numbers
        if (iRelpVerMajor > iRelpCmpMajor || (iRelpVerMajor == iRelpCmpMajor && iRelpVerMinor > iRelpCmpMinor) ||
            (iRelpVerMajor == iRelpCmpMajor && iRelpVerMinor == iRelpCmpMinor &&
             iRelpVerSubMinor >= iRelpCmpSubMinor)) {
            printf("RELP Version %ld.%ld.%ld OK (Requested Version %ld.%ld.%ld)\n", iRelpVerMajor, iRelpVerMinor,
                   iRelpVerSubMinor, iRelpCmpMajor, iRelpCmpMinor, iRelpCmpSubMinor);
            return 0;
        } else {
            printf("RELP Version %ld.%ld.%ld NOT OK (Requested Version %ld.%ld.%ld)\n", iRelpVerMajor, iRelpVerMinor,
                   iRelpVerSubMinor, iRelpCmpMajor, iRelpCmpMinor, iRelpCmpSubMinor);
            return 1;
        }

    } else {
        printf("RELP Version %s\n", RELP_VERSION);
    }
    return 0;
#else
    printf("RELP Version unknown\n");
    return 1;
#endif
}
