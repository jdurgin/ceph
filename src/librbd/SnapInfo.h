// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
#ifndef CEPH_LIBRBD_SNAPINFO_H
#define CEPH_LIBRBD_SNAPINFO_H


struct SnapInfo {
    snap_t id;
    uint64_t size;
    uint64_t features;
    cls_client::parent_info parent;
    SnapInfo(snap_t _id, uint64_t _size, uint64_t _features,
	     cls_client::parent_info _parent) :
      id(_id), size(_size), features(_features), parent(_parent) {}
  };

