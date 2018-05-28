************************************************************
Keyless Signature Infrastructure Provider (rsyslog-ksi-ls12)
************************************************************

===========================  ===========================================================================
**Module Name:**             **rsyslog-ksi-ls12**
**Author:**                  **Guardtime & Adiscon**
**Available Since:**         **8.27**
===========================  ===========================================================================


Purpose
=======

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
===============

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


Configuration Parameters
========================

Currently the log signing is only supported by the file output module, thus the action type must be ``omfile``. To activate signing, add the following parameters to the action of interest in your rsyslog configuration file:

.. note::

   Parameter names are case-insensitive.


Action Parameters
-----------------

sig.provider
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "no", "none"

Specifies the signature provider; in case of ``rsyslog-ksi-ls12`` package
this is ``"ksi_ls12"``.


sig.block.levelLimit
^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "size", "none", "yes", "none"

Defines the maximum level of the root of the local aggregation tree per
one block.


sig.aggregator.url
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "yes", "none"

Defines the endpoint of the KSI signing service in KSI Gateway. Supported
URL schemes are:

  - *http://*
  - *ksi+http://*
  - *ksi+tcp://*


sig.aggregator.user
^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "yes", "none"

Specifies the login name for the KSI signing service.


sig.aggregator.key
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "none", "yes", "none"

Specifies the key for the login name.


sig.syncmode
^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "string", "sync", "no", "none"

Defines the signing mode: ``"sync"`` or ``"async"``.


sig.hashFunction
^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "word", "SH2-256", "no", "none"

Defines the hash function to be used for hashing.
The following hash algorithms are currently supported:

   -  SHA1
   -  SHA2-256
   -  RIPEMD-160
   -  SHA2-224
   -  SHA2-384
   -  SHA2-512
   -  RIPEMD-256
   -  SHA3-244
   -  SHA3-256
   -  SHA3-384
   -  SHA3-512
   -  SM3


sig.block.timeLimit
^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "int", "0", "no", "none"

Defines the maximum duration of one block in seconds. Default value
indicates that no time limit is set.


sig.keepTreeHashes
^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "off", "no", "none"

Turns on/off the storing of the hashes that were used as leaves
for building the Merkle tree.


sig.keepRecordHashes
^^^^^^^^^^^^^^^^^^^^

.. csv-table::
   :header: "type", "default", "mandatory", "|FmtObsoleteName| directive"
   :widths: auto
   :class: parameter-table

   "binary", "on", "no", "none"

Turns on/off the storing of the hashes of the log records.

The log signature file, which stores the KSI signatures and information about the signed blocks, appears in the same directory as the log file itself; it is named ``<logfile>.logsig``.


See Also
========

To better understand the log signing mechanism and the module's possibilities it is advised to consult with:

- `KSI Rsyslog Integration User Guide <https://docs.guardtime.net/ksi-rsyslog-guide/>`_
- `KSI Developer Guide <https://docs.guardtime.net/ksi-dev-guide/>`_

Access for both of these documents requires Guardtime tryout service credentials, available from `<https://guardtime.com/technology/blockchain-developers>`_


Examples
========

Signing logs with KSI
---------------------

To sign the logs in ``/var/log/secure`` with KSI:

.. code-block:: none

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

