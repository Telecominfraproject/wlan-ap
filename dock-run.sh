#!/bin/bash -ex

tag=$(echo ${PWD} | tr / - | cut -b2- | tr A-Z a-z)
groups=$(id -G | xargs -n1 echo -n " --group-add ")
params="-v ${PWD}:${PWD} --rm -w ${PWD} -u"$(id -u):$(id -g)" $groups -v/etc/passwd:/etc/passwd:ro -v/etc/group:/etc/group:ro -v$HOME/.gitconfig:$HOME/.gitconfig:ro ${tag}"

docker build --tag=${tag} docker

docker run $params $@
