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


Configuration Parameters
========================

.. note::
   This module has no module-level parameters. All parameters listed below are for actions.
   Parameter names are case-insensitive; camelCase is recommended for readability.

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Parameter
     - Summary
   * - :ref:`param-mmrfc5424addhmac-key`
     - .. include:: ../../reference/parameters/mmrfc5424addhmac-key.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmrfc5424addhmac-hashfunction`
     - .. include:: ../../reference/parameters/mmrfc5424addhmac-hashfunction.rst
        :start-after: .. summary-start
        :end-before: .. summary-end
   * - :ref:`param-mmrfc5424addhmac-sd-id`
     - .. include:: ../../reference/parameters/mmrfc5424addhmac-sd-id.rst
        :start-after: .. summary-start
        :end-before: .. summary-end

.. toctree::
   :hidden:

   ../../reference/parameters/mmrfc5424addhmac-key
   ../../reference/parameters/mmrfc5424addhmac-hashfunction
   ../../reference/parameters/mmrfc5424addhmac-sd-id

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

