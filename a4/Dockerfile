FROM --platform=linux/amd64 ubuntu:22.04

LABEL description="CMSC417 Docker Image"

ENV TZ="America/New_York"

RUN apt-get -yq update &&     apt-get -yq upgrade &&     apt-get -yq install     git 	python3 	python3-pip 	python3-dev     openssl     libssl-dev     default-jdk 	tcpdump     cargo 	nano \ 
	emacs-nox 	unzip \ 
    make     gcc     g++     emacs-nox     vim     sudo     libssl-dev     libncurses-dev     libev-dev     cmake     protobuf-compiler     htop     curl     bison     flex \ 
    build-essential     valgrind &&     ln -fs /usr/share/zoneinfo/ /etc/localtime && echo  > /etc/timezone

RUN groupadd -g 1000 student &&     useradd -r -u 1000 -g student student &&     usermod --shell /bin/bash student &&     echo 'student ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers &&     mkdir /home/student &&     chown student:student /home/student

USER student:student

