#ifndef YCSB_C_SEQUENTIAL_GENERATOR_H_
#define YCSB_C_SEQUENTIAL_GENERATOR_H_

#include "generator.h"

namespace ycsbc {

class SequentialGenerator : public Generator<uint64_t> {
 public:
  SequentialGenerator(uint64_t min, uint64_t max) : min_(min), max_(max) {}

  uint64_t Next() override;
  uint64_t Last() override { return 0; }

 private:
  static thread_local uint64_t counter_;

  const uint64_t min_;
  const uint64_t max_;
};

thread_local uint64_t SequentialGenerator::counter_ = 0;

inline uint64_t SequentialGenerator::Next() {
  if (counter_ == 0) counter_ = min_;
  uint64_t val = counter_;
  if (++counter_ >= max_) counter_ = min_;
  return val;
}

}  // namespace ycsbc

#endif
