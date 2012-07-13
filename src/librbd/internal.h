// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
#ifndef CEPH_LIBRBD_INTERNAL_H
#define CEPH_LIBRBD_INTERNAL_H

#include <inttypes.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "include/buffer.h"
#include "include/rbd/librbd.hpp"
#include "include/rbd_types.h"

#include "librbd/ImageCtx.h"

namespace librbd {

  enum {
    l_librbd_first = 26000,

    l_librbd_rd,               // read ops
    l_librbd_rd_bytes,         // bytes read
    l_librbd_rd_latency,       // average latency
    l_librbd_wr,
    l_librbd_wr_bytes,
    l_librbd_wr_latency,
    l_librbd_discard,
    l_librbd_discard_bytes,
    l_librbd_discard_latency,
    l_librbd_flush,

    l_librbd_aio_rd,               // read ops
    l_librbd_aio_rd_bytes,         // bytes read
    l_librbd_aio_rd_latency,
    l_librbd_aio_wr,
    l_librbd_aio_wr_bytes,
    l_librbd_aio_wr_latency,
    l_librbd_aio_discard,
    l_librbd_aio_discard_bytes,
    l_librbd_aio_discard_latency,

    l_librbd_snap_create,
    l_librbd_snap_remove,
    l_librbd_snap_rollback,

    l_librbd_notify,
    l_librbd_resize,

    l_librbd_last,
  };

  const std::string id_obj_name(const std::string &name)
  {
    return RBD_ID_PREFIX + name;
  }

  const std::string header_name(const std::string &image_id)
  {
    return RBD_HEADER_PREFIX + image_id;
  }

  const std::string old_header_name(const std::string &image_name)
  {
    return image_name + RBD_SUFFIX;
  }

  int detect_format(librados::IoCtx &io_ctx, const std::string &name,
		    bool *old_format, uint64_t *size);

  int snap_set(ImageCtx *ictx, const char *snap_name);
  int list(librados::IoCtx& io_ctx, std::vector<std::string>& names);
  int create(librados::IoCtx& io_ctx, const char *imgname, uint64_t size,
	     int *order, bool old_format);
  int rename(librados::IoCtx& io_ctx, const char *srcname, const char *dstname);
  int info(ImageCtx *ictx, image_info_t& info, size_t image_size);
  int remove(librados::IoCtx& io_ctx, const char *imgname,
	     ProgressContext& prog_ctx);
  int resize(ImageCtx *ictx, uint64_t size, ProgressContext& prog_ctx);
  int resize_helper(ImageCtx *ictx, uint64_t size, ProgressContext& prog_ctx);
  int snap_create(ImageCtx *ictx, const char *snap_name);
  int snap_list(ImageCtx *ictx, std::vector<snap_info_t>& snaps);
  int snap_rollback(ImageCtx *ictx, const char *snap_name,
		    ProgressContext& prog_ctx);
  int snap_remove(ImageCtx *ictx, const char *snap_name);
  int add_snap(ImageCtx *ictx, const char *snap_name);
  int rm_snap(ImageCtx *ictx, const char *snap_name);
  int ictx_check(ImageCtx *ictx);
  int ictx_refresh(ImageCtx *ictx);
  int copy(ImageCtx& srci, librados::IoCtx& dest_md_ctx, const char *destname);

  int open_parent(ImageCtx *ictx, ImageCtx **parent_ctx,
		  std::string *parent_pool_name,
		  std::string *parent_image_name);
  int open_image(ImageCtx *ictx, bool watch);
  void close_image(ImageCtx *ictx);

  /* cooperative locking */
  int list_locks(ImageCtx *ictx,
                 std::set<std::pair<std::string, std::string> > &locks,
                 bool &exclusive);
  int lock_exclusive(ImageCtx *ictx, const std::string& cookie);
  int lock_shared(ImageCtx *ictx, const std::string& cookie);
  int unlock(ImageCtx *ictx, const std::string& cookie);
  int break_lock(ImageCtx *ictx, const std::string& lock_holder,
                 const std::string& cookie);

