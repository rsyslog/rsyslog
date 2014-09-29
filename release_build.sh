# this is used to create an rsyslog release tarball.
# it must be executed from the root of the rsyslog-doc
# project.
rm -rf ./build
sphinx-build -b html source build
rm rsyslog-doc.tar.gz
tar -czf rsyslog-doc.tar.gz build source LICENSE README.md build.sh
cd ..
