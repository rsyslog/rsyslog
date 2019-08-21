FROM	centos:7
RUN	yum -y update
RUN	yum -y install mock
RUN	mkdir /home/pkg
WORKDIR	/home/pkg
RUN	mkdir /private-files
VOLUME	/private-files
COPY	scriptsample.sh .
