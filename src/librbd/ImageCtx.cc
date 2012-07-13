// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
#include <errno.h>

#include "common/ceph_context.h"
#include "common/dout.h"
#include "common/errno.h"
#include "common/perf_counters.h"
#include "common/snap_types.h"

#include "librbd/cls_rbd_client.h"
#include "librbd/ImageCtx.h"
#include "librbd/librbd_internal.h"
#include "librbd/SnapInfo.h"
#include "librbd/WatchCtx.h"

#include "osdc/ObjectCacher.h"

#define dout_subsys ceph_subsys_rbd
#undef dout_prefix
#define dout_prefix *_dout << "librbd::ImageCtx: "

using std::map;
using std::pair;
using std::set;
using std::string;
using std::vector;

using ceph::bufferlist;
using librados::snap_t;
using librados::IoCtx;

namespace librbd {
  ImageCtx::ImageCtx(const string &image_name, const string &image_id,
		     const char *snap, IoCtx& p)
    : cct((CephContext*)p.cct()),
      perfcounter(NULL),
      snap_id(CEPH_NOSNAP),
      snap_exists(true),
      exclusive_locked(false),
      name(image_name),
      wctx(NULL),
      refresh_seq(0),
      last_refresh(0),
      refresh_lock("librbd::ImageCtx::refresh_lock"),
      lock("librbd::ImageCtx::lock"),
      cache_lock("librbd::ImageCtx::cache_lock"),
      old_format(true),
      order(0), size(0), features(0),	id(image_id), parent(NULL),
      object_cacher(NULL), writeback_handler(NULL), object_set(NULL)
  {
    md_ctx.dup(p);
    data_ctx.dup(p);

    string pname = string("librbd-") + id + string("-") +
      data_ctx.get_pool_name() + string("/") + name;
    if (snap) {
      snap_name = snap;
      pname += "@";
      pname += snap_name;
    }
    perf_start(pname);

    if (cct->_conf->rbd_cache) {
      Mutex::Locker l(cache_lock);
      ldout(cct, 20) << "enabling writeback caching..." << dendl;
      writeback_handler = new LibrbdWriteback(data_ctx, cache_lock);
      object_cacher = new ObjectCacher(cct, pname, *writeback_handler, cache_lock,
				       NULL, NULL,
				       cct->_conf->rbd_cache_size,
				       cct->_conf->rbd_cache_max_dirty,
				       cct->_conf->rbd_cache_target_dirty,
				       cct->_conf->rbd_cache_max_dirty_age);
      object_set = new ObjectCacher::ObjectSet(NULL, data_ctx.get_id(), 0);
      object_cacher->start();
    }
  }

  ImageCtx::~ImageCtx() {
    perf_stop();
    if (object_cacher) {
      delete object_cacher;
      object_cacher = NULL;
    }
    if (writeback_handler) {
      delete writeback_handler;
      writeback_handler = NULL;
    }
    if (object_set) {
      delete object_set;
      object_set = NULL;
    }
  }

  int ImageCtx::init() {
    int r;
    if (id.length()) {
      old_format = false;
    } else {
      r = detect_format(md_ctx, name, &old_format, NULL);
      if (r < 0) {
	lderr(cct) << "error finding header: " << cpp_strerror(r) << dendl;
	return r;
      }
    }

    if (!old_format) {
      if (!id.length()) {
	r = cls_client::get_id(&md_ctx, id_obj_name(name), &id);
	if (r < 0) {
	  lderr(cct) << "error reading image id: " << cpp_strerror(r)
		     << dendl;
	  return r;
	}
      }

      header_oid = header_name(id);
      r = cls_client::get_immutable_metadata(&md_ctx, header_oid,
					     &object_prefix, &order);
      if (r < 0) {
	lderr(cct) << "error reading immutable metadata: "
		   << cpp_strerror(r) << dendl;
	return r;
      }
    } else {
      header_oid = old_header_name(name);
    }

    return 0;
  }

