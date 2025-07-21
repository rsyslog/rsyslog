GuardTime Log Signature Provider (gt)
=====================================

**Signature Provider Name: gt**

**Author:** Rainer Gerhards <rgerhards@adiscon.com>

**Supported:** from 7.3.9 to 8.26.0

**Description**:

Provides the ability to sign syslog messages via the GuardTime signature
services.

**Configuration Parameters**:

Note: parameter names are case-insensitive.

Signature providers are loaded by omfile, when the provider is selected
in its "sig.providerName" parameter. Parameters for the provider are
given in the omfile action instance line.

This provider creates a signature file with the same base name but the
extension ".gtsig" for each log file (both for fixed-name files as well
as dynafiles). Both files together form a set. So you need to archive
both in order to prove integrity.

-  **sig.hashFunction** <Hash Algorithm>
   The following hash algorithms are currently supported:

   -  SHA1
   -  RIPEMD-160
   -  SHA2-224
   -  SHA2-256
   -  SHA2-384
   -  SHA2-512

-  **sig.timestampService** <timestamper URL>
   This provides the URL of the timestamper service. If not selected, a
   default server is selected. This may not necessarily be a good one
   for your region.

   *Note:* If you need to supply user credentials, you can add them to
   the timestamper URL. If, for example, you have a user "user" with
   password "pass", you can do so as follows:

       http://user:pass@timestamper.example.net

-  **sig.block.sizeLimit** <nbr-records>
   The maximum number of records inside a single signature block. By
   default, there is no size limit, so the signature is only written on
   file closure. Note that a signature request typically takes between
   one and two seconds. So signing to frequently is probably not a good
   idea.

-  **sig.keepRecordHashes** <on/**off**>
   Controls if record hashes are written to the .gtsig file. This
   enhances the ability to spot the location of a signature breach, but
   costs considerable disk space (65 bytes for each log record for
   SHA2-512 hashes, for example).

-  **sig.keepTreeHashes** <on/**off**>
   Controls if tree (intermediate) hashes are written to the .gtsig
   file. This enhances the ability to spot the location of a signature
   breach, but costs considerable disk space (a bit more than the amount
   sig.keepRecordHashes requires). Note that both Tree and Record hashes
   can be kept inside the signature file.

**See Also**

-  `How to sign log messages through signature provider
   Guardtime <http://www.rsyslog.com/how-to-sign-log-messages-through-signature-provider-guardtime/>`_

**Caveats/Known Bugs:**

-  currently none known

**Samples:**

This writes a log file with it's associated signature file. Default
parameters are used.

::

    action(type="omfile" file="/var/log/somelog" sig.provider="gt")

In the next sample, we use the more secure SHA2-512 hash function, sign
every 10,000 records and Tree and Record hashes are kept.

::

    action(type="omfile" file="/var/log/somelog" sig.provider="gt"
    sig.hashfunction="SHA2-512" sig.block.sizelimit="10000"
    sig.keepTreeHashes="on" sig.keepRecordHashes="on")
