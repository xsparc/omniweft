// SPDX-License-Identifier: Apache-2.0
// Independent typed-API assertions from the documented schema 0.1 sample.
#include <omniweft/commands.hpp>

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <variant>

namespace {
constexpr const char* valid_sample = R"json({
  "protocol_version":"0.1",
  "world_id":"workshop",
  "transaction_id":"018f7242-4387-7c98-a114-67787915a369",
  "idempotency":{"epoch":"server-issued-session-epoch","sequence":8},
  "expected_world_revision":42,
  "apply_at":{"mode":"next_tick","expires_after_ticks":120},
  "budget":{"max_operations":2,"max_blob_bytes":0},
  "operations":[
    {"type":"entity.create","temporary_id":"block","prefab":"builtin.unit_cube"},
    {"type":"transform.set","target":{"temporary_id":"block"},
     "position_m":[0.0,2.0,0.0],"rotation_xyzw":[0.0,0.0,0.0,1.0],"scale":[1.0,1.0,1.0]}
  ]
})json";

unsigned checks = 0;
void require(bool condition, const std::string& message) {
    ++checks;
    if (!condition) {
        throw std::runtime_error(message);
    }
}
}

int main() {
    using namespace ow::commands;
    try {
        const auto original = parse(valid_sample);
        require(original.envelope.has_value() && original.errors.empty(), "documented sample must parse");
        require(original.envelope->operations.size() == 2, "sample must preserve both operations");
        const auto& transform = std::get<TransformSet>(original.envelope->operations[1]);
        require(transform.position_m == std::array<double, 3>{0.0, 2.0, 0.0}, "typed position must match literal");
        require(transform.rotation_xyzw == std::array<double, 4>{0.0, 0.0, 0.0, 1.0}, "typed rotation must match literal");
        require(transform.scale == std::array<double, 3>{1.0, 1.0, 1.0}, "typed scale must match literal");

        const std::array<double, 3> invalid_values{
            std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(),
            -std::numeric_limits<double>::infinity()
        };
        const std::array<std::string, 3> fields{"position_m", "rotation_xyzw", "scale"};
        for (std::size_t field = 0; field < fields.size(); ++field) {
            const std::size_t count = field == 1 ? 4U : 3U;
            for (std::size_t component = 0; component < count; ++component) {
                for (const double value : invalid_values) {
                    auto candidate = *original.envelope;
                    auto& changed = std::get<TransformSet>(candidate.operations[1]);
                    if (field == 0) {
                        changed.position_m[component] = value;
                    } else if (field == 1) {
                        changed.rotation_xyzw[component] = value;
                    } else {
                        changed.scale[component] = value;
                    }
                    const auto rejected = serialize(candidate);
                    const auto path = "/operations/1/" + fields[field] + "/" + std::to_string(component);
                    require(!rejected.json.has_value(), "nonfinite typed value must not produce JSON: " + path);
                    require(rejected.errors.size() == 1, "nonfinite typed value must produce one error: " + path);
                    require(rejected.errors[0].code == "NONFINITE_VALUE", "typed nonfinite error code: " + path);
                    require(rejected.errors[0].path == path, "typed nonfinite error path: " + path);
                    require(!rejected.errors[0].message.empty(), "typed nonfinite error must explain rejection: " + path);
                    const auto recovered = serialize(*original.envelope);
                    require(recovered.json.has_value() && recovered.errors.empty(), "valid typed serialization after rejection");
                    const auto reparsed = parse(*recovered.json);
                    require(reparsed.envelope.has_value() && reparsed.errors.empty(), "serialized valid envelope must parse");
                    require(*reparsed.envelope == *original.envelope, "typed parse/serialize equality after rejection");
                }
            }
        }
        std::string unsupported(valid_sample);
        const auto version = unsupported.find("\"0.1\"");
        require(version != std::string::npos, "test fixture version marker");
        unsupported.replace(version, 5, "\"9.9\"");
        const auto rejected_parse = parse(unsupported);
        require(!rejected_parse.envelope.has_value() && rejected_parse.errors.size() == 1,
                "unsupported version has no typed envelope");
        require(rejected_parse.errors[0].code == "UNSUPPORTED_VERSION", "unsupported version code");
        const auto recovered_parse = parse(valid_sample);
        require(recovered_parse.envelope.has_value() && recovered_parse.errors.empty(),
                "valid parse recovers in same process");
        require(*recovered_parse.envelope == *original.envelope, "parse recovery preserves exact typed values");
        std::cout << "{\"status\":\"passed\",\"typed_nonfinite_cases\":30,\"assertions\":" << checks << "}\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "protocol_native_test failed: " << error.what() << '\n';
        return 1;
    }
}
