#!/bin/bash

set -x

msg_size=(128 256 512 1024 2048 4096 8192)

res_dir=/data/result/raw  # result directory

source $(dirname "$0")/config.sh

mkdir -p $res_dir

function run_zk() {
    stop_zk || true
    rm -rf $zkdir/zookeeper
    $zkdir/bin/zkServer.sh start
}

function stop_zk() {
    $zkdir/bin/zkServer.sh stop
}

function run_ncl_server() {
    kill_ncl_server || true
    for i in ${!replica[@]}
    do
        r=${replica[$i]}
        ssh -o StrictHostKeyChecking=no $user@$r "nohup $ncl_dir/server > $ncl_dir/server$i.log 2>&1 &"
    done
}

function kill_ncl_server() {
    for r in ${replica[@]}
    do
        ssh -o StrictHostKeyChecking=no $user@$r "pid=\$(sudo lsof -i :8011 | awk 'NR==2 {print \$2}') ; \
            sudo kill -2 \$pid 2> /dev/null || true "
        sleep 1
        ssh -o StrictHostKeyChecking=no $user@$r "ps aux | grep $ncl_dir/server"
    done
}

function run_cephfs() {
    csv=$res_dir/cephfs_write.csv
    rm $csv
    for ms in ${msg_size[@]}
    do
        lat=`$ncl_dir/posix_client $ms w /mnt/cephfs/test.txt normal | grep average | awk '{print $2}'`
        echo $ms,$lat >> $csv
    done
}

function run_sync() {
    csv=$res_dir/sync_write.csv
    rm $csv
    for ms in ${msg_size[@]}
    do
        lat=`$ncl_dir/posix_client $ms w /mnt/cephfs/test.txt sync 1 | grep average | awk '{print $2}'`
        echo $ms,$lat >> $csv
    done
}

function run_ncl() {
    run_zk

    run_ncl_server

    csv=$res_dir/sync_ncl_write.csv
    rm $csv
    for ms in ${msg_size[@]}
    do
        lat=`$ncl_dir/posix_client $ms w ./test.txt ncl | grep average | awk '{print $2}'`
        echo $ms,$lat >> $csv
    done

    kill_ncl_server

    stop_zk
}

run_cephfs
run_sync
run_ncl
