/* libgcry_common.c
 * This file hosts functions both being used by the rsyslog runtime as
 * well as tools who do not use the runtime (so we can maintain the
 * code at a single place).
 *
 * Copyright 2013 Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *       http://www.apache.org/licenses/LICENSE-2.0
 *       -or-
 *       see COPYING.ASL20 in the source distribution
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <gcrypt.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "rsyslog.h" /* we need data typedefs */
#include "libgcry.h"


/* read a key from a key file
 * @param[out] key - key buffer, must be freed by caller
 * @param[out] keylen - length of buffer
 * @returns 0 if OK, something else otherwise (we do not use
 *            iRet as this is also called from non-rsyslog w/o runtime)
 * The key length is limited to 64KiB to prevent DoS.
 * Note well: key is a blob, not a C string (NUL may be present!)
 */
int
gcryGetKeyFromFile(char *fn, char **key, unsigned *keylen)
{
	struct stat sb;
	int fd;
	int r;

	if(stat(fn, &sb) == -1) {
		r = 1; goto done;
	}
	if((sb.st_mode & S_IFMT) != S_IFREG) {
		r = 2; goto done;
	}
	if(sb.st_size > 64*1024) {
		r = 3; goto done;
	}
	if((*key = malloc(sb.st_size)) == NULL) {
		r = -1; goto done;
	}
	if((fd = open(fn, O_RDONLY)) < 0) {
		r = 4; goto done;
	}
	if(read(fd, *key, sb.st_size) != sb.st_size) {
		r = 5; goto done;
	}
	*keylen = sb.st_size;
	close(fd);
	r = 0;
done:	return r;
}
