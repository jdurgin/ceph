#!/bin/bash -ex

setup_images() {
    # create
    rbd create -s 1024 foo
    rbd create -s 512 --image-format 2 bar
    rbd create -s 2048 --image-format 2 baz
    rbd create -s 1 quux

    # snapshot
    rbd snap create bar@snap
    rbd resize -s 1024 bar
    rbd snap create bar@snap2
    rbd snap create foo@snap

    # clone
    rbd snap protect bar@snap
    rbd clone bar@snap data/child
    rbd snap create data/child@snap
    rbd flatten data/child

    # lock
    rbd lock add quux id
    rbd lock add baz id1 --shared tag
    rbd lock add baz id2 --shared tag
    rbd lock add baz id3 --shared tag
}

remove_images() {
    rbd snap remove data/child@snap
    rbd remove data/child
    rbd snap unprotect bar@snap
    rbd snap purge bar
    rbd snap purge foo
    rbd rm foo
    rbd rm bar
    rbd rm quux
    rbd rm baz
}

VENV=virtualenv
virtualenv "$VENV" && $VENV/bin/pip install cram
TESTDIR=$(mktemp -d)
cp /home/joshd/ceph/src/test/cli-integration/rbd/*.t $TESTDIR

#setup_images
"$VENV/bin/cram" -v "$@" -- "$TESTDIR"/*.t
#remove_images

rm -rf -- "$TESTDIR"
echo OK
exit 0
