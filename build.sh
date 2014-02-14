#!/bin/bash

MASTERBRANCH=v8-devel

for version in `git branch | cut -c3-`
  do 
    VER=$(if [[ ${version} == 'master' ]];then echo ${MASTERBRANCH} ; else echo ${version}; fi)
    git checkout ${version}
    sphinx-build -b html source ${VER}
done
