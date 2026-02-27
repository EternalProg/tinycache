#include <benchmark/benchmark.h>
#include <boost/asio/streambuf.hpp>
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>
#include <utility>
#include <vector>
#include "respParser.hpp"
#include "respSerializer.hpp"
#include "respValue.hpp"

using tinycache::ParsingResult;
using tinycache::RespParser;
using tinycache::RespSerializer;
using tinycache::RespValue;

namespace {

RespValue make_bulk(std::string value) {
  return RespValue{RespValue::Type::kBulkString, std::move(value)};
}

RespValue make_array(std::vector<RespValue> elements) {
  return RespValue{RespValue::Type::kArray, std::move(elements)};
}

RespValue build_simple_string(std::string value) {
  return RespValue{RespValue::Type::kSimpleString, std::move(value)};
}

RespValue build_error_string(std::string value) {
  return RespValue{RespValue::Type::kError, std::move(value)};
}

RespValue build_integer(std::int64_t value) {
  return RespValue{RespValue::Type::kInteger, value};
}

RespValue build_null_bulk() {
  RespValue value;
  value.type = RespValue::Type::kNullBulkString;
  value.data = std::string();
  return value;
}

RespValue build_bulk_string(std::size_t size) {
  return make_bulk(std::string(size, 'x'));
}

RespValue build_empty_array() {
  return make_array({});
}

RespValue build_mixed_array() {
  std::vector<RespValue> elements;
  elements.reserve(4);
  elements.push_back(build_simple_string("OK"));
  elements.push_back(build_integer(42));
  elements.push_back(make_bulk("payload"));
  elements.push_back(build_null_bulk());
  return make_array(std::move(elements));
}

RespValue build_nested_array() {
  std::vector<RespValue> elements;
  elements.reserve(3);
  elements.push_back(build_mixed_array());

  std::vector<RespValue> inner;
  inner.reserve(2);
  inner.push_back(build_integer(-7));
  inner.push_back(make_bulk("nested"));
  elements.push_back(make_array(std::move(inner)));

  elements.push_back(build_empty_array());
  return make_array(std::move(elements));
}

std::string serialize_value(const RespValue& value) {
  return RespSerializer::serialize(value);
}

void run_parse_bench(benchmark::State& state, const std::string& payload) {
  boost::asio::streambuf buffer;
  RespValue parsed;

  for (auto _ : state) {
    (void)_;
    state.PauseTiming();
    buffer.consume(buffer.size());
    std::ostream os(&buffer);
    os.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    state.ResumeTiming();

    auto result = RespParser::parse(buffer, parsed);
    if (result != ParsingResult::kReady) {
      state.SkipWithError("RESP parse failed");
      break;
    }
    benchmark::DoNotOptimize(parsed);
  }

  state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) *
                          static_cast<std::int64_t>(payload.size()));
}

void run_serialize_bench(benchmark::State& state, const RespValue& value) {
  const std::string sample = RespSerializer::serialize(value);
  const auto bytes = static_cast<std::int64_t>(sample.size());

  for (auto _ : state) {
    (void)_;
    auto encoded = RespSerializer::serialize(value);
    benchmark::DoNotOptimize(encoded);
  }

  state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) *
                          bytes);
}

void BM_Parse_SimpleString(benchmark::State& state) {
  const std::string payload = serialize_value(build_simple_string("OK"));
  run_parse_bench(state, payload);
}

void BM_Parse_ErrorString(benchmark::State& state) {
  const std::string payload = serialize_value(build_error_string("ERR parse"));
  run_parse_bench(state, payload);
}

void BM_Parse_Integer(benchmark::State& state) {
  const std::string payload = serialize_value(build_integer(123456));
  run_parse_bench(state, payload);
}

void BM_Parse_BulkString(benchmark::State& state) {
  const std::string payload = serialize_value(build_bulk_string(256));
  run_parse_bench(state, payload);
}

void BM_Parse_LargeBulk(benchmark::State& state) {
  const std::string payload = serialize_value(build_bulk_string(256 * 1024));
  run_parse_bench(state, payload);
}

void BM_Parse_MixedArray(benchmark::State& state) {
  const std::string payload = serialize_value(build_mixed_array());
  run_parse_bench(state, payload);
}

void BM_Parse_NestedArray(benchmark::State& state) {
  const std::string payload = serialize_value(build_nested_array());
  run_parse_bench(state, payload);
}

void BM_Serialize_SimpleString(benchmark::State& state) {
  const RespValue value = build_simple_string("OK");
  run_serialize_bench(state, value);
}

void BM_Serialize_ErrorString(benchmark::State& state) {
  const RespValue value = build_error_string("ERR parse");
  run_serialize_bench(state, value);
}

void BM_Serialize_Integer(benchmark::State& state) {
  const RespValue value = build_integer(123456);
  run_serialize_bench(state, value);
}

void BM_Serialize_BulkString(benchmark::State& state) {
  const RespValue value = build_bulk_string(256);
  run_serialize_bench(state, value);
}

void BM_Serialize_LargeBulk(benchmark::State& state) {
  const RespValue value = build_bulk_string(256 * 1024);
  run_serialize_bench(state, value);
}

void BM_Serialize_MixedArray(benchmark::State& state) {
  const RespValue value = build_mixed_array();
  run_serialize_bench(state, value);
}

void BM_Serialize_NestedArray(benchmark::State& state) {
  const RespValue value = build_nested_array();
  run_serialize_bench(state, value);
}

}  // namespace

BENCHMARK(BM_Parse_SimpleString)->MinTime(0.1)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_Parse_ErrorString)->MinTime(0.1)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_Parse_Integer)->MinTime(0.1)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_Parse_BulkString)->MinTime(0.1)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_Parse_LargeBulk)->MinTime(0.1)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_Parse_MixedArray)->MinTime(0.1)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_Parse_NestedArray)->MinTime(0.1)->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_Serialize_SimpleString)
    ->MinTime(0.1)
    ->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_Serialize_ErrorString)
    ->MinTime(0.1)
    ->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_Serialize_Integer)->MinTime(0.1)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_Serialize_BulkString)->MinTime(0.1)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_Serialize_LargeBulk)->MinTime(0.1)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_Serialize_MixedArray)->MinTime(0.1)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_Serialize_NestedArray)
    ->MinTime(0.1)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN();
