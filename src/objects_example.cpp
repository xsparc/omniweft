// SPDX-License-Identifier: Apache-2.0
#include "objects_example.hpp"
#include "omniweft/transactions.hpp"
#include <nlohmann/json.hpp>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using Json = nlohmann::json;
namespace commands = ow::commands;
namespace transactions = ow::transactions;
namespace world = ow::world;
constexpr std::string_view usage =
    "omniweft_examples --example objects.atomic --headless --seed 7 "
    "[--verify] --output <new-artifact-directory> [--max-slots <1..1024>] "
    "[--input <json-file>]...\n";
struct Options {
  bool hierarchy = false;
  bool headless = false;
  bool verify = false;
  std::uint32_t max_slots = 1024;
  std::filesystem::path output;
  std::vector<std::filesystem::path> inputs;
};
std::uint32_t number(std::string_view token) {
  std::uint32_t result = 0;
  const auto conversion = std::from_chars(token.data(), token.data() + token.size(), result);
  if (conversion.ec != std::errc{} || conversion.ptr != token.data() + token.size())
    throw std::invalid_argument("expected an unsigned 32-bit decimal integer");
  return result;
}
Options options_from(int argc, char* argv[]) {
  Options options;
  bool example = false, seed = false, output = false, slots = false;
  for (int index = 1; index < argc; ++index) {
    const std::string_view arg(argv[index]);
    const auto value = [&]() -> std::string_view {
      if (index + 1 >= argc || std::string_view(argv[index + 1]).starts_with("--"))
        throw std::invalid_argument("missing value for " + std::string(arg));
      return argv[++index];
    };
    if (arg == "--example" && !example) {
      example = true;
      const auto selected = value();
      options.hierarchy = selected == "objects.hierarchy";
      if (selected != "objects.atomic" && !options.hierarchy)
        throw std::invalid_argument("expected objects.atomic or objects.hierarchy");
    } else if (arg == "--headless" && !options.headless) {
      options.headless = true;
    } else if (arg == "--verify" && !options.verify) {
      options.verify = true;
    } else if (arg == "--seed" && !seed) {
      seed = true;
      if (number(value()) != 7)
        throw std::invalid_argument("objects.atomic defines one fixture seed; use --seed 7");
    } else if (arg == "--max-slots" && !slots) {
      slots = true;
      options.max_slots = number(value());
      if (options.max_slots == 0 || options.max_slots > 1024)
        throw std::invalid_argument("--max-slots must be from 1 through 1024");
    } else if (arg == "--output" && !output) {
      output = true;
      options.output = std::filesystem::path(value());
      if (options.output.empty()) throw std::invalid_argument("--output must name a fresh directory");
    } else if (arg == "--input") {
      const auto input = value();
      if (input.empty()) throw std::invalid_argument("--input must name a JSON file");
      if (options.inputs.size() >= 64) throw std::invalid_argument("at most 64 --input files are allowed");
      options.inputs.emplace_back(input);
    } else {
      throw std::invalid_argument("unknown or duplicate option: " + std::string(arg));
    }
  }
  if (!example || !seed || !output)
    throw std::invalid_argument("required options: --example, --seed and --output");
  return options;
}
std::string read_bounded(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) throw std::runtime_error("cannot open --input file");
  std::string bytes(commands::max_envelope_bytes + 1, '\0');
  stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  if (stream.bad()) throw std::runtime_error("failed to read --input file");
  bytes.resize(static_cast<std::size_t>(stream.gcount()));
  return bytes;
}
void write_report(const std::filesystem::path& directory, const Json& report) {
  if (!directory.parent_path().empty())
    std::filesystem::create_directories(directory.parent_path());
  if (!std::filesystem::create_directory(directory))
    throw std::runtime_error("output directory already exists; choose a fresh --output directory");
  const auto temporary = directory / "result.json.tmp";
  try {
    std::ofstream stream(temporary, std::ios::binary);
    stream.exceptions(std::ios::badbit | std::ios::failbit);
    stream << report.dump(2) << '\n';
    stream.close();
    std::filesystem::rename(temporary, directory / "result.json");
  } catch (...) {
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    throw;
  }
}
std::string hexadecimal(const std::vector<std::uint8_t>& bytes) {
  constexpr char digits[] = "0123456789abcdef";
  std::string result;
  result.reserve(bytes.size() * 2);
  for (const auto byte : bytes) {
    result.push_back(digits[byte >> 4U]);
    result.push_back(digits[byte & 15U]);
  }
  return result;
}
Json snapshot_json(const world::Snapshot& snapshot) {
  Json slots = Json::array();
  for (const auto& slot : snapshot.slots) {
    Json entity = nullptr;
    if (slot.entity) {
      const auto& value = *slot.entity;
      entity = {{"prefab", value.prefab}, {"authoring_revision", value.authoring_revision},
        {"transform", {{"position_m", value.transform.position_m},
          {"rotation_xyzw", value.transform.rotation_xyzw}, {"scale", value.transform.scale}}}};
      if (snapshot.format_version == 2) {
        entity["parent"] = nullptr;
        entity["local_transform"] = nullptr;
        if (value.parent) {
          entity["parent"] = {{"entity_uuid", value.parent->entity_uuid}, {"generation", value.parent->generation}};
          entity["local_transform"] = {{"position_m", value.local_transform.position_m},
            {"rotation_xyzw", value.local_transform.rotation_xyzw}, {"scale", value.local_transform.scale}};
        }
      }
    }
    slots.push_back({{"entity_uuid", slot.entity_uuid}, {"generation", slot.generation},
      {"retired", slot.retired}, {"entity", std::move(entity)}});
  }
  return {{"format_version", snapshot.format_version}, {"world_id", snapshot.world_id},
    {"seed", snapshot.seed}, {"max_slots", snapshot.max_slots},
    {"world_revision", snapshot.world_revision}, {"slots", std::move(slots)}};
}
Json receipt_json(const transactions::Receipt& receipt) {
  Json errors = Json::array(), created = Json::array();
  for (const auto& error : receipt.errors) {
    Json item = {{"code", error.code}, {"path", error.path}, {"message", error.message}};
    if (error.operation_index) item["operation_index"] = *error.operation_index;
    errors.push_back(std::move(item));
  }
  for (const auto& binding : receipt.created)
    created.push_back({{"temporary_id", binding.temporary_id}, {"world_id", binding.world_id},
      {"entity_uuid", binding.entity_uuid}, {"generation", binding.generation}});
  return {{"status", receipt.status}, {"durability", receipt.durability},
    {"transaction_id", receipt.transaction_id}, {"world_revision", receipt.world_revision},
    {"created", std::move(created)}, {"errors", std::move(errors)}};
}
transactions::Receipt parse_rejection(const commands::ParseResult& parsed, std::uint64_t revision) {
  transactions::Receipt receipt;
  receipt.world_revision = revision;
  for (const auto& source : parsed.errors) {
    transactions::Error error{source.code, source.path, source.message, std::nullopt};
    constexpr std::string_view prefix = "/operations/";
    if (std::string_view(source.path).starts_with(prefix)) {
      const auto suffix = std::string_view(source.path).substr(prefix.size());
      std::size_t index = 0;
      const auto result = std::from_chars(suffix.data(), suffix.data() + suffix.size(), index);
      if (result.ec == std::errc{} && result.ptr != suffix.data() &&
          (result.ptr == suffix.data() + suffix.size() || *result.ptr == '/'))
        error.operation_index = index;
    }
    receipt.errors.push_back(std::move(error));
  }
  return receipt;
}
commands::Envelope builtin_envelope(std::size_t index) {
  commands::Envelope envelope;
  envelope.world_id = "workshop";
  envelope.transaction_id = "018f7242-4387-7c98-a114-67787915a30" + std::to_string(index + 1);
  envelope.idempotency = {"fixture-epoch", static_cast<std::uint64_t>(index) + 1};
  envelope.expected_world_revision = index == 0 ? 0 : 1;
  envelope.apply_at = {"next_tick", 120};
  const commands::EntityTarget a{"workshop", "00000007-0000-4000-8000-000000000001", 1};
  const commands::EntityTarget b{"workshop", "00000007-0000-4000-8000-000000000002", 1};
  if (index == 0) {
    envelope.operations = {
      commands::EntityCreate{"A", "builtin.unit_cube"},
      commands::TransformSet{commands::TemporaryTarget{"A"}, {1, 2, 3}, {0, 0, 0, 1}, {1, 2, 1}},
      commands::EntityCreate{"B", "builtin.unit_cube"},
      commands::TransformSet{commands::TemporaryTarget{"B"}, {-2, 0, 4}, {0, 0, 1, 0}, {-1, 1, 1}}};
  } else if (index == 1) {
    envelope.operations = {
      commands::TransformSet{a, {99, 98, 97}, {0, 0, 0, 1}, {1, 1, 1}},
      commands::EntityCreate{"C", "builtin.unit_cube"},
      commands::EntityDelete{b, "reject_if_children"},
      commands::TransformSet{commands::TemporaryTarget{"missing"}, {0, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}}};
  } else {
    envelope.operations = {commands::EntityCreate{"C", "builtin.unit_cube"}};
  }
  envelope.budget = {static_cast<std::uint64_t>(envelope.operations.size()), 0};
  return envelope;
}
commands::Envelope hierarchy_envelope(std::size_t index) {
  auto envelope = builtin_envelope(0);
  envelope.transaction_id.back() = static_cast<char>('1' + index);
  envelope.idempotency.sequence = index + 1;
  constexpr std::uint64_t revisions[]{0,1,2,2,3};
  envelope.expected_world_revision = revisions[index];
  const commands::EntityTarget c{"workshop", "00000007-0000-4000-8000-000000000002", 1};
  const commands::EntityTarget q{"workshop", "00000007-0000-4000-8000-000000000003", 1};
  if (index == 0) envelope.operations = {
    commands::EntityCreate{"P", "builtin.unit_cube"},
    commands::TransformSet{commands::TemporaryTarget{"P"}, {10,0,0}, {0,0,1,0}, {2,2,2}},
    commands::EntityCreate{"C", "builtin.unit_cube"},
    commands::TransformSet{commands::TemporaryTarget{"C"}, {1,0,0}},
    commands::EntityReparent{commands::TemporaryTarget{"C"}, commands::TemporaryTarget{"P"}, "preserve_local"}};
  else if (index == 1) envelope.operations = {
    commands::EntityCreate{"Q", "builtin.unit_cube"},
    commands::TransformSet{commands::TemporaryTarget{"Q"}, {-4,0,0}},
    commands::EntityReparent{c, commands::TemporaryTarget{"Q"}, "preserve_world"}};
  else if (index == 2) envelope.operations = {commands::EntityReparent{q, c, "preserve_local"}};
  else if (index == 3) envelope.operations = {commands::TransformSet{q, {-2,0,0}}};
  else envelope.operations = {commands::EntityReparent{c, std::nullopt, "preserve_local"}};
  envelope.budget.max_operations = envelope.operations.size();
  return envelope;
}
bool hierarchy_verified(std::size_t index, const transactions::Receipt& receipt) {
  constexpr std::uint64_t revisions[]{1,2,2,3,4};
  return receipt.world_revision == revisions[index] &&
    (index == 2 ? receipt.status == "rejected" && receipt.errors.size() == 1 &&
      receipt.errors[0].code == "INVALID_SCHEMA" && receipt.created.empty() :
      receipt.status == "committed" && receipt.errors.empty());
}
bool builtin_verified(std::size_t index, const transactions::Receipt& receipt) {
  if (index != 1)
    return receipt.status == "committed" && receipt.errors.empty() &&
      receipt.world_revision == (index == 0 ? 1U : 2U) && receipt.created.size() == (index == 0 ? 2U : 1U);
  return receipt.status == "rejected" && receipt.created.empty() && receipt.world_revision == 1 &&
    receipt.errors.size() == 1 && receipt.errors[0].code == "NOT_FOUND" &&
    receipt.errors[0].path == "/operations/3/target/temporary_id" && receipt.errors[0].operation_index == 3;
}
}  // namespace

