#ifndef YCSB_C_ROCKSDB_SVR_H_
#define YCSB_C_ROCKSDB_SVR_H_

#include "common.h"
#include "rpc.h"

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/filter_policy.h>
#include <rocksdb/utilities/options_util.h>

#include <mutex>

void read_handler(erpc::ReqHandle *req_handle, void *context);
void put_handler(erpc::ReqHandle *req_handle, void *context);
void delete_handler(erpc::ReqHandle *req_handle, void *context);
void scan_handler(erpc::ReqHandle *req_handle, void *context);

class ServerContext {
 public:
  erpc::Rpc<erpc::CTransport> *rpc_;
  erpc::MsgBuffer resp_buf_;
  static rocksdb::DB *db_;
  static bool sync;
  int session_num_;
  int thread_id_;
};

#endif
