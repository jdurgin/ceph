// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#include "ECCache.h"

namespace ECCache {

void Object::clear()
{
  m_extents.clear();
}

uint64_t Object::write(uint64_t offset, bufferlist &bl)
{
  unique_ptr<Extent> to_write(new Extent{++m_version, bl});

  uint64_t cur = offset;
  uint64_t length = bl.length();
  auto it = extent_lower_bound(offset);
  while (it != m_extents.end()) {
    uint64_t start = it->first;
    uint64_t end = it->first + it->second->data.length();
    if (start > offset + length) {
      break;
    }

    unique_ptr<Extent> split_left, split_right;
    if (start < cur) {
      // overlap on the left
      bufferlist left_data;
      left_data.substr_of(it->second->data, 0, cur - start);
      split_left.reset(new Extent{it->second->version, left_data});
    } else if (end <= offset + length) {
      // complete overwrite
      m_extents.erase(it++);
      cur = end;
      continue;
    }
    uint64_t right_start = end - (offset + length);
    if (offset + length < end) {
      // overlap on the right
      bufferlist right_data;
      right_data.substr_of(it->second->data, right_start, end - right_start);
      split_right.reset(new Extent{it->second->version, right_data});
    }
    m_extents.erase(it++);
    cur = end;
    if (split_left) {
      m_extents.insert(std::make_pair(start, std::move(split_left)));
    }
    if (split_right) {
      m_extents.insert(std::make_pair(right_start, std::move(split_right)));
    }
  }

  m_extents.insert(std::make_pair(offset, std::move(to_write)));
  return m_version;
}

void Object::remove(uint64_t offset, uint64_t length, uint64_t version)
{
  assert(version <= m_version);
  // extents are never merged, so there's no need to check for the
  // offset in the middle of an extent
  for (auto it = m_extents.lower_bound(offset); it != m_extents.end();) {
    uint64_t extent_version = it->second->version;
    if (it->first > offset + length) {
      assert(extent_version != version);
      return;
    }
    if (extent_version == version) {
      m_extents.erase(it++);
    } else {
      // overlapping section must have been written later
      assert(extent_version > version);
      ++it;
    }
  }
}

void Object::read(uint64_t offset, uint64_t length, bufferlist &out)
{
  assert(out.length() >= length);
  for (auto it = extent_lower_bound(offset); it != m_extents.end(); ++it) {
    if (it->first > offset + length) {
      return;
    }
    uint64_t copy_len = MIN(length - it->first, it->second->data.length());
    uint64_t extent_off = offset > it->first ? offset - it->first : 0;
    out.copy_in(it->first + extent_off - offset, copy_len, it->second->data.c_str() + extent_off);
  }
}

map<uint64_t, unique_ptr<Extent> >::iterator Object::extent_lower_bound(uint64_t offset)
{
  auto it = m_extents.lower_bound(offset);
  if (it != m_extents.begin() &&
      (it == m_extents.end() || it->first > offset)) {
    --it;
    if (it->first + it->second->data.length() <= offset) {
      ++it;
    }
  }
  return it;
}

Cache::Cache() {}
Cache::~Cache() {}

uint64_t write(const hobject_t &hoid, uint64_t offset, bufferlist &bl) {
  return m_objects[hoid].write(offset, bl);
}

void remove(const hobject_t &hoid, uint64_t offset, uint64_t length, uint64_t version) {
  m_objects[hoid].remove(offset, length, version);
}

void read(const hobject_t &hoid, uint64_t offset, uint64_t length, bufferlist &out) {
  m_objects[hoid].read(offset, length, out);
}
} // namespace ECCache