int run_objects_example(int argc, char* argv[]) {
  Options options;
  try {
    options = options_from(argc, argv);
  } catch (const std::exception& error) {
    std::cerr << "invalid_arguments: " << error.what() << '\n' << usage;
    return 2;
  }
  if (!options.headless) {
    std::cerr << "graphics_unavailable: this authoring example is headless; rerun with --headless.\n";
    return 3;
  }
  const bool builtin = options.inputs.empty();
  const std::size_t count = builtin ? (options.hierarchy ? 5U : 3U) : options.inputs.size();
  Json results = Json::array();
  try {
    world::World target_world("workshop", 7, options.max_slots);
    for (std::size_t index = 0; index < count; ++index) {
      const auto before = target_world.snapshot();
      const auto before_bytes = target_world.canonical_bytes();
      transactions::Receipt receipt;
      if (builtin) {
        receipt = transactions::Coordinator::apply_at_boundary(target_world, options.hierarchy ? hierarchy_envelope(index) : builtin_envelope(index));
      } else {
        std::string bytes;
        try {
          bytes = read_bounded(options.inputs[index]);
        } catch (const std::exception& error) {
          std::cerr << "input_error: " << error.what() << "; provide a readable JSON fixture file\n";
          return 7;
        }
        const auto parsed = commands::parse(bytes);
        receipt = parsed.envelope
          ? transactions::Coordinator::apply_at_boundary(target_world, *parsed.envelope)
          : parse_rejection(parsed, before.world_revision);
      }
      const auto after = target_world.snapshot();
      const auto after_bytes = target_world.canonical_bytes();
      if (options.verify && ((builtin && !(options.hierarchy ? hierarchy_verified(index, receipt) : builtin_verified(index, receipt))) ||
          after_bytes != target_world.canonical_bytes())) {
        std::cerr << "verification_failed: fixture receipts or repeated canonical encoding differ\n";
        return 4;
      }
      results.push_back({{"index", index}, {"receipt", receipt_json(receipt)},
        {"before", snapshot_json(before)}, {"after", snapshot_json(after)},
        {"before_canonical_hex", hexadecimal(before_bytes)}, {"after_canonical_hex", hexadecimal(after_bytes)}});
    }
  } catch (const std::exception& error) {
    std::cerr << "runtime_error: " << error.what() << '\n';
    return 6;
  }
  const Json report = {{"schema_version", 1}, {"example", options.hierarchy ? "objects.hierarchy" : "objects.atomic"}, {"seed", 7},
    {"verified", options.verify}, {"results", std::move(results)}};
  try {
    write_report(options.output, report);
  } catch (const std::exception& error) {
    std::cerr << "output_error: " << error.what() << "; use a writable, fresh --output directory\n";
    return 5;
  }
  std::cout << (options.hierarchy ? "objects.hierarchy" : "objects.atomic") << ": volatile authoring receipts and complete state written\n";
  return 0;
}
