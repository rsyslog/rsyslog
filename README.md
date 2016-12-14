rsyslog-docs
============

Documentation for the rsyslog project
-------------------------------------

Documentation for rsyslog is generated with the (Python) Sphinx documentation
processor. There is also a procedure which automatically picks up the most
recent doc from the git archive, generates the html pages and uploads them
to rsyslog.com.

## Learning the doc tools

If you are new to rst and Sphinx, visit the Sphinx doc to get started:
http://sphinx-doc.org/contents.html

## Importing missing content
While this hasn't happened for some time now, there might be cases 
where a page from previous html-only doc seems to
be missing in rsyslog-doc. To recover it, check out the respective version (v8.1.6
is the latest v8 with html doc) and use this too to convert to rst:

$ pandoc -f html -t rst <html_file> -o <output_file>

Nowaday, it would be rather unexpected that this might really be needed, but
we still wanted to include the information.

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
