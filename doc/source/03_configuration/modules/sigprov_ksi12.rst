KSI Signature Provider (rsyslog-ksi-ls12)
============================================================

**Module Name: rsyslog-ksi-ls12**

**Available Since:** 8.27

**Author:** Guardtime & Adiscon

Description
###########

The ``rsyslog-ksi-ls12`` module enables record level log signing with Guardtime KSI Blockchain. KSI signatures provide long-term log integrity and prove the time of log records cryptographically using independent verification.

Main features of the ``rsyslog-ksi-ls12`` module are:

* Automated online signing of file output log.
* Efficient block-based signing with record-level verification.
* Log records removal detection.

For best results use the ``rsyslog-ksi-ls12`` module together with Guardtime ``logksi`` tool, which will become handy in:

* Signing recovery.
* Extension of KSI signatures inside the log signature file.
* Verification of the log using log signatures.
* Extraction of record-level signatures.
* Integration of log signature files (necessary when signing in async mode).

Getting Started
###############

To get started with log signing:

- Sign up to the Guardtime tryout service to be able to connect to KSI blockchain:
  `guardtime.com/technology/blockchain-developers <https://guardtime.com/technology/blockchain-developers>`_
- Install the ``libksi`` library (v3.20 or later)
  `(libksi install) <https://github.com/guardtime/libksi#installation>`_
- Install the ``rsyslog-ksi-ls12`` module (same version as rsyslog) from Adiscon repository.
- Install the accompanying ``logksi`` tool (recommended v1.5 or later)
  `(logksi install) <https://github.com/guardtime/logksi#installation>`_

The format of the output depends on signing mode enabled (synchronous (``sync``) or asynchronous (``async``)).

- In ``sync`` mode, log signature file is written directly into file ``<logfile>.logsig``. This mode is blocking as issuing KSI signatures one at a time will halt actual writing of log lines into log files. This mode suits for a system where signatures are issued rarely and delay caused by signing process is acceptable. Advantage compared to ``async`` mode is that the user has no need to integrate intermediate files to get actual log signature.

- In ``async`` mode, log signature intermediate files are written into directory ``<logfile>.logsig.parts``. This mode is not blocking enabling high availability and concurrent signing of several blocks at the same time. Log signature is divided into two files, where one contains info about log records and blocks, and the other contains KSI signatures issued asynchronously. To create ``<logfile>.logsig`` from ``<logfile>.logsig.parts``, use ``logksi integrate <logfile>``. Advantage compared to ``sync`` mode is much better operational stability and speed.

Currently the log signing is only supported by the file output module, thus the action type must be ``omfile``. To activate signing, add the following parameters to the action of interest in your rsyslog configuration file:

Mandatory parameters (no default value defined):

- **sig.provider** specifies the signature provider; in case of ``rsyslog-ksi-ls12`` package this is ``"ksi_ls12"``.
- **sig.block.levelLimit** defines the maximum level of the root of the local aggregation tree per one block. The maximum number of log lines in one block is calculated as ``2^(levelLimit - 1)``.
- **sig.aggregator.url** defines the endpoint of the KSI signing service in KSI Gateway. In ``async`` mode it is possible to specify up to 3 endpoints for high availability service, where user credentials are integrated into URL. Supported URI schemes are:

  - *ksi+http://*
  - *ksi+tcp://*

  Examples:

  - sig.aggregator.url="ksi+tcp://signingservice1.example.com"

    sig.aggregator.user="rsmith"

    sig.aggregator.key= "secret"

  - sig.aggregator.url="ksi+tcp://rsmith:secret@signingservice1.example.com|ksi+tcp://jsmith:terces@signingservice2.example.com"

- **sig.aggregator.user** specifies the login name for the KSI signing service. For high availability service, credentials are specified in URI.
- **sig.aggregator.key** specifies the key for the login name. For high availability service, credentials are specified in URI.

Optional parameters (if not defined, default value is used):

- **sig.syncmode** defines the signing mode: ``"sync"`` (default) or ``"async"``.
- **sig.hashFunction** defines the hash function to be used for hashing, default is ``"SHA2-256"``.
  Other SHA-2, as well as RIPEMED-160 functions are supported.
- **sig.block.timeLimit** defines the maximum duration of one block in seconds.
  Default value ``"0"`` indicates that no time limit is set.
- **sig.block.signTimeout** specifies a time window within the block signatures
  have to be issued, default is ``10``. For example, issuing 4 signatures in a
  second with sign timeout 10s, it is possible to handle 4 x 10 signatures
  request created at the same time. More than that, will close last blocks with
  signature failure as signature requests were not sent out within 10 seconds.
- **sig.aggregator.hmacAlg** defines the HMAC algorithm to be used in communication with the KSI Gateway.
  This must be agreed on with the KSI service provider, default is ``"SHA2-256"``.
- **sig.keepTreeHashes** turns on/off the storing of the hashes that were used as leaves
  for building the Merkle tree, default is ``"off"``.
- **sig.keepRecordHashes** turns on/off the storing of the hashes of the log records, default is ``"on"``.
- **sig.confInterval** defines interval of periodic request for aggregator configuration in seconds, default is ``3600``.
- **sig.randomSource** defines source of random as file, default is ``"/dev/urandom"``.
- **sig.debugFile** enables libksi log and redirects it into file specified. Note that logger level has to be specified (see ``sig.debugLevel``).
- **sig.debugLevel** specifies libksi log level. Note that log file has to be specified (see ``sig.debugFile``).

  - *0* None (default).
  - *1* Error.
  - *2* Warning.
  - *3* Notice.
  - *4* Info.
  - *5* Debug.

The log signature file, which stores the KSI signatures and information about the signed blocks, appears in the same directory as the log file itself.

Sample
######

To sign the logs in ``/var/log/secure`` with KSI:
::

  # The authpriv file has restricted access and is signed with KSI
  authpriv.*  action(type="omfile" file="/var/log/secure"
    sig.provider="ksi_ls12"
    sig.syncmode="sync"
    sig.hashFunction="SHA2-256"
    sig.block.levelLimit="8"
    sig.block.timeLimit="0"
    sig.aggregator.url=
      "http://tryout.guardtime.net:8080/gt-signingservice"
    sig.aggregator.user="rsmith"
    sig.aggregator.key="secret"
    sig.aggregator.hmacAlg="SHA2-256"
    sig.keepTreeHashes="off"
    sig.keepRecordHashes="on")


Note that all parameter values must be between quotation marks!

See Also
########

To better understand the log signing mechanism and the module's possibilities it is advised to consult with:

- `KSI Rsyslog Integration User Guide <https://docs.guardtime.net/ksi-rsyslog-guide/>`_
- `KSI Developer Guide <https://docs.guardtime.net/ksi-dev-guide/>`_

Access for both of these documents requires Guardtime tryout service credentials, available from `<https://guardtime.com/technology/blockchain-developers>`_
