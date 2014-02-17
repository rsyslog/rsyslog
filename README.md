rsyslog-docs (master: v8-devel)
===============================

Documentation for the rsyslog project
-------------------------------------

This is a work in progress. We are currently migrating over to a new document
generation framework.

The process of this work will be done as follows:

1. Complete v5-stable documentation
2. Merge v5-stable into v7-stable branch
3. Update v7-stable branch with all new documentation and materials specific for that version
4. Repeat 2 and 3 merging current repo with next highest until Master is merged and updated.

Current Status -
* v5-stable - (In Development)
* v7-stable - (In Development)
* v8-devel - (In Development)

## Learning the doc tools

If you are new to rst and Sphinx, visit the Sphinx doc to get started:
http://sphinx-doc.org/contents.html

## Instructions

These assume default installs of Python for Windows and Linux

### Generate HTML Documentation on Linux

1.  Download the pip installer from here: https://raw.github.com/pypa/pip/master/contrib/get-pip.py
2.  Run: python ./get-pip.py
3.  Run: pip install sphinx
4.  Checkout Branch in Repo –
  1.  Run: git clone https://github.com/rsyslog/rsyslog-doc.git
  2.  Run: cd rsyslog-doc
  3.  Run: git checkout v5-stable
5.  Run: sphinx-build -b html source build
6.  open rsyslog-doc/build/index.html in a browser

###Generate HTML Documentation on Windows

1.  Download the pip installer from here: https://raw.github.com/pypa/pip/master/contrib/get-pip.py
2.  Download and install Git for windows if you don’t already have Git:
  1.  https://code.google.com/p/msysgit/downloads/list?can=3&q=full+installer+official+git&colspec=Filename+Summary+Uploaded+ReleaseDate+Size+DownloadCount
  2.  Install Git for Windows.
3.  Run: c:\python27\python get-pip.py
4.  Run: c:\python27\scripts\pip install sphinx
5.  Checkout Branch in Repo –
  1.  Run: git clone https://github.com/rsyslog/rsyslog-doc.git
  2.  Run: cd rsyslog-doc
  3.  Run: git checkout v5-stable
6.  Run: c:\python27\scripts\sphinx-build -b html source build
7. open rsyslog-doc/build/index.html in a browser
