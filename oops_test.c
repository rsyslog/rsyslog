/*
 * This is a small test program for generating a kernel protection fault
 * using the oops loadable module.
 *
 * Fri Apr 26 12:52:43 CDT 1996:  Dr. Wettstein
 *	Initial version.
 */


/* Includes. */
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>


/* Function prototypes. */
extern int main(int, char **);


extern int main(argc, argv)

     int argc;

     char *argv[];

{
	auto int fd;

	if ( argc != 2 )
	{
		fprintf(stderr, "No oops device specified.\n");
		return(1);
	}

	if ( (fd = open(argv[1], O_RDONLY)) < 0 )
	{
		fprintf(stderr, "Cannot open device: %s.\n", argv[1]);
		return(1);
	}

	if ( ioctl(fd, 1, 0) < 0 )
	{
		fprintf(stderr, "Failed on oops.\n");
		return(1);
	}

	printf("OOoops\n");

	close(fd);
	return(0);
}
