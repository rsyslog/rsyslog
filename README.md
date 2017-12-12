rsyslog-docs
============

Documentation for the rsyslog project
-------------------------------------

Documentation for rsyslog is generated with the (Python) Sphinx documentation
processor. There is also a procedure which automatically picks up the most
recent doc from the git archive, generates the html pages and uploads them
to rsyslog.com.

## Learning the doc tools

If you are new to rst and Sphinx, see the Sphinx documentation to get started:
http://www.sphinx-doc.org/en/stable/contents.html

## Instructions

These assume default installs of Python for Windows and Linux. Because the
[Sphinx project recommends using Python 2.7](http://www.sphinx-doc.org/en/stable/install.html),
that is what is shown here.

### Assumptions

- You wish to install the `pip` Python package as a standard user, which places
  installed packages into that user's home directory. Remove the `--user`
  flag if you wish to install system-wide for all users instead.

- You wish to use a virtual environment to install Sphinx and its dependencies
  into a dedicated environment instead of installing alongside packages that
  were installed system-wide or to the user's home directory with the `--user`
  flag. If you wish to install the `sphinx` package and all dependent packages
  for all users of the system, then uou will need to run the package
  installation commands as an elevated user account (e.g., `sudo`, `su` or
  with administrator rights on a Windows system).

### Generate HTML Documentation on Linux

1. Download the pip installer from here: https://bootstrap.pypa.io/get-pip.py
1. Run: `python ./get-pip.py --user`
1. Run: `python -m pip install virtualenv --user`
1. Run: `python -m virtualenv rsyslog-docs-build`
1. Run: `source rsyslog-docs-build/bin/activate`
1. Run `pip install --user sphinx`
1. Checkout Branch in Repo –
  1. Run: `git clone https://github.com/rsyslog/rsyslog-doc.git`
  1. Run: `cd rsyslog-doc`
  1. Run: `git checkout v8-stable`
1. Run: `sphinx-build -b html source build`
1. (Optional):  Run `sphinx-build -b epub source build` to generate an epub file
1. open rsyslog-doc/build/index.html in a browser

### Generate HTML Documentation on Windows

1. Download the pip installer from here: https://bootstrap.pypa.io/get-pip.py
1. Download and install Git for windows if you don’t already have Git:
  1. https://git-scm.com/download/win
  1.  Install Git for Windows.
1. Run: `c:\python27\python get-pip.py --user`
1. Run: `c:\python27\python -m pip install virtualenv --user`
1. Run: `c:\python27\python -m virtualenv rsyslog-docs-build`
1. Run: `rsyslog-docs-build\Scripts\activate.bat`
1. Run `cd rsyslog-docs-build`
1. Checkout Branch in Repo –
  1. Run: `git clone https://github.com/rsyslog/rsyslog-doc.git`
  1. Run: `cd rsyslog-doc`
  1. Run: `git checkout v8-stable`
1. Run: `sphinx-build -b html source build`
1. (Optional):  Run `sphinx-build -b epub source build` to generate an epub file
1. open rsyslog-doc/build/index.html in a browser
