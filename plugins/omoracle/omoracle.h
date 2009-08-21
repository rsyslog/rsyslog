/** Definitions for the Oracle output module.

    This module needs OCI to be installed (on Red Hat-like systems
    this is usually the oracle-instantclient-devel RPM).

    This file is part of rsyslog.

    This file is licensed under the terms of the GPL version 3 or, at
    your choice, any later version. Exceptionally (perhaps), you are
    allowed to link to the Oracle Call Interface in your derived work

    Author: Luis Fernando Muñoz Mejías <Luis.Fernando.Munoz.Mejias@cern.ch>
*/
#ifndef __OMORACLEH__
#define __OMORACLEH__

/** Macros to make error handling easier. */

/** Checks for errors on the OCI handling. */
#define CHECKERR(handle,status) CHKiRet(oci_errors((handle), \
						   OCI_HTYPE_ERROR, (status)))

/** Checks for errors when handling the environment of OCI. */
#define CHECKENV(handle,status) CHKiRet(oci_errors((handle), \
						   OCI_HTYPE_ENV, (status)))

enum { MAX_BUFSIZE = 2048 };

#define BIND_MARK ':'

#endif
