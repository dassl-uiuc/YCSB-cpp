#include "lazylog_db.h"

#include "core/db_factory.h"
#include "utils/utils.h"

namespace ycsbc {

std::atomic<int> client_id = 0;

LazylogDB::LazylogDB() { lzlog_ = std::make_shared<lazylog::LazyLogClient>(); }

void LazylogDB::Init() {
  lazylog::Properties props;
  props.FromMap(props_->ToMap());
  props.SetProperty("dur_log.client_id", std::to_string(client_id.fetch_add(1)));

  lzlog_->Initialize(props);
}

void LazylogDB::Cleanup() {
  lzlog_->Finalize();
}

DB::Status LazylogDB::Insert(const std::string &table, const std::string &_key, std::vector<Field> &values) {
  std::string data;
  SerializeRow(values, data);

  auto reqid = lzlog_->AppendEntryAll(data);
  if (reqid.first == 0)
    return Status::kOK;
  else
    return Status::kOK;
}

DB::Status LazylogDB::ReadIdx(const uint64_t idx, std::string &data) {
  bool ret = lzlog_->ReadEntry(idx, data);

  if (ret)
    return Status::kOK;
  else
    return Status::kError;
}

void LazylogDB::SerializeRow(const std::vector<Field> &values, std::string &data) {
  for (const Field &field : values) {
    uint32_t len = field.first.size();
    data.append(reinterpret_cast<char *>(&len), sizeof(uint32_t));
    data.append(field.first.data(), field.first.size());
    len = field.second.size();
    data.append(reinterpret_cast<char *>(&len), sizeof(uint32_t));
    data.append(field.second.data(), field.second.size());
  }
}

const bool registered = DBFactory::RegisterDB("lazylog", []() { return static_cast<DB *>(new LazylogDB); });

}  // namespace ycsbc
