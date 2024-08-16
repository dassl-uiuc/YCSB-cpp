//
// YCSB-cpp
// lazylog_db.h
//

#ifndef YCSB_C_LAZYLOG_DB_H_
#define YCSB_C_LAYZLOG_DB_H_

#include "core/db.h"

#include <src/utils/properties.h>
#include <src/client/lazylog_cli.h>
// #include <src/client/lazylog_scalable_cli.h>

namespace ycsbc {

class LazylogDB : public DB {
 public:
  LazylogDB();
  void Init() override;
  void Cleanup() override;

  Status Read(const std::string &table, const std::string &key, const std::vector<std::string> *fields,
              std::vector<Field> &result) {}

  Status Scan(const std::string &table, const std::string &key, int len, const std::vector<std::string> *fields,
              std::vector<std::vector<Field>> &result) {
    throw "Scan: function not implemented!";
  }

  Status Update(const std::string &table, const std::string &key, std::vector<Field> &values) {}

  Status Insert(const std::string &table, const std::string &key, std::vector<Field> &values) override;

  Status Delete(const std::string &table, const std::string &key) { throw "Delete: function not implemented!"; }

  Status ReadIdx(const uint64_t idx, std::string &data) override;
 
 private:
  static void SerializeRow(const std::vector<Field> &values, std::string &data);
 protected:
  std::shared_ptr<lazylog::LazyLogClient> lzlog_;
};

}  // namespace ycsbc

#endif