  void ImageCtx::perf_start(string name) {
    PerfCountersBuilder plb(cct, name, l_librbd_first, l_librbd_last);

    plb.add_u64_counter(l_librbd_rd, "rd");
    plb.add_u64_counter(l_librbd_rd_bytes, "rd_bytes");
    plb.add_fl_avg(l_librbd_rd_latency, "rd_latency");
    plb.add_u64_counter(l_librbd_wr, "wr");
    plb.add_u64_counter(l_librbd_wr_bytes, "wr_bytes");
    plb.add_fl_avg(l_librbd_wr_latency, "wr_latency");
    plb.add_u64_counter(l_librbd_discard, "discard");
    plb.add_u64_counter(l_librbd_discard_bytes, "discard_bytes");
    plb.add_fl_avg(l_librbd_discard_latency, "discard_latency");
    plb.add_u64_counter(l_librbd_flush, "flush");
    plb.add_u64_counter(l_librbd_aio_rd, "aio_rd");
    plb.add_u64_counter(l_librbd_aio_rd_bytes, "aio_rd_bytes");
    plb.add_fl_avg(l_librbd_aio_rd_latency, "aio_rd_latency");
    plb.add_u64_counter(l_librbd_aio_wr, "aio_wr");
    plb.add_u64_counter(l_librbd_aio_wr_bytes, "aio_wr_bytes");
    plb.add_fl_avg(l_librbd_aio_wr_latency, "aio_wr_latency");
    plb.add_u64_counter(l_librbd_aio_discard, "aio_discard");
    plb.add_u64_counter(l_librbd_aio_discard_bytes, "aio_discard_bytes");
    plb.add_fl_avg(l_librbd_aio_discard_latency, "aio_discard_latency");
    plb.add_u64_counter(l_librbd_snap_create, "snap_create");
    plb.add_u64_counter(l_librbd_snap_remove, "snap_remove");
    plb.add_u64_counter(l_librbd_snap_rollback, "snap_rollback");
    plb.add_u64_counter(l_librbd_notify, "notify");
    plb.add_u64_counter(l_librbd_resize, "resize");

    perfcounter = plb.create_perf_counters();
    cct->get_perfcounters_collection()->add(perfcounter);
  }

  void ImageCtx::perf_stop() {
    assert(perfcounter);
    cct->get_perfcounters_collection()->remove(perfcounter);
    delete perfcounter;
  }

  int ImageCtx::snap_set(string in_snap_name)
  {
    map<string, SnapInfo>::iterator it = snaps_by_name.find(in_snap_name);
    if (it != snaps_by_name.end()) {
      snap_name = in_snap_name;
      snap_id = it->second.id;
      snap_exists = true;
      data_ctx.snap_set_read(snap_id);
      return 0;
    }
    return -ENOENT;
  }

  void ImageCtx::snap_unset()
  {
    snap_id = CEPH_NOSNAP;
    snap_name = "";
    snap_exists = true;
    data_ctx.snap_set_read(snap_id);
  }

  snap_t ImageCtx::get_snap_id(string snap_name) const
  {
    map<string, SnapInfo>::const_iterator it = snaps_by_name.find(snap_name);
    if (it != snaps_by_name.end())
      return it->second.id;
    return CEPH_NOSNAP;
  }

  int ImageCtx::get_snap_name(snapid_t snap_id, string *snap_name) const
  {
    map<string, SnapInfo>::const_iterator it;

    for (it = snaps_by_name.begin(); it != snaps_by_name.end(); it++) {
      if (it->second.id == snap_id) {
	*snap_name = it->first;
	return 0;
      }
    }
    return -ENOENT;
  }

  int ImageCtx::get_snap_size(string snap_name, uint64_t *size) const
  {
    map<string, SnapInfo>::const_iterator it = snaps_by_name.find(snap_name);
    if (it != snaps_by_name.end()) {
      *size = it->second.size;
      return 0;
    }
    return -ENOENT;
  }

  void ImageCtx::add_snap(string snap_name, snap_t id, uint64_t size, uint64_t features,
		cls_client::parent_info parent)
  {
    snaps.push_back(id);
    SnapInfo info(id, size, features, parent);
    snaps_by_name.insert(pair<string, SnapInfo>(snap_name, info));
  }

  uint64_t ImageCtx::get_image_size() const
  {
    if (snap_name.length() == 0) {
      return size;
    } else {
      map<string, SnapInfo>::const_iterator p = snaps_by_name.find(snap_name);
      if (p == snaps_by_name.end())
	return 0;
      return p->second.size;
    }
  }

  int ImageCtx::get_features(uint64_t *out_features) const
  {
    if (snap_name.length() == 0) {
      *out_features = features;
      return 0;
    }
    map<string, SnapInfo>::const_iterator p = snaps_by_name.find(snap_name);
    if (p == snaps_by_name.end())
      return -ENOENT;
    *out_features = p->second.features;
    return 0;
  }

  int64_t ImageCtx::get_parent_pool_id() const
  {
    if (snap_name.length() == 0) {
      return parent_md.pool_id;
    }
    map<string, SnapInfo>::const_iterator p = snaps_by_name.find(snap_name);
    if (p == snaps_by_name.end())
      return -1;
    return p->second.parent.pool_id;
  }

