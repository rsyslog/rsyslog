Rsyslog Documentation Review Proposal
=====================================

Currently the Rsyslog documentation is spread over many places. It's not logical
and well-organized as well. The objective of this proposal is to address those issues
and establish a procedure for further development of the documentation.

.. note::

    We SHOULD NOT write examples using mixed formats, RainerScripts and legacy. It confused
    the readers. We can provide them in both formats, but should not mix them.


Compile information from current sources of information
-------------------------------------------------------

Below are listed the official locations where documentation about Rsyslog can be found.

	* Wiki: http://wiki.rsyslog.com/index.php/Main_Page
	* Rainer's blog: https://rainer.gerhards.net/2012/10/how-to-use-rsyslogs-ruleset-and-call.html
	* Issues: https://github.com/rsyslog/rsyslog
	* docs: https://github.com/rsyslog/rsyslog-doc
	* Forum: http://kb.monitorware.com/configuration-f36.html
	* https://www.youtube.com/user/rainergerhards


Add a Cookbook Section
----------------------

We should create some cookbooks to help people get started with Rsyslog.
Some candidates to be a cookbook are below.

	* http://sickbits.net/log-storage-and-analysis-infrastructure-reliable-logging-and-analysis-with-rsyslog-and-relp/
	* http://kb.monitorware.com/omfile-with-dynfile-syslogfacility-text-t12515.html
	* http://www.freeipa.org/page/Howto/Centralised_Logging_with_Logstash/ElasticSearch/Kibana

Add a subsection called "processing logs from". We'd place articles that'd would help people with specific
common scenarios for a specific log sender application.

Add a Reference Section
-----------------------

This section would have all the reference configuration of all possible tags, in both formats, RainerScript
and legacy.


Write articles that address common problems
-------------------------------------------

Some of the common are following.

    * https://github.com/rsyslog/rsyslog/issues/160
    * http://kb.monitorware.com/nginx-logging-rsyslog-t12359.html
    * http://trac.nginx.org/nginx/ticket/677


Extra
-----

Some resources worth taking a look.

    * https://docs.redhat.com/en/documentation/Red_Hat_Enterprise_Linux/7/html/system_administrators_guide/ch-viewing_and_managing_log_files
	* https://www.usenix.org/system/files/login/articles/06_lang-online.pdf
	* https://rsyslog.readthedocs.io/_/downloads/en/latest/pdf/
	* http://download.rsyslog.com/rainerscript2_rsyslog.conf
	* https://people.redhat.com/pvrabec/rpms/rsyslog/rsyslog-example.conf
	* http://www.rsyslog.com/doc/syslog_parsing.html


Initial Summary
---------------

	- Troubleshooting

		* http://www.rsyslog.com/how-can-i-check-the-config/

Proposed Documentation Structure
--------------------------------

 toctree::

    new_documentation/index

