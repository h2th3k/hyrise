#pragma once

#include <thread>

#include "abstract_features.hpp"
#include "all_type_variant.hpp"
#include "storage/encoding_type.hpp"

namespace opossum {
namespace cost_model {

// Hard-coded to test server
struct ConstantHardwareFeatures : AbstractFeatures {
  size_t l1_size_kb = 0;
  size_t l1_block_size_kb = 0;
  size_t l2_size_kb = 0;
  size_t l2_block_size_kb = 0;
  size_t l3_size_kb = 0;
  size_t l3_block_size_kb = 0;

  size_t memory_size_kb = 0;
  size_t memory_access_bandwidth = 0;
  size_t memory_access_latency = 0;
  size_t cpu_core_count = std::thread::hardware_concurrency();
  size_t cpu_clock_speed_mhz = 0;
  size_t numa_node_count = 0;

  size_t cpu_architecture = 0;  // Should be ENUM

  const std::map<std::string, AllTypeVariant> serialize() const override;
  const std::unordered_map<std::string, float> to_cost_model_features() const override;
};

}  // namespace cost_model
}  // namespace opossum