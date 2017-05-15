Keyless Signature Infrastructure Provider (rsyslog-ksi-ls12)
============================================================

**Module Name: rsyslog-ksi-ls12**

**Available Since:** 8.27

**Author:** Guardtime & Adiscon

Description
###########

The ``rsyslog-ksi-ls12`` module enables record level log signing with Guardtime KSI blockchain. KSI keyless signatures provide long-term log integrity and prove the time of log records cryptographically using independent verification.

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
- Install the ``libksi`` library (v3.13 or later) from Guardtime repository either for RHEL/CentOS 6
  `<http://download.guardtime.com/ksi/configuration/guardtime.el6.repo>`_
  or RHEL/CentOS 7 `<http://download.guardtime.com/ksi/configuration/guardtime.el7.repo>`_
- Install the ``rsyslog-ksi-ls12`` module (same version as rsyslog) from Adiscon repository.
- Install the accompanying ``logksi`` tool (v1.0 or later) from Guardtime repository either for RHEL/CentOS 6
  `<http://download.guardtime.com/ksi/configuration/guardtime.el6.repo>`_
  or RHEL/CentOS 7 `<http://download.guardtime.com/ksi/configuration/guardtime.el7.repo>`_

Currently the log signing is only supported by the file output module, thus the action type must be ``omfile``. To activate signing, add the following parameters to the action of interest in your rsyslog configuration file:

Mandatory parameters (no default value defined):

- **sig.provider** specifies the signature provider; in case of ``rsyslog-ksi-ls12`` package this is ``"ksi_ls12"``.
- **sig.block.levelLimit** defines the maximum level of the root of the local aggregation tree per one block.
- **sig.aggregator.url** defines the endpoint of the KSI signing service in KSI Gateway. Supported URI schemes are:

  - *http://*
  - *ksi+http://*
  - *ksi+tcp://*

- **sig.aggregator.user** specifies the login name for the KSI signing service.
- **sig.aggregator.key** specifies the key for the login name.

Optional parameters (if not defined, default value is used):

- **sig.syncmode** defines the signing mode: ``"sync"`` (default) or ``"async"``.
- **sig.hashFunction** defines the hash function to be used for hashing, default is ``"SHA2-256"``.
  Other SHA-2, as well as RIPEMED-160 functions are supported.
- **sig.block.timeLimit** defines the maximum duration of one block in seconds.
  Default value ``"0"`` indicates that no time limit is set.
- **sig.keepTreeHashes** turns on/off the storing of the hashes that were used as leaves
  for building the Merkle tree, default is ``"off"``.
- **sig.keepRecordHashes** turns on/off the storing of the hashes of the log records, default is ``"on"``.

The log signature file, which stores the KSI signatures and information about the signed blocks, appears in the same directory as the log file itself; it is named ``<logfile>.logsig``.

Sample
######

To sign the logs in ``/var/log/secure`` with KSI:
::

  # The authpriv file has restricted access and is signed with KSI
  authpriv.*	action(type="omfile" file="/var/log/secure"
    sig.provider="ksi_ls12"
    sig.syncmode="sync"
    sig.hashFunction="SHA2-256"
    sig.block.levelLimit="8"
    sig.block.timeLimit="0"
    sig.aggregator.url=
      "http://tryout.guardtime.net:8080/gt-signingservice"
    sig.aggregator.user="rsmith"
    sig.aggregator.key="secret"
    sig.keepTreeHashes="off"
    sig.keepRecordHashes="on")


Note that all parameter values must be between quotation marks!

See Also
########

To better understand the log signing mechanism and the module's possibilities it is advised to consult with:

- `KSI Rsyslog Integration User Guide <https://docs.guardtime.net/ksi-rsyslog-guide/>`_
- `KSI Developer Guide <https://docs.guardtime.net/ksi-dev-guide/>`_

Access for both of these documents requires Guardtime tryout service credentials, available from `<https://guardtime.com/technology/blockchain-developers>`_
