libgcrypt Log Crypto Provider (gcry)
====================================

**Crypto Provider Name:**    gcry

**Author:** Rainer Gerhards <rgerhards@adiscon.com>

**Supported Since:** since 7.3.10

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

-  **cry.algo** <Encryption Algorithm>
   The algorithm (cipher) to be used for encryption. The default algorithm is "AES128".
   Currently, the following Algorithms are supported:

   -  3DES
   -  CAST5
   -  BLOWFISH
   -  AES128
   -  AES192
   -  AES256
   -  TWOFISH
   -  TWOFISH128
   -  ARCFOUR
   -  DES
   -  SERPENT128
   -  SERPENT192
   -  SERPENT256
   -  RFC2268\_40
   -  SEED
   -  CAMELLIA128
   -  CAMELLIA192
   -  CAMELLIA256

   The actual availability of an algorithms depends on which ones are
   compiled into libgcrypt. Note that some versions of libgcrypt simply
   abort the process (rsyslogd in this case!) if a supported algorithm
   is select but not available due to libgcrypt build settings. There is
   nothing rsyslog can do against this. So in order to avoid production
   downtime, always check carefully when you change the algorithm.

-  **cry.mode** <Algorithm Mode>
   The encryption mode to be used. Default ist Cipher Block Chaining
   (CBC). Note that not all encryption modes can be used together with
   all algorithms.
   Currently, the following modes are supported:

   -  ECB
   -  CFB
   -  CBC
   -  STREAM
   -  OFB
   -  CTR
   -  AESWRAP


-  **cry.key** <encryption key>
   TESTING AID, NOT FOR PRODUCTION USE. This uses the KEY specified
   inside rsyslog.conf. This is the actual key, and as such this mode is
   highly insecure. However, it can be useful for initial testing steps.
   This option may be removed in the future.

-  **cry.keyfile** <filename>
   Reads the key from the specified file. The file must contain the
   key, only, no headers or other meta information. Keyfiles can be
   generated via the rscrytool utility.

-  **cry.keyprogram** <path to program>
   If given, the key is provided by a so-called "key program". This
   program is executed and must return the key (as well as some meta
   information) via stdout. The core idea of key programs is that using
   this interface the user can implement as complex (and secure) method
   to obtain keys as desired, all without the need to make modifications
   to rsyslog.

**Caveats/Known Bugs:**

-  currently none known

**Samples:**

This encrypts a log file. Default parameters are used, they key is
provided via a keyfile.

::

    action(type="omfile" file="/var/log/somelog" cry.provider="gcry"
           cry.keyfile="/secured/path/to/keyfile") 

Note that the keyfile can be generated via the rscrytool utility (see its
documentation for how to actually do that).
