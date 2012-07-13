// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
#ifndef CEPH_LIBRBD_AIOREQUEST_H
#define CEPH_LIBRBD_AIOREQUEST_H

namespace librbd {

  typedef enum {
    AIO_TYPE_READ = 0,
    AIO_TYPE_WRITE,
    AIO_TYPE_DISCARD
  } aio_type_t;

  class ImageCtx;

  class AioRequest
  {
    AioRequest(ImageCtx *ictx, const string &oid, uint64_t ofs,
	       size_t len, uint64_t overlap, char *buf, aio_type_t type,
	       Context *completion);
    bool read_from_parent(int r);

  private:
    AioRequest(const AioRequest &o);
    void operater=(const AioRequest &o);
    ImageCtx *m_ictx;
    string m_oid;
    uint64_t m_ofs;
    size_t m_len;
    uint64_t m_overlap;
    char *m_buf;
    aio_type_t m_type;
    AioCompletion *m_parent_comp;
    bool m_tried_parent;
    Context *m_completion;
  };
}

#endif
