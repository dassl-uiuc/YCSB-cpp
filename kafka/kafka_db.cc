#include "kafka_db.h"
#include "core/db_factory.h"

namespace ycsbc {

std::atomic<int> KafkaDB::global_id_cnt_ = 0;

using namespace cppkafka;

KafkaDB::KafkaDB() : prod_id_(global_id_cnt_.fetch_add(1)) {}

void KafkaDB::Init() {
  utils::Properties &p = *props_;
  Configuration config = {
    {"bootstrap.servers", p.GetProperty("bootstrap.servers", "localhost:9092")},
    {"acks", "all"}
  };

  producer_ = new Producer(config);
  topic_ = p.GetProperty("kafka.topic", "default_topic");
  shard_num_ = std::stoi(p.GetProperty("shard.num", "1"));
}

void KafkaDB::Cleanup() {
  try {
    producer_->flush();
  } catch (const std::exception &e) {
    printf("Exception: %s\n", e.what());
  }
  if (producer_) delete producer_;
}

DB::Status KafkaDB::Insert(const std::string &table, const std::string &key, std::vector<Field> &values) {
  std::string data;
  SerializeRow(values, data);
  int shard_id = prod_id_ % shard_num_;
  
  producer_->produce(MessageBuilder(topic_).partition(shard_id).payload(data));
  producer_->poll(std::chrono::milliseconds(1));

  return DB::Status::kOK;
}

void KafkaDB::SerializeRow(const std::vector<Field> &values, std::string &data) {
  for (const Field &field : values) {
    uint32_t len = field.first.size();
    data.append(reinterpret_cast<char *>(&len), sizeof(uint32_t));
    data.append(field.first.data(), field.first.size());
    len = field.second.size();
    data.append(reinterpret_cast<char *>(&len), sizeof(uint32_t));
    data.append(field.second.data(), field.second.size());
  }
}

const bool registered = DBFactory::RegisterDB("kafka", []() { return static_cast<DB *>(new KafkaDB); });

}  // namespace ycsbc
