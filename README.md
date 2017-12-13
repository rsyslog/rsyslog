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
  for all users of the system, then you will need to run the package
  installation commands as an elevated user account (e.g., `sudo`, `su` or
  with administrator rights on a Windows system).

- You are running through these steps for the first time. Leave out the steps
  involving installation of packages and applications if generating an updated
  copy of the documentation.

### Prep environment

The first part of the process is a little different depending on your OS. The
later steps are identical, so those steps have been covered in one place.

#### Linux

1. Download the pip installer from https://bootstrap.pypa.io/get-pip.py
1. Install `pip` locally instead of system-wide
    1. `python ./get-pip.py --user`
1. Install `virtualenv` package and create new virtual environment
    1. `python -m pip install virtualenv --user`
    1. `python -m virtualenv rsyslog-docs-build`
    1. `source rsyslog-docs-build/bin/activate`

#### Windows

1. Download the pip installer from https://bootstrap.pypa.io/get-pip.py
1. Download and install Git for windows from https://git-scm.com/download/win
1. Install `pip` locally instead of system-wide
    1. `c:\python27\python get-pip.py --user`
1. Install `virtualenv` package and create new virtual environment
    1. `c:\python27\python -m pip install virtualenv --user`
    1. `c:\python27\python -m virtualenv rsyslog-docs-build`
    1. `rsyslog-docs-build\Scripts\activate.bat`

#### Windows and Linux

1. Install `sphinx` package in our new virtual environment instead of system-wide
    1. `pip install sphinx`    
1. Clone the official Git repo
    1. `git clone https://github.com/rsyslog/rsyslog-doc.git`
1. Checkout either the current stable or development (aka, "master") branch
    1. `cd rsyslog-doc`
    1. `git checkout BRANCH_NAME_HERE`
        - Choose the `v8-stable` branch for coverage of features currently
          available in the latest stable release
        - Choose the `master` branch for coverage of upcoming features and fixes
1. Optional: If you have previously cloned the repo, run `git pull` to update it
   with new changes before continuing.

### Generate documentation

1. Generate HTML format
    1. `sphinx-build -b html source build`
1. Generate EPUB format
    1. `sphinx-build -b epub source build`
1. Review generated contents
    - Open rsyslog-doc/build/index.html in a browser
    - Use Calibre, Microsoft Edge, Okular, Google Play Books or any other
      EPUB compatible reader to view the rsyslog-doc/build/rsyslog.epub file
