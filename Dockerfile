#FROM ubuntu:18.04
FROM ubuntu:20.04

RUN apt-get update \
    && DEBIAN_FRONTEND="noninteractive" apt-get -y install tzdata \
    && apt-get install -y \
            time git-core build-essential gcc-multilib \
            libncurses5-dev zlib1g-dev gawk flex gettext wget unzip python \
            python3 python3-pip python3-yaml openvswitch-common openvswitch-switch libssl-dev \
    && apt-get clean
RUN git config --global user.email "you@example.com"
RUN git config --global user.name "Your Name"
RUN pip3 install kconfiglib
