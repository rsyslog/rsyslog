# this is used to create an rsyslog release tarball.
# it must be executed from the root of the rsyslog-doc
# project.
rm -rf ./build
sphinx-build -b html source build
rm rsyslog-doc.tar.gz
cd build
tar -czf ../rsyslog-doc.tar.gz *
cd ..
cd ..
