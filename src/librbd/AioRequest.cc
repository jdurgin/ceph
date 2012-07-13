// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#include "common/ceph_context.h"
#include "common/dout.h"
#include "include/Context.h"

#include "AioRequest.h"

namespace librbd {
  AioRequest(ImageCtx *ictx, const string &oid, uint64_t ofs,
	     size_t len, uint64_t overlap, char *buf, aio_type_t type,
	     Context *completion) :
    m_ictx(ictx), m_oid(oid), m_ofs(ofs), m_len(len), m_overlap(overlap),
    m_buf(buf), m_type(type), m_parent_comp(NULL), m_tried_parent(false),
    m_completion(completion)
  {
  }

  bool read_from_parent(int r)
  {
    if (tried_parent || !ictx->should_read_parent(r, ofs, len, overlap))
      return false;

    ldout(cct, 20) << "Reading from parent..." << dendl;
    tried_parent = true;
    parent_completion = aio_create_completion(this, rbd_aio_read_parent_cb);
    buffer::ptr bp(len);
    data_bl.append(bp);
    aio_read(ictx->parent, ofs, len, data_bl.c_str(), parent_completion);
    return true;
  }
}
