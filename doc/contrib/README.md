# CONTRIB

*Scripts and other unofficial content for use with maintaining
or building the rsyslog documentation.*

## Scripts

### cron script: build all branches (cron_build_all_branches.sh)

This script is intended to run on-demand or as a cron script
to build HTML and epub formats for ALL branches of a
`rsyslog-doc` fork. One of the regular contributors uses it
on their VPS to regularly build all branches in order to showcase
prototype or "almost ready" changes to be added to the `master`
branch of the official `rsyslog-doc` repo.

While usable, this script needs more work to make it both efficient
(right now all branches are built every time the script is run)
and easy to use (needs to have hard-coded values exposed through
a configuration file). Please file an issue in the official
`rsyslog-doc` project if you run into problems or have questions
about using this script.


#### Requirements

- Bash
- Python
- Git
- Sphinx Python package

Instructions for installing Git and the Sphinx package can be found in
the [main project README](../README.md). See your distro documentation
for installing Bash and Python if they are not already installed.
