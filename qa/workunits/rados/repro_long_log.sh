#!/bin/bash

set -ex

function test_log_size()
{
    PGID=$1
    EXPECTED=$2
    ceph tell osd.\* flush_pg_stats
    sleep 3
    ceph pg $PGID query | jq .info.stats.log_size | grep "${EXPECTED}"
}

ceph osd pool rm test test --yes-i-really-really-mean-it
ceph osd pool create test 1 1 || true
POOL_ID=$(ceph osd dump --format json | jq '.pools[] | select(.pool_name == "test") | .pool')
PGID="${POOL_ID}.0"

ceph tell osd.\* injectargs -- --osd-min-pg-log-entries 10
ceph tell osd.\* injectargs -- --osd-max-pg-log-entries 20
ceph tell osd.\* injectargs -- --osd-pg-log-trim-min 10
ceph tell osd.\* injectargs -- --osd-pg-log-dups-tracked 10

touch foo
for i in $(seq 1 20)
do
    rados -p test put foo foo
done

test_log_size $PGID 20

rados -p test rm foo

# generate error entries
for i in $(seq 1 20)
do
    rados -p test rm foo || true
done

# this demonstrates the problem - it should fail
test_log_size $PGID 41

# regular write should trim the log
rados -p test put foo foo
test_log_size $PGID 22
