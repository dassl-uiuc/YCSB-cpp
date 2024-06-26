#include "sqlite_cli.h"

#include <string.h>

#include <iostream>
#include <mutex>
#include <string>

#include "common.h"
#include "core/db_factory.h"

#define DEBUG 0

namespace ycsbc {

std::mutex lk_;
std::atomic<uint8_t> SQLiteDB::global_rpc_id_ = 0;
erpc::Nexus *SQLiteDB::nexus_ = nullptr;

void cli_sm_handler(int, erpc::SmEventType, erpc::SmErrType, void *) {}

void rpc_cont_func(void *context, void *_tag) { reinterpret_cast<SQLiteDB *>(context)->notifyRpcComplete(); }

void SQLiteDB::Init() {
  std::lock_guard<std::mutex> guard(lk_);
  const utils::Properties &props = *props_;
  const std::string &client_hostname = props.GetProperty(PROP_CLI_HOSTNAME, PROP_CLI_HOSTNAME_DEFAULT);
  const std::string &client_udpport = props.GetProperty(PROP_UDP_PORT_CLI, PROP_UDP_PORT_CLI_DEFAULT);
  const std::string client_uri = client_hostname + ":" + client_udpport;

  const std::string &server_hostname = props.GetProperty(PROP_SVR_HOSTNAME, PROP_SVR_HOSTNAME_DEFAULT);
  const std::string &server_udpport = props.GetProperty(PROP_UDP_PORT_SVR, PROP_UDP_PORT_SVR_DEFAULT);
  const std::string server_uri = server_hostname + ":" + server_udpport;

  const uint8_t phy_port = std::stoi(props.GetProperty(PROP_PHY_PORT, PROP_PHY_PORT_DEFAULT));

  uint8_t rpc_id = global_rpc_id_.fetch_add(1);

  if (!nexus_) {
    nexus_ = new erpc::Nexus(client_uri);
  }

  rpc_ = new erpc::Rpc<erpc::CTransport>(nexus_, this, rpc_id, cli_sm_handler, phy_port);

  session_num_ = rpc_->create_session(server_uri, rpc_id);

  while (!rpc_->is_connected(session_num_)) {
    rpc_->run_event_loop_once();
  }
  std::cout << "eRPC client " << (int)rpc_id << " connected to " << server_uri << std::endl;

  const int msg_size = std::stoull(props.GetProperty(PROP_MSG_SIZE, PROP_MSG_SIZE_DEFAULT));
  req_ = rpc_->alloc_msg_buffer_or_die(msg_size);
  resp_ = rpc_->alloc_msg_buffer_or_die(msg_size);
}

void SQLiteDB::Cleanup() {
  rpc_->free_msg_buffer(req_);
  rpc_->free_msg_buffer(resp_);
  delete rpc_;
  if (global_rpc_id_.fetch_sub(1) == 1) {  // todo: this assumes Cleanup() and Init() on different threads will never overlap
    delete nexus_;  // at this moment, all rpc objects must have been freed
    nexus_ = nullptr;
  }
}

DB::Status SQLiteDB::Read(const std::string &table, const std::string &key, const std::vector<std::string> *fields,
                            std::vector<Field> &result) {
  size_t k_size = SerializeKey(key, reinterpret_cast<char *>(req_.buf_));
  rpc_->resize_msg_buffer(&req_, k_size);
  rpc_->enqueue_request(session_num_, SQL_READ_REQ, &req_, &resp_, rpc_cont_func, nullptr);
  pollForRpcComplete();

  if (resp_.get_data_size() < sizeof(DB::Status)) return DB::kError;
  DB::Status s = *reinterpret_cast<DB::Status *>(resp_.buf_);
  if (s != DB::kOK) return s;
  size_t v_size = resp_.get_data_size() - sizeof(DB::Status);
  const char *v_base = reinterpret_cast<const char *>(resp_.buf_ + sizeof(DB::Status));
  DeserializeRow(result, v_base, v_base + v_size);
#if DEBUG
  std::ostringstream vstream;
  for (auto &v : result) vstream << "f: " << v.first << " v: " << v.second << std::endl;
  std::cout << "[READ] key: " << key << " value: " << vstream.str();
#endif
  return DB::kOK;
}

DB::Status SQLiteDB::Scan(const std::string &table, const std::string &key, int len,
                        const std::vector<std::string> *fields, std::vector<std::vector<Field>> &result) {
  size_t k_size = SerializeKey(key, reinterpret_cast<char *>(req_.buf_));
  *reinterpret_cast<int *>(req_.buf_ + k_size) = len;
  rpc_->resize_msg_buffer(&req_, k_size + sizeof(int));
  rpc_->enqueue_request(session_num_, SQL_SCAN_REQ, &req_, &resp_, rpc_cont_func, nullptr);
  pollForRpcComplete();

  if (resp_.get_data_size() < sizeof(DB::Status)) return DB::kError;
  DB::Status s = *reinterpret_cast<DB::Status *>(resp_.buf_);
  if (s != DB::kOK) return s;
  size_t v_size = resp_.get_data_size() - sizeof(DB::Status);
  const char *v_base = reinterpret_cast<const char *>(resp_.buf_ + sizeof(DB::Status));
  DeserializeRowVector(result, v_base, v_base + v_size);
#if DEBUG
  std::ostringstream vstream;
  for (auto &values : result) {
    vstream << "values:\n";
    for (auto &v: values) vstream << "\tf: " << v.first << " v: " << v.second << std::endl;
  }
  std::cout << "[SCAN] len: " << len << " results: \n" << vstream.str();
#endif
  return DB::kOK;
}

DB::Status SQLiteDB::Update(const std::string &table, const std::string &key, std::vector<Field> &values) {
  size_t k_size = SerializeKey(key, reinterpret_cast<char *>(req_.buf_));
  size_t v_size = SerializeRow(values, reinterpret_cast<char *>(req_.buf_ + k_size));
#if DEBUG
  std::ostringstream vstream;
  for (auto &f : values) vstream << "f: " << f.first << " v: " << f.second << std::endl;
  std::cout << "[UPDATE] key: " << key << " value:\n" << vstream.str();
#endif

  rpc_->resize_msg_buffer(&req_, k_size + v_size);
  rpc_->enqueue_request(session_num_, SQL_UPDATE_REQ, &req_, &resp_, rpc_cont_func, nullptr);
  pollForRpcComplete();
  assert(resp_.get_data_size() == sizeof(DB::Status));
  return *reinterpret_cast<DB::Status *>(resp_.buf_);
}

DB::Status SQLiteDB::Insert(const std::string &table, const std::string &key, std::vector<Field> &values) {
  size_t k_size = SerializeKey(key, reinterpret_cast<char *>(req_.buf_));
  size_t v_size = SerializeRow(values, reinterpret_cast<char *>(req_.buf_ + k_size));
#if DEBUG
  std::ostringstream vstream;
  for (auto &f : values) vstream << "f: " << f.first << " v: " << f.second << std::endl;
  std::cout << "[INSERT] key: " << key << " value:\n" << vstream.str();
#endif

  rpc_->resize_msg_buffer(&req_, k_size + v_size);
  rpc_->enqueue_request(session_num_, SQL_INSERT_REQ, &req_, &resp_, rpc_cont_func, nullptr);
  pollForRpcComplete();
  assert(resp_.get_data_size() == sizeof(DB::Status));
  return *reinterpret_cast<DB::Status *>(resp_.buf_);
}

DB::Status SQLiteDB::Delete(const std::string &table, const std::string &key) {
  size_t k_size = SerializeKey(key, reinterpret_cast<char *>(req_.buf_));
  rpc_->resize_msg_buffer(&req_, k_size);
  rpc_->enqueue_request(session_num_, SQL_DELETE_REQ, &req_, &resp_, rpc_cont_func, nullptr);
  pollForRpcComplete();
  assert(resp_.get_data_size() == sizeof(DB::Status));
  return *reinterpret_cast<DB::Status *>(resp_.buf_);
}

void SQLiteDB::pollForRpcComplete() {
  while (!complete_)
    rpc_->run_event_loop_once();
  complete_ = false;
}

void SQLiteDB::notifyRpcComplete() { complete_ = true; }

size_t SQLiteDB::SerializeKey(const std::string &key, char *data) {
  uint32_t len = key.size();
  memcpy(data, reinterpret_cast<char *>(&len), sizeof(uint32_t));
  memcpy(data + sizeof(uint32_t), key.data(), key.size());
  return sizeof(uint32_t) + key.size();
}

size_t SQLiteDB::SerializeRow(const std::vector<Field> &values, char *data) {
  size_t offset = 0;
  for (const Field &field : values) {
    uint32_t len = field.first.size();
    memcpy(data + offset, reinterpret_cast<char *>(&len), sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(data + offset, field.first.data(), field.first.size());
    offset += field.first.size();
    len = field.second.size();
    memcpy(data + offset, reinterpret_cast<char *>(&len), sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(data + offset, field.second.data(), field.second.size());
    offset += field.second.size();
  }
  return offset;
}

void SQLiteDB::DeserializeRow(std::vector<Field> &values, const char *p, const char *lim) {
  while (p != lim) {
    assert(p < lim);
    uint32_t len = *reinterpret_cast<const uint32_t *>(p);
    p += sizeof(uint32_t);
    std::string field(p, static_cast<const size_t>(len));
    p += len;
    len = *reinterpret_cast<const uint32_t *>(p);
    p += sizeof(uint32_t);
    std::string value(p, static_cast<const size_t>(len));
    p += len;
    values.push_back({field, value});
  }
}

void SQLiteDB::DeserializeRow(std::vector<Field> &values, const std::string &data) {
  const char *p = data.data();
  const char *lim = p + data.size();
  DeserializeRow(values, p, lim);
}

void SQLiteDB::DeserializeRowVector(std::vector<std::vector<Field>> &results, const char *p, const char *lim) {
  while (p != lim) {
    assert(p < lim);
    uint32_t len = *reinterpret_cast<const uint32_t *>(p);
    p += sizeof(uint32_t);
    results.push_back(std::vector<Field>());
    std::vector<Field> &values = results.back();
    DeserializeRow(values, p, p + len);
    p += len;
  }
}

SQLiteDB::~SQLiteDB() {}

DB *NewSQLiteDB() { return new SQLiteDB; }

const bool registered = DBFactory::RegisterDB("sqlite", NewSQLiteDB);

}  // namespace ycsbc