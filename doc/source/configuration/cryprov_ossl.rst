libossl Log Crypto Provider (ossl)
====================================

**Crypto Provider Name:**    ossl

**Author:** Attila Lakatos <alakatos@redhat.com>

**Supported Since:** since 8.2408.0

**Description**:

Provides encryption support to rsyslog.

**Configuration Parameters**:

Crypto providers are loaded by omfile, when the provider is selected in
its "cry.providerName" parameter. Parameters for the provider are given
in the omfile action instance line.

This provider creates an encryption information file with the same base
name but the extension ".encinfo" for each log file (both for fixed-name
files as well as dynafiles). Both files together form a set. So you need
to archive both in order to prove integrity.

The main differences between the ossl and gcry crypto providers are:
In ossl, algorithms are not hardcoded. There are concerns about potential
side-channel vulnerabilities with ossl, such as the Minerva Attack
and the Raccoon Attack, due to their bad handling of well-known
side-channel attacks. As a result of the Marvin incident, they have downgraded
their threat model.

Note for distro maintainers: the libossl crypto provider will be available
only if rsyslog is compiled with --enable-openssl option.

-  **cry.algo** <Encryption Algorithm>
   The algorithm and mode to be used for encryption. The default is "AES-128-CBC".
   The actual availability of an algorithm depends on which ones are
   compiled into openssl. The cipher implementation is retrieved using the
   EVP_CIPHER_fetch() function. See "ALGORITHM FETCHING" in crypto(7) for
   further information. Algorithms are not hardcoded, we provide everything
   that can be fetched using the aforementioned function.
   Note: Always check carefully when you change the algorithm if it's available.

-  **cry.key** <encryption key>
   TESTING AID, NOT FOR PRODUCTION USE. This uses the KEY specified
   inside rsyslog.conf. This is the actual key, and as such this mode is
   highly insecure. However, it can be useful for initial testing steps.
   This option may be removed in the future.

-  **cry.keyfile** <filename>
   Reads the key from the specified file. The file must contain the
   key, only, no headers or other meta information. Keyfiles can be
   generated via the rscrytool utility.

**Caveats/Known Bugs:**

-  currently none known

**Samples:**

This encrypts a log file. Default parameters are used, they key is
provided via a keyfile.

::

    action(type="omfile" file="/var/log/somelog" cry.provider="ossl"
           cry.keyfile="/secured/path/to/keyfile")

In addition to previous example, this changes the default algorithm:

::

    action(type="omfile" file="/var/log/somelog" cry.provider="ossl"
           cry.keyfile="/secured/path/to/keyfile" cry.algo="AES-256-CBC")

Note that the keyfile can be generated via the rscrytool utility (see its
documentation for how to actually do that).
