#!/bin/bash -e

# fsid
if [ -e fsid ] ; then
    fsid=`cat fsid`
else
    fsid=`uuidgen`
    echo $fsid > fsid
fi
echo "fsid $fsid"

sudo ../src/cephadm/cephadm rm-cluster --force --fsid $fsid

