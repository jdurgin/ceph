// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#include "osd/ECCache.h"
#include "common/ceph_argparse.h"
#include "global/global_context.h"
#include "global/global_init.h"
#include <gtest/gtest.h>
#include <set>

class ObjectTest : public ::testing::Test, protected ECCache::Object {
public:
  ObjectTest() {}
  virtual void SetUp() {}

  virtual void TearDown() {
    clear();
    m_version = 0;
  }

  void populate_simple() {
    bufferlist data;
    data.append("foo", 3);
    uint64_t version, last_version = 0;
    for (int i = 0; i < 30; i += 3) {
      version = write(i, data);
      EXPECT_LT(last_version, version);
      last_version = version;
      versions.insert(version);
      EXPECT_EQ(i / 3 + 1, m_extents.size());
    }
  }

  void remove_write(uint64_t version, size_t num_removed) {
    size_t start_size = m_extents.size();
    versions.erase(version);
    remove(0, UINT64_MAX, version);
    size_t end_size = m_extents.size();
    EXPECT_EQ(num_removed, start_size - end_size);
  }

  set<uint64_t> versions;
};

TEST_F(ObjectTest, write_simple) {
  populate_simple();
  clear();
  ASSERT_EQ(0u, m_extents.size());
}

TEST_F(ObjectTest, remove) {
  populate_simple();
  for (auto it = versions.begin(); it != versions.end();) {
    uint64_t v = *it;
    ++it;
    remove_write(v, 1);
  }
  ASSERT_EQ(0u, m_extents.size());
}

TEST_F(ObjectTest, read_simple) {
  populate_simple();
  bufferlist expected;
  bufferlist read_data;
  for (int i = 0; i < 10; ++i) {
    expected.append("foo");
    read_data.append("bar");
  }
  expected.append("bar");
  read_data.append("bar");
  read(0, read_data.length(), read_data);
  ASSERT_EQ(expected, read_data);
}

TEST_F(ObjectTest, overlapping_write) {
  bufferlist orig, new_data, expected, read_data;
  orig.append("foofoo");
  new_data.append("bar");
  expected.append("fobaro");
  read_data.append("aaaaaa");

  read(0, 6, read_data);
  ASSERT_EQ(string("aaaaaa"), string(read_data.c_str(), 6));

  uint64_t v1 = write(0, orig);
  read(0, 6, read_data);
  ASSERT_EQ(orig, read_data);

  // overlap in the middle
  uint64_t v2 = write(2, new_data);
  read(0, 6, read_data);
  ASSERT_EQ(expected, read_data);

  remove(0, 6, v1);
  read_data.copy_in(0, 6, "aaaaaa");
  expected.copy_in(0, 6, "aabara");
  read(0, 6, read_data);
  ASSERT_EQ(expected, read_data);

  remove(2, 3, v2);
  read_data.copy_in(0, 6, "aaaaaa");
  expected.copy_in(0, 6, "aaaaaa");
  read(0, 6, read_data);
  ASSERT_EQ(expected, read_data);

  write(0, orig);
  write(0, new_data);
  read(0, 6, read_data);
  expected.copy_in(0, 6, "barfoo");
  ASSERT_EQ(expected, read_data);

  // overwrite entire buffer
  write(0, orig);
  read(0, 6, read_data);
  ASSERT_EQ(orig, read_data);

  // overlap at the end
  write(3, new_data);
  read(0, 6, read_data);
  expected.copy_in(0, 6, "foobar");
  ASSERT_EQ(expected, read_data);
}

int main(int argc, char **argv) {
  vector<const char*> args;
  argv_to_vec(argc, (const char **)argv, args);

  global_init(NULL, args, CEPH_ENTITY_TYPE_CLIENT, CODE_ENVIRONMENT_UTILITY, 0);
  common_init_finish(g_ceph_context);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
