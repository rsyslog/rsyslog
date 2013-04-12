=========
rscryutil
=========

--------------------------
Manage Encrypted Log Files
--------------------------

:Author: Rainer Gerhards <rgerhards@adiscon.com>
:Date: 2013-04-08
:Manual section: 1

SYNOPSIS
========

::

   rscryutil [OPTIONS] [FILE] ...


DESCRIPTION
===========

This tool performs various operations on encrypted log files.
Most importantly, it provides the ability to decrypt them.


OPTIONS
=======

-d, --decrypt
  Select decryption mode. This is the default mode.

-v, --verbose
  Select verbose mode.

-k, --key <KEY>
  TESTING AID, NOT FOR PRODUCTION USE. This uses the KEY specified
  on the command line. This is the actual key, and as such this mode
  is highly insecure. However, it can be useful for intial testing
  steps. This option may be removed in the future.

-a, --algo <algo>
  Sets the encryption algorightm (cipher) to be used. See below
  for supported algorithms. The default is "AES128".

-m, --mode <mode>
  Sets the ciphermode to be used. See below for supported modes.
  The default is "CBC".

OPERATION MODES
===============

The operation mode specifies what exactly the tool does with the provided
files. The default operation mode is "dump", but this may change in the future.
Thus, it is recommended to always set the operations mode explicitely. If 
multiple operations mode are set on the command line, results are 
unpredictable.

decrypt
-------

The provided log files are decrypted.

EXIT CODES
==========

The command returns an exit code of 0 if everything went fine, and some 
other code in case of failures.


SUPPORTED ALGORITHMS
====================

We basically support what libgcrypt supports. This is:
	3DES
	CAST5
	BLOWFISH
	AES128
	AES192
	AES256
	TWOFISH
	TWOFISH128
	ARCFOUR
	DES
	SERPENT128
	SERPENT192
	SERPENT256
	RFC2268_40
	SEED
	CAMELLIA128
	CAMELLIA192
	CAMELLIA256


SUPPORTED CIPHER MODES
======================

We basically support what libgcrypt supports. This is:
  	ECB
	CFB
	CBC
	STREAM
	OFB
	CTR
	AESWRAP

EXAMPLES
========

**rscryutil logfile**

Decrypts "logfile" and sends data to stdout.

SEE ALSO
========
**rsyslogd(8)**

COPYRIGHT
=========

This page is part of the *rsyslog* project, and is available under
LGPLv2.
