#include <ATen/core/dispatch/Dispatcher.h>
#include <caffe2/serialize/inline_container.h>
#include <torch/csrc/jit/mobile/runtime_compatibility.h>
#include <torch/csrc/jit/mobile/type_parser.h>
#include <torch/csrc/jit/runtime/operator.h>
#include <torch/custom_class.h>
#include <unordered_map>

namespace c10 {
TypePtr parseType(const std::string& pythonStr);
} // namespace c10

namespace torch {
namespace jit {
const std::unordered_map<std::string, c10::TypePtr>& string_to_type_lut();

uint64_t _get_runtime_bytecode_version() {
  return caffe2::serialize::kMaxSupportedBytecodeVersion;
}

/*
 * Returns all registered PyTorch ops and their versioning
 */
std::unordered_map<std::string, OperatorInfo> _get_runtime_ops_and_info() {
  std::unordered_map<std::string, OperatorInfo> result;

  // Grab the jit operators
  auto nonDispatcherOperators = torch::jit::getAllOperators();
  for (const auto& full_op : nonDispatcherOperators) {
    auto op = full_op->schema();
    int num_schema_args = op.arguments().size();
    auto op_name = op.name();
    if (!op.overload_name().empty()) {
      op_name += ("." + op.overload_name());
    }
    result.emplace(op_name, OperatorInfo{num_schema_args});
  }

  // Grab the dispatcher operators
  auto dispatcherOperators = c10::Dispatcher::singleton().getAllOpNames();
  for (auto& op : dispatcherOperators) {
    // grab schema
    const auto op_handle = c10::Dispatcher::singleton().findOp(op);
    c10::optional<int> num_schema_args;
    if (op_handle->hasSchema()) {
      num_schema_args = op_handle->schema().arguments().size();
    }
    auto op_name = op.name;
    if (!op.overload_name.empty()) {
      op_name += ("." + op.overload_name);
    }
    result.emplace(op_name, OperatorInfo{num_schema_args});
  }

  return result;
}

RuntimeCompatibilityInfo RuntimeCompatibilityInfo::get() {
  return RuntimeCompatibilityInfo{
      _get_runtime_bytecode_version(),
      _get_runtime_ops_and_info(),
      _get_mobile_supported_types()};
}

std::unordered_set<std::string> _get_mobile_supported_types() {
  std::unordered_set<std::string> supported_types;
  for (const auto& it : string_to_type_lut()) {
    supported_types.insert(it.first);
  }
  supported_types.insert(
      at::TypeParser::getNonSimpleType().begin(),
      at::TypeParser::getNonSimpleType().end());
  supported_types.insert(
      at::TypeParser::getCustomType().begin(),
      at::TypeParser::getCustomType().end());

  return supported_types;
}

TORCH_API std::unordered_set<std::string> _get_loaded_custom_classes() {
  return torch::getAllCustomClassesNames();
}

} // namespace jit
} // namespace torch
