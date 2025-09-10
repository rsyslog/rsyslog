# rsyslog-doc builds: A guide to building the rsyslog documentation for maintainers and contributors

*Documentation for building the documentation for ...*

This doc is intended for maintainers and regular contributors to
the project. The [`README.md`](README.md) doc covers the basic steps for one-off
builds from the `main` branch.

In all cases, the version number and release string do not *need* to be
manually specified, though the directions do provide steps for doing so
in at least one case.

Directions in this doc provide examples for building HTML and epub formats. See
the official http://www.sphinx-doc.org/en/stable/invocation.html doc for other
formats that Sphinx is capable of building. Some formats may require
installation of other packages for your operating system that are not
installed by following the current setup/prep directions in the
[`README.md`](README.md) file.


## Generating release version of the docs

These steps guide you through the steps necessary to build a version of the
docs that match those hosted on rsyslog.com or that are available via
distro packages.

These directions assume that `8.33.0` is the latest stable release version.
Substitute the actual latest stable release version as you follow the
steps in this document.


### Maintainer

#### Notes

- These directions are generalized as the specific steps are highly dependent
  on the production build/hosting environment.
- All steps provided are intended to be run from the maintainer's dev
  workstation.

#### Directions

1. Review the [`README.md`](README.md) file for instructions that cover
   installing `pip`, setting up the virtual environment and installing
   the latest version of the Sphinx package.
1. Change to the `doc/` folder within the rsyslog repository
1. Merge `main` into the current stable branch (e.g., `v8-stable`)
1. Tag the stable branch
1. Push all changes to the remote
1. Run the `tools/release_build.sh` script
1. Sync the contents of the `build` directory over to the web server
1. Sync the latest release doc tarball to where the previous release
   tarball is hosted. Update references to this tarball as necessary.


### Contributors

#### Obtain stable docs

You have several options:

- Download the release tarball
  (http://www.rsyslog.com/download/download-v8-stable/)
- Download a zip file from GitHub
- Clone GitHub repo and checkout the latest stable tag

#### Build from official release tarball

1. Decompress the tarball
1. Change your current working directory to that of the directory containing
   the `source` directory, [`README.md`](README.md) and other content that
   was within the tarball
    - For example, `cd /tmp/rsyslog-8.33.0`
      (the `rsyslog-8.33.0` folder is within the tarball)
1. Run `sphinx-build -b html source build`

You may need to first follow the directions in the [`README.md`](README.md)
doc to create or activate your virtual environment if the `sphinx` package
is not already installed and known to your installation of Python.

#### Build from GitHub download

1. Visit https://github.com/rsyslog/rsyslog
1. Click on 'Branch: main' and choose 'Tags' from the right-column
1. Select the `v8.33.0` tag
1. Click on the 'Clone or download' green button to the far right
1. Click 'Download ZIP' and save the file to your system
1. Decompress the zip file
1. Change your current working directory to that of the directory containing
   the `source` directory, [`README.md`](README.md) and other content that was
   within the tarball.
    - For example, `cd C:\users\deoren\Downloads\rsyslog-8.33.0`
      (the `rsyslog-8.33.0` folder is within the tarball)
1. `sphinx-build -D version="8.33" release="8.33.0" -b html source build`
    - You have to specify the `version` and `release` values here for now
      because we do not modify the `source/conf.py` file in the stable branch
      or its tags to hard-code the new version number.

You may need to first follow the directions in the [`README.md`](README.md)
doc to create or activate your virtual environment if the `sphinx` package
is not already installed and known to your installation of Python.

#### Build from Git repo

1. Review the [`README.md`](README.md) file for instructions that cover
   installing `pip`, setting up the virtual environment and installing
   the latest version of the Sphinx package.
1. Run `git clone https://github.com/rsyslog/rsyslog.git`
1. Move to doc directory.
1. Run `git checkout v8.33.0`
1. Run `sphinx-build -D version="8.33" release="8.33.0" -b html source build`

You may need to first follow the directions in the [`README.md`](README.md)
doc to create or activate your virtual environment if the `sphinx` package
is not already installed and known to your installation of Python.


## Generating development builds

### Obtain docs

You have several options for obtaining a snapshot of the branch you would
like to build (most often this is the `main` branch):

- Download a zip file from GitHub
- Clone GitHub repo using Git

We do not (currently) build tarballs ourselves for in-development snapshots
of the docs.


### Build from GitHub download

These directions assume that you wish to build the docs from the `main`
branch. Substitute `main` for any other branch that you wish to build.

1. Follow the steps previously given for downloading the zip file from GitHub,
   this time substituting the tag download option for the branch you wish
   to obtain doc sources for.
1. Decompress the zip file and change your current working directory to
   that path.
1. Run `sphinx-build -D release="dev-build" -b html source build` to generate
   HTML format and `sphinx-build -b epub source build` to build an epub file.



### Build from Git repo

These directions assume that you wish to build the docs from the `main`
branch. Substitute `main` for any other branch that you wish to build.

The Sphinx build configuration file will attempt to pull the needed information
from the Git repo in order to generate a "dev build" release string. This
release string is prominently displayed in various places throughout the docs
and is useful to identify a dev build from a release set of documentation.

1. Review the [`README.md`](README.md) file for instructions that cover
   installing `pip`, setting up the virtual environment and installing
   the latest version of the Sphinx package.
1. Run `git clone https://github.com/rsyslog/rsyslog.git`
1. Run `git checkout main`
1. Change to doc folder
1. Run `sphinx-build -b html source build` to generate HTML format and
   `sphinx-build -b epub source build` to build an epub file.
