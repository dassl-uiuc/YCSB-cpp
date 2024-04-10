#ifndef YCSB_C_KAFKA_DB_H_
#define YCSB_C_KAFKA_DB_H_

#include <atomic>

#include "core/db.h"
#include "cppkafka/cppkafka.h"

namespace ycsbc {

class KafkaDB : public DB {
 public:
  KafkaDB();
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

  Status ReadIdx(const uint64_t idx, std::string &data){};

 protected:
  cppkafka::Producer *producer_;
  std::string topic_;
  int shard_num_;
  static std::atomic<int> global_id_cnt_;
  int prod_id_;

 private:
  static void SerializeRow(const std::vector<Field> &values, std::string &data);
  bool delivered_;
};

}  // namespace ycsbc

#endif
