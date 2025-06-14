mmrfc5424addhmac
================

**Module Name:    mmrfc5424addhmac**

**Author:**\ Rainer Gerhards <rgerhards@adiscon.com>

**Available since**: 7.5.6

**Description**:

This module adds a hmac to RFC5424 structured data if not already
present. This is a custom module and uses openssl as requested by the
sponsor. This works exclusively for RFC5424 formatted messages; all
others are ignored.

If both `mmpstrucdata <mmpstrucdata.html>`_ and mmrfc5424addhmac are to
be used, the recommended calling sequence is

#. mmrfc5424addhmac
#. mmpstrucdata

with that sequence, the generated hash will become available for
mmpstrucdata.

 

**Module Configuration Parameters**:

Note: parameter names are case-insensitive.

Currently none.

 

**Action Configuration Parameters**:

Note: parameter names are case-insensitive.

-  **key**
   The "key" (string) to be used to generate the hmac.
-  **hashfunction**
   An openssl hash function name for the function to be used. This is
   passed on to openssl, so see the openssl list of supported function
   names.
-  **sd\_id**
   The RFC5424 structured data ID to be used by this module. This is
   the SD-ID that will be added. Note that nothing is added if this
   SD-ID is already present.

**Verification method**

rsyslog does not contain any tools to verify a log file (this was not
part of the custom project). So you need to write your own verifier.

When writing the verifier, keep in mind that the log file contains
messages with the hash SD-ID included. For obvious reasons, this SD-ID
was not present when the hash was created. So before the actual
verification is done, this SD-ID must be removed, and the remaining
(original) message be verified. Also, it is important to note that the
output template must write the exact same message format that was
received. Otherwise, a verification failure will obviously occur - and
must so, because the message content actually was altered.

So in a more formal description, verification of a message m can be done
as follows:

#. let m' be m with the configured SD-ID removed (everything between
   []). Otherwise, m' must be an exact duplicate of m.
#. call openssl's HMAC function as follows:
   ``HMAC(hashfunction, key, len(key), m', len(m'), hash, &hashlen);``
   Where hashfunction and key are the configured values and hash is an
   output buffer for the hash.
#. let h be the extracted hash value obtained from m within the relevant
   SD-ID. Be sure to convert the hex string back to the actual byte
   values.
#. now compare hash and h under consideration of the sizes. If these
   values match the verification succeeds, otherwise the message was
   modified.

If you need help implementing a verifier function or want to sponsor
development of a verification tool, please simply email
`sales@adiscon.com <sales@adiscon.com>`_ for a quote.

**See Also**

-  `How to add an HMAC to RFC5424
   messages <http://www.rsyslog.com/how-to-add-a-hmac-to-rfc5424-structured-data-messages/>`_

**Caveats/Known Bugs:**

-  none

