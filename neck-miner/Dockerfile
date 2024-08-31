# Pull base image.
FROM buildpack-deps:bionic

RUN apt-get update \
 && apt-get install -y sudo
RUN adduser --disabled-password --gecos '' docker
RUN adduser docker sudo
RUN echo '%sudo ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers
USER docker

COPY install.sh .
RUN bash ./install.sh

# Where we're going to put the llvm install, which is in the shared fs we
# mount while running the docker container, but NOT in the neck source
# directory.
ENV PATH=$PATH:/lmcas/local/bin:/lmcas/local/sbin

USER root

#WORKDIR /neck-identification
#COPY build-llvm.sh .
#RUN bash /neck-identification/build-llvm.sh
#COPY . .
#RUN bash /neck-identification/build.sh

