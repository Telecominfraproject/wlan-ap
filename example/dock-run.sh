#!/bin/bash -ex

tag=$(echo ${PWD} | tr / - | cut -b2- | tr A-Z a-z)
groups=$(id -G | xargs -n1 echo -n " --group-add ")
params="-v ${PWD}:${PWD} -i --rm -w ${PWD} -u"$(id -u):$(id -g)" $groups -v/etc/passwd:/etc/passwd -v/etc/group:/etc/group -t ${tag}"

cd example/docker
docker build --tag=${tag} .
cd -

docker run $params $@
