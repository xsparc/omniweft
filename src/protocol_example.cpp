// SPDX-License-Identifier: Apache-2.0
#include "protocol_example.hpp"
#include "omniweft/commands.hpp"
#include <nlohmann/json.hpp>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
using Json = nlohmann::json;
namespace commands = ow::commands;
constexpr std::string_view usage =
    "omniweft_examples --example protocol.reject_invalid --headless --seed 7 "
    "[--verify] --output <new-artifact-directory> [--input <json-file>]...\n";
struct Options {
  bool headless = false;
  bool verify = false;
  std::filesystem::path output;
  std::vector<std::filesystem::path> inputs;
};
Options options_from(int argc, char* argv[]) {
  Options options;
  bool example = false, seed = false, output = false;
  for (int index = 1; index < argc; ++index) {
    const std::string_view arg(argv[index]);
    const auto value = [&]() -> std::string_view {
      if (index + 1 >= argc || std::string_view(argv[index + 1]).starts_with("--"))
        throw std::invalid_argument("missing value for " + std::string(arg));
      return argv[++index];
    };
    if (arg == "--example" && !example) {
      example = true;
      if (value() != "protocol.reject_invalid")
        throw std::invalid_argument("expected --example protocol.reject_invalid");
    } else if (arg == "--headless" && !options.headless) {
      options.headless = true;
    } else if (arg == "--verify" && !options.verify) {
      options.verify = true;
    } else if (arg == "--seed" && !seed) {
      seed = true;
      const auto token = value();
      std::uint32_t number = 0;
      const auto conversion = std::from_chars(token.data(), token.data() + token.size(), number);
      if (conversion.ec != std::errc{} || conversion.ptr != token.data() + token.size() || number != 7)
        throw std::invalid_argument("protocol.reject_invalid defines one fixture; use --seed 7");
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

constexpr std::string_view fixture = R"json({
  "protocol_version":"0.1",
  "world_id":"workshop",
  "transaction_id":"018f7242-4387-7c98-a114-67787915a369",
  "idempotency":{"epoch":"server-issued-session-epoch","sequence":8},
  "expected_world_revision":42,
  "apply_at":{"mode":"next_tick","expires_after_ticks":120},
  "budget":{"max_operations":2,"max_blob_bytes":0},
  "operations":[
    {"type":"entity.create","temporary_id":"block","prefab":"builtin.unit_cube"},
    {"type":"transform.set","target":{"temporary_id":"block"},"position_m":[0.0,2.0,0.0],
     "rotation_xyzw":[0.0,0.0,0.0,1.0],"scale":[1.0,1.0,1.0]}
  ]
})json";

}  // namespace

int run_protocol_example(int argc, char* argv[]) {
  Options options;
  try {
    options = options_from(argc, argv);
  } catch (const std::exception& error) {
    std::cerr << "invalid_arguments: " << error.what() << '\n' << usage;
    return 2;
  }
  if (!options.headless) {
    std::cerr << "graphics_unavailable: this schema-validation example is headless; rerun with --headless.\n";
    return 3;
  }
  Json results = Json::array();
  const bool builtin = options.inputs.empty();
  const std::size_t count = builtin ? 3 : options.inputs.size();
  try {
    for (std::size_t index = 0; index < count; ++index) {
      std::string bytes;
      if (builtin) {
        bytes = std::string(fixture);
        if (index == 1) bytes.replace(bytes.find("\"0.1\""), 5, "\"9.9\"");
      } else {
        try {
          bytes = read_bounded(options.inputs[index]);
        } catch (const std::exception& error) {
          std::cerr << "input_error: " << error.what() << "; provide a readable JSON fixture file\n";
          return 7;
        }
      }
      const auto parsed = commands::parse(bytes);
      Json item = {{"index", index}, {"status", parsed.envelope ? "schema_valid" : "schema_invalid"}, {"errors", Json::array()}};
      for (const auto& error : parsed.errors)
        item["errors"].push_back({{"code", error.code}, {"path", error.path}, {"message", error.message}});
      if (parsed.envelope) {
        const auto serialized = commands::serialize(*parsed.envelope);
        if (!serialized.json) throw std::runtime_error("a parsed envelope failed typed serialization");
        item["serialized"] = *serialized.json;
        item["round_trip"] = Json::parse(*serialized.json);
        if (options.verify) {
          const auto round_trip = commands::parse(*serialized.json);
          if (!round_trip.envelope || *round_trip.envelope != *parsed.envelope)
            throw std::runtime_error("typed round-trip changed envelope fields");
          const auto repeated = commands::serialize(*round_trip.envelope);
          if (!repeated.json || *repeated.json != *serialized.json)
            throw std::runtime_error("second serialization changed bytes");
        }
      }
      if (options.verify && builtin) {
        if (parsed.envelope.has_value() != (index != 1))
          throw std::runtime_error("built-in valid/invalid/valid outcomes differ from the fixture");
        if (index == 1 && (parsed.errors.size() != 1 || parsed.errors[0].code != "UNSUPPORTED_VERSION"))
          throw std::runtime_error("built-in invalid version did not return UNSUPPORTED_VERSION");
      }
      results.push_back(std::move(item));
    }
  } catch (const std::exception& error) {
    std::cerr << "verification_failed: " << error.what() << '\n';
    return 4;
  }
  const Json report = {{"schema_version", 1}, {"example", "protocol.reject_invalid"},
    {"protocol_version", "0.1"}, {"seed", 7}, {"verified", options.verify}, {"results", std::move(results)}};
  try {
    write_report(options.output, report);
  } catch (const std::exception& error) {
    std::cerr << "output_error: " << error.what() << "; use a writable, fresh --output directory\n";
    return 5;
  }
  std::cout << "protocol.reject_invalid: schema-validation results written; no commands executed\n";
  return 0;
}
