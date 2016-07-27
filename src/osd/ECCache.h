// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#ifndef CEPH_OSD_ECCACHE_H
#define CEPH_OSD_ECCACHE_H

#include <map>
#include <memory>
#include "include/types.h"

namespace ECCache {

struct Extent {
  uint64_t version; /// version of the CacheObject when this was inserted
  bufferlist data; /// continous buffer of data
};

class Object {
public:
  Object() {}
  ~Object() {}

  /// removes all cached extents
  void clear();

  /// returns the version of the data written, so it can be
  /// referenced for removal later.
  uint64_t write(uint64_t offset, bufferlist &bl);

  /// remove all buffers with the given version in the given range
  void remove(uint64_t offset, uint64_t length, uint64_t version);

  /// read any cached extents in the given range into the bufferlist
  /// at their position relative to the given offset.
  /// The bufferlist must be large enough to hold the whole extent.
  void read(uint64_t offset, uint64_t length, bufferlist &out);
protected:
  map<uint64_t, unique_ptr<Extent> > m_extents; /// offset -> extent
  uint64_t m_version;

  /// return an iterator to the first extent that includes or
  /// follows an offset
  map<uint64_t, unique_ptr<Extent> >::iterator extent_lower_bound(uint64_t offset);
};

} // namespace ECCache

#endif
