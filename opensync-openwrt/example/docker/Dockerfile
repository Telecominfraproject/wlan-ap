FROM ubuntu:16.04

RUN apt-get update \
    && apt-get install -y \
            time git-core build-essential gcc-multilib \
            libncurses5-dev zlib1g-dev gawk flex gettext wget unzip python \
            python3 python3-pip openvswitch-common openvswitch-switch libssl-dev \
    && apt-get clean

RUN pip3 install kconfiglib
