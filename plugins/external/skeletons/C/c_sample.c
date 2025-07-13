/* A simple sample for an external C program acting
 * as a rsyslog output plugin.
 * Copyright (C) 2014 by Adiscon GmbH.
 */
#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char *argv[])
{
    FILE *fpout;
    char *mode;
    char buf[64*1024];

    if(argc != 3 && argc != 2) {
        fprintf(stderr, "Usage: c_sample filename [fopen-mode]\n");
        exit(1);
    }

    if(argc != 3)
        mode = "a+";

    fpout = fopen(argv[1], mode);
    while(1) {
        if(fgets(buf, sizeof(buf), stdin) == NULL)
            break; /* end of file means terminate */
        fputs(buf, fpout);
    }
}
