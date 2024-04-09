#include "lazylog_db.h"

#include "core/db_factory.h"
#include "utils/utils.h"

namespace ycsbc {

LazylogDB::LazylogDB() { lzlog_ = std::make_shared<lazylog::LazyLogScalableClient>(); }

void LazylogDB::Init() {
  lazylog::Properties props;
  props.FromMap(props_->ToMap());

  lzlog_->Initialize(props);
}

void LazylogDB::CleanUp() {
  lzlog_->Finalize();
}

DB::Status LazylogDB::Insert(const std::string &table, const std::string &_key, std::vector<Field> &values) {
  std::string data;
  SerializeRow(values, data);

  auto reqid = lzlog_->AppendEntryAll(data);
  if (reqid.first == 0)
    return Status::kError;
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