  void trim_image(ImageCtx *ictx, uint64_t newsize, ProgressContext& prog_ctx);
  int read_rbd_info(librados::IoCtx& io_ctx, const std::string& info_oid,
		    struct rbd_info *info);

  int touch_rbd_info(librados::IoCtx& io_ctx, const std::string& info_oid);
  int rbd_assign_bid(librados::IoCtx& io_ctx, const std::string& info_oid,
		     uint64_t *id);
  int read_header_bl(librados::IoCtx& io_ctx, const std::string& md_oid,
		     ceph::bufferlist& header, uint64_t *ver);
  int notify_change(librados::IoCtx& io_ctx, const std::string& oid,
		    uint64_t *pver, ImageCtx *ictx);
  int read_header(librados::IoCtx& io_ctx, const std::string& md_oid,
		  struct rbd_obj_header_ondisk *header, uint64_t *ver);
  int write_header(librados::IoCtx& io_ctx, const std::string& md_oid,
		   ceph::bufferlist& header);
  int tmap_set(librados::IoCtx& io_ctx, const std::string& imgname);
  int tmap_rm(librados::IoCtx& io_ctx, const std::string& imgname);
  int rollback_image(ImageCtx *ictx, uint64_t snap_id,
		     ProgressContext& prog_ctx);
  void image_info(const ImageCtx& ictx, image_info_t& info, size_t info_size);
  std::string get_block_oid(const std::string &object_prefix, uint64_t num,
			    bool old_format);
  uint64_t get_max_block(uint64_t size, uint8_t obj_order);
  uint64_t get_block_size(uint8_t order);
  uint64_t get_block_num(uint8_t order, uint64_t ofs);
  uint64_t get_block_ofs(uint8_t order, uint64_t ofs);
  int check_io(ImageCtx *ictx, uint64_t off, uint64_t len);
  int init_rbd_info(struct rbd_info *info);
  void init_rbd_header(struct rbd_obj_header_ondisk& ondisk,
			      uint64_t size, int *order, uint64_t bid);

  int64_t read_iterate(ImageCtx *ictx, uint64_t off, size_t len,
		       int (*cb)(uint64_t, size_t, const char *, void *),
		       void *arg);
  ssize_t read(ImageCtx *ictx, uint64_t off, size_t len, char *buf);
  ssize_t write(ImageCtx *ictx, uint64_t off, size_t len, const char *buf);
  int discard(ImageCtx *ictx, uint64_t off, uint64_t len);
  int aio_write(ImageCtx *ictx, uint64_t off, size_t len, const char *buf,
                AioCompletion *c);
  int aio_discard(ImageCtx *ictx, uint64_t off, size_t len, AioCompletion *c);
  int aio_read(ImageCtx *ictx, uint64_t off, size_t len,
               char *buf, AioCompletion *c);
  int flush(ImageCtx *ictx);
  int _flush(ImageCtx *ictx);

  ssize_t handle_sparse_read(CephContext *cct,
			     ceph::bufferlist data_bl,
			     uint64_t block_ofs,
			     const std::map<uint64_t, uint64_t> &data_map,
			     uint64_t buf_ofs,
			     size_t buf_len,
			     int (*cb)(uint64_t, size_t, const char *, void *),
			     void *arg);

  AioCompletion *aio_create_completion() {
    AioCompletion *c = new AioCompletion();
    return c;
  }
  AioCompletion *aio_create_completion(void *cb_arg, callback_t cb_complete) {
    AioCompletion *c = new AioCompletion();
    c->set_complete_cb(cb_arg, cb_complete);
    return c;
  }

  // raw callbacks
  void rados_cb(rados_completion_t cb, void *arg);
  void rados_aio_sparse_read_cb(rados_completion_t cb, void *arg);
}

#endif
