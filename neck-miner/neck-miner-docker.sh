#! /bin/bash

name="neck-miner"
cname="${USER}-$name"

function get_docker_cid() {
    local cid
    cid=`docker ps -aqf "name=^$cname\$"`
    echo "$cid"
}

function start_docker() {
    echo "Attempting to docker start $cname..."
    docker start "$cname"
}

function enter_docker() {
    echo "Attempting to enter container $1 with docker exec..."
    docker exec -it "$1" /bin/bash
}

function run_docker() {
    echo "Attempting to docker run $cname..."
    docker run \
        --name "$cname" \
        -h "$cname" \
        -v $NEO_FUZZ_HOME:/neo-fuzz \
        -v $HOME/repos/git/lmcas:/lmcas \
        -it "$name" \
        /lmcas/neck/login-shell.sh
}

start_docker

if [[ $(docker ps -f "name=$cname" --format '{{.Names}}') == $cname ]]; then
    echo "Getting docker cid for $cname..."
    cid=`get_docker_cid`
    echo "Currently running $cname container with id: $cid"
    enter_docker $cid
else
    run_docker
fi
