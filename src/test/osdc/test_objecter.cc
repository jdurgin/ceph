// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#include "gtest/gtest.h"

#include "common/ceph_argparse.h"
#include "common/config.h"
#include "global/global_init.h"
#include "librados/RadosClient.h"
#include "mon/MonClient.h"
#include "mgr/MgrClient.h"
#include "msg/Dispatcher.h"
#include "osdc/Objecter.h"
#include "test/librados/test.h"

class ObjecterTest : public ::testing::Test {
public:
  ObjecterTest() {}
  ~ObjecterTest() override {}
protected:
  static int mon_command(const std::string &cmd) {
    vector<string> cmds;
    cmds.push_back(cmd);
    bufferlist inbl;
    return rados->mon_command(cmds, inbl, nullptr, nullptr);
  }

  static void delete_profile_and_ruleset() {
    ASSERT_EQ(0, mon_command("{\"prefix\": \"osd erasure-code-profile rm\", \"name\": \"testprofile-" + pool_name + "\"}"));
    ASSERT_EQ(0, mon_command("{\"prefix\": \"osd crush rule rm\", \"name\":\"" + pool_name + "\"}"));
  }

  static void SetUpTestCase() {
    rados = new librados::RadosClient(g_ceph_context);
    ASSERT_EQ(0, rados->connect());
    pool_name = get_temp_pool_name();
    delete_profile_and_ruleset();
    ASSERT_EQ(0, mon_command("{\"prefix\": \"osd erasure-code-profile set\", \"name\": \"testprofile-" + pool_name + "\", \"profile\": [ \"k=2\", \"m=2\", \"crush-failure-domain=osd\"]}"));
    ASSERT_EQ(0, mon_command("{\"prefix\": \"osd pool create\", \"pool\": \"" + pool_name + "\", \"pool_type\":\"erasure\", \"pg_num\":8, \"pgp_num\":8, \"erasure_code_profile\":\"testprofile-" + pool_name + "\"}"));
    ASSERT_EQ(0, mon_command("{\"prefix\": \"osd pool set\", \"pool\":\"" + pool_name + "\", \"var\": \"allow_ec_overwrites\", \"val\": \"true\"}"));
    rados->wait_for_latest_osdmap();
    objecter = rados->get_objecter();
  }

  static void TearDownTestCase() {
    ASSERT_EQ(0, rados->pool_delete(pool_name.c_str()));
    delete_profile_and_ruleset();
    rados->shutdown();
  }

  static string pool_name;
  static librados::RadosClient *rados;
  static Objecter *objecter;
};

string ObjecterTest::pool_name;
librados::RadosClient *ObjecterTest::rados = nullptr;
Objecter *ObjecterTest::objecter = nullptr;

TEST_F(ObjecterTest, ECtrimtrunc) {
  C_SaferCond on_write, on_trunc, on_read, on_zero, on_trunc2, on_read2;
  bufferlist write_bl;
  string oid = "foo";
  object_locator_t oloc(rados->lookup_pool(pool_name.c_str()));
  SnapContext snapc;

  // write non-stripe-aligned
  write_bl.append("abcdefghijklmnopqrstuvwxyz");
  objecter->write(oid, oloc, 182990, write_bl.length(), snapc, write_bl,
		  ceph::real_clock::now(), 0, &on_write);
  ASSERT_EQ(0, on_write.wait());

  // trimtrunc so there's still a partial stripe
  vector<OSDOp> ops(1);
  ops[0].op.op = CEPH_OSD_OP_TRIMTRUNC;
  ops[0].op.extent.truncate_seq = 10;
  ops[0].op.extent.truncate_size = 182996;
  objecter->_modify(oid, oloc, ops, ceph::real_clock::now(), snapc,
		    0, &on_trunc);
  ASSERT_EQ(0, on_trunc.wait());

  // read back all data
  bufferlist outbl;
  objecter->read(oid, oloc, 182990, 4096, CEPH_NOSNAP, &outbl, 0, &on_read);
  EXPECT_EQ(0, on_read.wait());
  EXPECT_EQ(6, outbl.length());
  ASSERT_EQ(0, memcmp("abcdef", outbl.c_str(), 6));

  // zero beyond end
  objecter->zero(oid, oloc, 552406, 26467, snapc, ceph::real_clock::now(), 0, &on_zero);
  ASSERT_EQ(0, on_zero.wait());

  // truncate larger than exists
  vector<OSDOp> ops2(1);
  ops2[0].op.op = CEPH_OSD_OP_TRIMTRUNC;
  ops2[0].op.extent.truncate_seq = 11;
  ops2[0].op.extent.truncate_size = 207239;
  objecter->_modify(oid, oloc, ops2, ceph::real_clock::now(), snapc,
		    0, &on_trunc2);
  ASSERT_EQ(0, on_trunc2.wait());

  // read again, this time from the space that logically exists due to
  // the trimtrunc but is not on-disk
  bufferlist outbl2;
  objecter->read(oid, oloc, 184320, 24576, CEPH_NOSNAP, &outbl2, 0, &on_read2);
  EXPECT_EQ(0, on_read2.wait());
  EXPECT_EQ(1657, outbl2.length());
  ASSERT_EQ(0, outbl2.c_str()[0]);
}

int main(int argc, char **argv) {
  vector<const char*> args;
  argv_to_vec(argc, (const char **)argv, args);

  auto cct = global_init(NULL, args, CEPH_ENTITY_TYPE_CLIENT,
			 CODE_ENVIRONMENT_UTILITY, 0);
  common_init_finish(cct.get());

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
