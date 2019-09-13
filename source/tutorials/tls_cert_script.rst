Creating certificates with a script
===================================

*Written by* `Florian Riedl  <https://www.adiscon.com>`_
*(2019-09-12)*


Overview
--------

This small article describes is a quick addon to the TLS guides. It describes 
in short words, how you can create some quick and dirty certificates for 
testing. 

**Disclaimer**: When creating certificates with the attached scripts and more or 
less default configurations, you cannot create secure certificates. You need to 
use more detailed configuration files to create secure certificates.


Description
-----------

We created a few simple scripts and added configuration files from the sample 
configuration in the certtool man page. You can download them here: 
:download:`Download Scripts <cert-script.tar.gz>`.

The tarball contains 6 files, 3 scripts and 3 configurations. To execute, you 
must make the scripts executable and have certtool installed via libgnutls.

- Script 1 creates the CA key and certificate as outlined in `Setting up the CA 
  <tls_cert_ca.html>`_

- Script 2 creates the `machine key and certificate <tls_cert_machine.html>`_ for 
  a client.

- Script 3 creates the machine key and certificate for a server.

These scripts can easily be combined into one. But, we decided to go for 
separate scripts so each step can be repeated separately if needed.

After the scripts are executed, you should have 2 new files per script. 
Distribute the files to the machines as described before.


Example
-------

Apart from executing the scripts, no extra input is required. All input from 
manual certificate creating can be done automatically via the configuration 
template in the cfg files.

Sample output for the CA certificate generation.
::

    test@ubuntu:~/Documents$ ./1-generate-ca.sh 
    ** Note: Please use the --sec-param instead of --bits
    Generating a 2048 bit RSA private key...
    Generating a self signed certificate...
    X.509 Certificate Information:
	Version: 3
	Serial Number (hex): 5d7a6351
	Validity:
		Not Before: Thu Sep 12 15:25:05 UTC 2019
		Not After: Sun Sep 09 15:25:05 UTC 2029
	Subject: C=US,O=Example,OU=Example,CN=CA-Cert
	Subject Public Key Algorithm: RSA
	Certificate Security Level: Low
		Modulus (bits 2048):
			00:95:28:40:b6:4d:60:7c:cf:72:1d:17:36:b5:f1:11
			0d:42:05:e9:38:c7:6e:95:d9:42:02:c5:4b:f2:9d:e2
			c8:31:ac:18:ae:55:f7:e0:4c:dd:6d:72:32:01:fa:1d
			da:a1:3d:ad:c9:13:0a:68:3e:bc:40:6a:1e:f2:f7:65
			f0:e9:64:fa:84:8b:96:15:b5:10:f3:99:29:14:ee:fc
			88:8d:41:29:8e:c7:9b:23:df:8b:a3:79:28:56:ed:27
			66:a4:9a:fa:75:47:67:0a:e2:f4:35:98:e8:9e:ad:35
			c2:b2:17:8b:98:72:c4:30:58:fd:13:b6:f4:01:d0:66
			56:be:61:85:55:dc:91:b6:4e:0a:3f:d4:3f:40:fa:a8
			92:5e:c5:dd:75:da:c3:27:33:59:43:47:74:fe:d2:28
			14:49:62:ee:39:22:34:6b:2f:e8:d1:ba:e9:95:6d:29
			d2:6f:8a:a2:fc:c8:da:f0:47:78:3b:2c:03:dc:fb:43
			31:9e:a1:cb:11:18:b9:0b:31:d3:86:43:68:f8:c4:bd
			ab:90:13:33:75:e9:b5:ca:74:c3:83:98:e9:91:3d:39
			fb:65:43:77:0b:b2:bc:3b:33:c2:91:7e:db:c3:a2:a1
			80:0b:a0:ce:cb:34:29:8b:24:52:25:aa:eb:bd:40:34
			cb
		Exponent (bits 24):
			01:00:01
	Extensions:
		Basic Constraints (critical):
			Certificate Authority (CA): TRUE
		Key Usage (critical):
			Certificate signing.
		Subject Key Identifier (not critical):
			6bbe9a650dbcaf5103c78daf8a2604d76a749f42
    Other Information:
	Public Key Id:
		6bbe9a650dbcaf5103c78daf8a2604d76a749f42



    Signing certificate...

Sample output for the machine certificate generation.
::

    test@ubuntu:~/Documents$ ./2-generate-client.sh 
    ** Note: Please use the --sec-param instead of --bits
    Generating a 2048 bit RSA private key...
    Generating a PKCS #10 certificate request...
    Generating a signed certificate...
    X.509 Certificate Information:
	Version: 3
	Serial Number (hex): 5d7a6402
	Validity:
		Not Before: Thu Sep 12 15:28:02 UTC 2019
		Not After: Sun Sep 09 15:28:02 UTC 2029
	Subject: C=US,O=Example,OU=Example,CN=Test Client
	Subject Public Key Algorithm: RSA
	Certificate Security Level: Low
		Modulus (bits 2048):
			00:bd:7f:0b:20:2e:fe:f1:49:91:71:fa:f1:72:76:6b
			c0:96:ce:e0:85:80:a3:6a:d2:9e:07:dd:02:94:4f:df
			c8:34:13:7d:d1:8f:b8:1b:1f:cf:b8:b7:ae:2f:dd:9a
			da:52:6e:a3:f4:73:20:63:32:46:c2:e1:94:73:6b:cd
			b4:e4:82:46:25:b0:62:f9:12:28:4f:4f:76:23:5c:47
			1b:f9:61:cd:68:c1:c1:17:93:90:3c:d2:2b:6e:82:c2
			a3:ca:80:b7:89:6e:b6:16:ae:47:05:e5:b4:07:bf:75
			d9:bd:aa:fe:79:77:72:6e:af:ed:5b:97:d1:e0:00:ba
			ab:6f:9e:1f:a6:d4:95:d7:d3:39:88:9b:58:88:28:a0
			7e:b6:fe:07:7e:68:ad:a1:d0:23:12:3d:96:b2:a8:8e
			73:66:c0:4f:10:a0:e5:9e:ab:2a:37:1d:83:b1:c3:e5
			7c:35:cc:20:05:7c:7e:41:89:f1:b3:6b:e4:00:f2:bc
			0b:08:55:07:b3:67:e4:14:1c:3c:64:1b:92:2d:d7:f0
			f7:d4:dc:d7:63:1e:fd:e4:98:bc:6b:f1:1a:a9:af:05
			7a:94:52:f5:b5:36:f0:0c:c0:41:0a:39:b7:fb:b3:50
			c1:ce:ee:24:56:61:77:9d:9e:e1:d0:e1:39:f0:cc:b6
			29
		Exponent (bits 24):
			01:00:01
	Extensions:
		Basic Constraints (critical):
			Certificate Authority (CA): FALSE
		Key Purpose (not critical):
			TLS WWW Client.
			TLS WWW Server.
		Subject Key Identifier (not critical):
			5a1a7316c4594cafafbeb45ddb49623af3a9f231
		Authority Key Identifier (not critical):
			6bbe9a650dbcaf5103c78daf8a2604d76a749f42
    Other Information:
	Public Key Id:
		5a1a7316c4594cafafbeb45ddb49623af3a9f231



    Signing certificate...

**Be sure to safeguard ca-key.pem!** Nobody except the CA itself needs
to have it. If some third party obtains it, you security is broken!