  string ImageCtx::get_parent_image_id() const
  {
    if (snap_name.length() == 0) {
      return parent_md.image_id;
    }
    map<string, SnapInfo>::const_iterator p = snaps_by_name.find(snap_name);
    if (p == snaps_by_name.end())
      return "";
    return p->second.parent.image_id;
  }

  uint64_t ImageCtx::get_parent_snap_id() const
  {
    if (snap_name.length() == 0) {
      return parent_md.snap_id;
    }
    map<string, SnapInfo>::const_iterator p = snaps_by_name.find(snap_name);
    if (p == snaps_by_name.end())
      return CEPH_NOSNAP;
    return p->second.parent.snap_id;
  }

  int ImageCtx::get_parent_overlap(uint64_t *overlap) const
  {
    if (snap_name.length() == 0) {
      *overlap = parent_md.overlap;
      return 0;
    }
    map<string, SnapInfo>::const_iterator p = snaps_by_name.find(snap_name);
    if (p == snaps_by_name.end())
      return -ENOENT;
    *overlap = p->second.parent.overlap;
    return 0;
  }

  void ImageCtx::aio_read_from_cache(object_t o, bufferlist *bl, size_t len,
				     uint64_t off, Context *onfinish) {
    lock.Lock();
    ObjectCacher::OSDRead *rd = object_cacher->prepare_read(snap_id, bl, 0);
    lock.Unlock();
    ObjectExtent extent(o, off, len);
    extent.oloc.pool = data_ctx.get_id();
    extent.buffer_extents[0] = len;
    rd->extents.push_back(extent);
    cache_lock.Lock();
    int r = object_cacher->readx(rd, object_set, onfinish);
    cache_lock.Unlock();
    if (r > 0)
      onfinish->complete(r);
  }

  void ImageCtx::write_to_cache(object_t o, bufferlist& bl, size_t len,
				uint64_t off) {
    lock.Lock();
    ObjectCacher::OSDWrite *wr = object_cacher->prepare_write(snapc, bl,
							      utime_t(), 0);
    lock.Unlock();
    ObjectExtent extent(o, off, len);
    extent.oloc.pool = data_ctx.get_id();
    extent.buffer_extents[0] = len;
    wr->extents.push_back(extent);
    {
      Mutex::Locker l(cache_lock);
      object_cacher->writex(wr, object_set, cache_lock);
    }
  }

  int ImageCtx::read_from_cache(object_t o, bufferlist *bl, size_t len,
				uint64_t off) {
    int r;
    Mutex mylock("librbd::ImageCtx::read_from_cache");
    Cond cond;
    bool done;
    Context *onfinish = new C_SafeCond(&mylock, &cond, &done, &r);
    aio_read_from_cache(o, bl, len, off, onfinish);
    mylock.Lock();
    while (!done)
      cond.Wait(mylock);
    mylock.Unlock();
    return r;
  }

  int ImageCtx::flush_cache() {
    int r = 0;
    Mutex mylock("librbd::ImageCtx::flush_cache");
    Cond cond;
    bool done;
    Context *onfinish = new C_SafeCond(&mylock, &cond, &done, &r);
    cache_lock.Lock();
    bool already_flushed = object_cacher->commit_set(object_set, onfinish);
    cache_lock.Unlock();
    if (!already_flushed) {
      mylock.Lock();
      while (!done) {
	ldout(cct, 20) << "waiting for cache to be flushed" << dendl;
	cond.Wait(mylock);
      }
      mylock.Unlock();
      ldout(cct, 20) << "finished flushing cache" << dendl;
    }
    return r;
  }

  void ImageCtx::shutdown_cache() {
    lock.Lock();
    invalidate_cache();
    lock.Unlock();
    object_cacher->stop();
  }

  void ImageCtx::invalidate_cache() {
    assert(lock.is_locked());
    if (!object_cacher)
      return;
    cache_lock.Lock();
    object_cacher->release_set(object_set);
    cache_lock.Unlock();
    int r = flush_cache();
    if (r)
      lderr(cct) << "flush_cache returned " << r << dendl;
    cache_lock.Lock();
    bool unclean = object_cacher->release_set(object_set);
    cache_lock.Unlock();
    if (unclean)
      lderr(cct) << "could not release all objects from cache" << dendl;
  }

  int ImageCtx::register_watch() {
    assert(!wctx);
    wctx = new WatchCtx(this);
    return md_ctx.watch(header_oid, 0, &(wctx->cookie), wctx);
  }

  void ImageCtx::unregister_watch() {
    assert(wctx);
    lock.Lock();
    wctx->invalidate();
    md_ctx.unwatch(header_oid, wctx->cookie);
    lock.Unlock();
    delete wctx;
    wctx = NULL;
  }
}
