// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#ifndef MOSDRECOVERYDELETEREPLY_H
#define MOSDRECOVERYDELETEREPLY_H

#include "msg/Message.h"
#include "osd/osd_types.h"

struct MOSDPGRecoveryDeleteReply : public Message {
  static const int HEAD_VERSION = 2;
  static const int COMPAT_VERSION = 1;

  pg_shard_t from;
  spg_t pgid;
  epoch_t map_epoch;
  list<pair<hobject_t, eversion_t> > objects;

  bool partial_decode_needed, final_decode_needed;

  epoch_t get_map_epoch() const {
    return map_epoch;
  }
  epoch_t get_min_epoch() const {
    return map_epoch;
  }
  spg_t get_spg() const {
    return pgid;
  }

  MOSDPGRecoveryDeleteReply()
    : Message(MSG_OSD_PG_RECOVERY_DELETE_REPLY, HEAD_VERSION, COMPAT_VERSION),
      partial_decode_needed(true), final_decode_needed(true)
    {}

  void decode_payload() override {
    assert(partial_decode_needed && final_decode_needed);
    partial_decode_needed = false;
    if (header.version == 1) {
      // need feature bits from osdmap for final decode
      return;
    }
    decode_current();
  }

  void finish_decode(uint64_t features) {
    assert(!partial_decode_needed);
    assert(final_decode_needed);
    final_decode_needed = false;
    if (HAVE_FEATURE(features, SERVER_LUMINOUS)) {
      decode_current();
    } else {
      decode_orig();
    }
  }

  void decode_current() {
    epoch_t unused_min_epoch;
    bufferlist::iterator p = payload.begin();
    ::decode(pgid.pgid, p);
    ::decode(map_epoch, p);
    ::decode(unused_min_epoch, p);
    ::decode(objects, p);
    ::decode(pgid.shard, p);
    ::decode(from, p);
  }

  void decode_orig() {
    // original incompatible version without min_epoch
    bufferlist::iterator p = payload.begin();
    ::decode(pgid.pgid, p);
    ::decode(map_epoch, p);
    ::decode(objects, p);
    ::decode(pgid.shard, p);
    ::decode(from, p);
  }

  void encode_payload(uint64_t features) override {
    ::encode(pgid.pgid, payload);
    ::encode(map_epoch, payload);
    ::encode(map_epoch, payload); // min_epoch
    ::encode(objects, payload);
    ::encode(pgid.shard, payload);
    ::encode(from, payload);
  }

  void print(ostream& out) const override {
    out << "MOSDPGRecoveryDeleteReply(" << pgid
        << " e" << map_epoch << " " << objects << ")";
  }

  const char *get_type_name() const override { return "recovery_delete_reply"; }
};

#endif
