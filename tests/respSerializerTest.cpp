#include <gtest/gtest.h>
#include <respSerializer.hpp>
#include "respValue.hpp"

using tinycache::RespSerializer;
using tinycache::RespValue;

class RespSerializerTest : public ::testing::Test {
 protected:
  static RespValue CreateSimpleString(const std::string& str) {
    RespValue val;
    val.type = RespValue::Type::kSimpleString;
    val.data = str;
    return val;
  }

  static RespValue CreateError(const std::string& str) {
    RespValue val;
    val.type = RespValue::Type::kError;
    val.data = str;
    return val;
  }

  static RespValue CreateInteger(std::int64_t num) {
    RespValue val;
    val.type = RespValue::Type::kInteger;
    val.data = num;
    return val;
  }

  static RespValue CreateBulkString(const std::string& str) {
    RespValue val;
    val.type = RespValue::Type::kBulkString;
    val.data = str;
    return val;
  }

  static RespValue CreateNullBulkString() {
    RespValue val;
    val.type = RespValue::Type::kNullBulkString;
    val.data = std::string();
    return val;
  }

  static RespValue CreateArray(const std::vector<RespValue>& elements) {
    RespValue val;
    val.type = RespValue::Type::kArray;
    val.data = elements;
    return val;
  }
};

// Simple String Tests
TEST_F(RespSerializerTest, SerializeSimpleString) {
  auto val = CreateSimpleString("OK");
  std::string result = RespSerializer::serialize(val);
  EXPECT_EQ(result, "+OK\r\n");
}

TEST_F(RespSerializerTest, SerializeSimpleStringWithSpaces) {
  auto val = CreateSimpleString("hello world");
  std::string result = RespSerializer::serialize(val);
  EXPECT_EQ(result, "+hello world\r\n");
}

TEST_F(RespSerializerTest, SerializeEmptySimpleString) {
  auto val = CreateSimpleString("");
  std::string result = RespSerializer::serialize(val);
  EXPECT_EQ(result, "+\r\n");
}

// Error Tests
TEST_F(RespSerializerTest, SerializeError) {
  auto val = CreateError("ERR unknown command");
  std::string result = RespSerializer::serialize(val);
  EXPECT_EQ(result, "-ERR unknown command\r\n");
}

TEST_F(RespSerializerTest, SerializeErrorWithLongMessage) {
  auto val = CreateError("ERR syntax error");
  std::string result = RespSerializer::serialize(val);
  EXPECT_EQ(result, "-ERR syntax error\r\n");
}

TEST_F(RespSerializerTest, SerializeEmptyError) {
  auto val = CreateError("");
  std::string result = RespSerializer::serialize(val);
  EXPECT_EQ(result, "-\r\n");
}

// Integer Tests
TEST_F(RespSerializerTest, SerializePositiveInteger) {
  auto val = CreateInteger(42);
  std::string result = RespSerializer::serialize(val);
  EXPECT_EQ(result, ":42\r\n");
}

TEST_F(RespSerializerTest, SerializeNegativeInteger) {
  auto val = CreateInteger(-1);
  std::string result = RespSerializer::serialize(val);
  EXPECT_EQ(result, ":-1\r\n");
}

TEST_F(RespSerializerTest, SerializeZero) {
  auto val = CreateInteger(0);
  std::string result = RespSerializer::serialize(val);
  EXPECT_EQ(result, ":0\r\n");
}

TEST_F(RespSerializerTest, SerializeLargePositiveInteger) {
  auto val = CreateInteger(9223372036854775807LL);  // int64_t max
  std::string result = RespSerializer::serialize(val);
  EXPECT_EQ(result, ":9223372036854775807\r\n");
}

TEST_F(RespSerializerTest, SerializeLargeNegativeInteger) {
  auto val = CreateInteger(-9223372036854775807LL - 1);  // int64_t min
  std::string result = RespSerializer::serialize(val);
  EXPECT_EQ(result, ":-9223372036854775808\r\n");
}

// Bulk String Tests
TEST_F(RespSerializerTest, SerializeBulkString) {
  auto val = CreateBulkString("hello");
  std::string result = RespSerializer::serialize(val);
  EXPECT_EQ(result, "$5\r\nhello\r\n");
}

TEST_F(RespSerializerTest, SerializeEmptyBulkString) {
  auto val = CreateBulkString("");
  std::string result = RespSerializer::serialize(val);
  EXPECT_EQ(result, "$0\r\n\r\n");
}

TEST_F(RespSerializerTest, SerializeBulkStringWithSpecialCharacters) {
  auto val = CreateBulkString("hello\r\nworld");
  std::string result = RespSerializer::serialize(val);
  // String "hello\r\nworld" has 12 chars (including CRLF as 2 chars)
  EXPECT_EQ(result, "$12\r\nhello\r\nworld\r\n");
}

TEST_F(RespSerializerTest, SerializeBulkStringWithBinaryData) {
  std::string binary_data;
  binary_data.push_back('\x00');
  binary_data.push_back('\x01');
  binary_data.push_back('\x02');
  binary_data.push_back('\x03');
  auto val = CreateBulkString(binary_data);
  std::string result = RespSerializer::serialize(val);
  // Length should be: "$4\r\n" (4 bytes) + 4 binary bytes + "\r\n" (2 bytes) = 10 bytes
  EXPECT_EQ(result.size(), 10);
  EXPECT_TRUE(result.find("$4\r\n") == 0);
  EXPECT_TRUE(result.substr(result.size() - 2) == "\r\n");
}

TEST_F(RespSerializerTest, SerializeLongBulkString) {
  std::string long_str(1000, 'a');
  auto val = CreateBulkString(long_str);
  std::string result = RespSerializer::serialize(val);
  EXPECT_EQ(result, "$1000\r\n" + long_str + "\r\n");
}

// Null Bulk String Tests
TEST_F(RespSerializerTest, SerializeNullBulkString) {
  auto val = CreateNullBulkString();
  std::string result = RespSerializer::serialize(val);
  EXPECT_EQ(result, "$-1\r\n");
}

// Array Tests
TEST_F(RespSerializerTest, SerializeEmptyArray) {
  auto val = CreateArray({});
  std::string result = RespSerializer::serialize(val);
  EXPECT_EQ(result, "*0\r\n");
}

TEST_F(RespSerializerTest, SerializeArrayOfBulkStrings) {
  std::vector<RespValue> elements = {
      CreateBulkString("hello"),
      CreateBulkString("world"),
  };
  auto val = CreateArray(elements);
  std::string result = RespSerializer::serialize(val);
  EXPECT_EQ(result, "*2\r\n$5\r\nhello\r\n$5\r\nworld\r\n");
}

TEST_F(RespSerializerTest, SerializeArrayOfSimpleStrings) {
  std::vector<RespValue> elements = {
      CreateSimpleString("OK"),
      CreateSimpleString("DONE"),
  };
  auto val = CreateArray(elements);
  std::string result = RespSerializer::serialize(val);
  EXPECT_EQ(result, "*2\r\n+OK\r\n+DONE\r\n");
}

TEST_F(RespSerializerTest, SerializeArrayOfIntegers) {
  std::vector<RespValue> elements = {
      CreateInteger(1),
      CreateInteger(2),
      CreateInteger(3),
  };
  auto val = CreateArray(elements);
  std::string result = RespSerializer::serialize(val);
  EXPECT_EQ(result, "*3\r\n:1\r\n:2\r\n:3\r\n");
}

TEST_F(RespSerializerTest, SerializeArrayOfMixedTypes) {
  std::vector<RespValue> elements = {
      CreateSimpleString("OK"),
      CreateInteger(42),
      CreateBulkString("foo"),
      CreateError("ERR unknown"),
  };
  auto val = CreateArray(elements);
  std::string result = RespSerializer::serialize(val);
  EXPECT_EQ(result,
            "*4\r\n"
            "+OK\r\n"
            ":42\r\n"
            "$3\r\nfoo\r\n"
            "-ERR unknown\r\n");
}

TEST_F(RespSerializerTest, SerializeArrayWithNullBulkString) {
  std::vector<RespValue> elements = {
      CreateBulkString("hello"),
      CreateNullBulkString(),
      CreateBulkString("world"),
  };
  auto val = CreateArray(elements);
  std::string result = RespSerializer::serialize(val);
  EXPECT_EQ(result,
            "*3\r\n"
            "$5\r\nhello\r\n"
            "$-1\r\n"
            "$5\r\nworld\r\n");
}

// Nested Array Tests
TEST_F(RespSerializerTest, SerializeNestedArray) {
  std::vector<RespValue> inner = {
      CreateInteger(1),
      CreateInteger(2),
  };
  std::vector<RespValue> outer = {
      CreateArray(inner),
      CreateBulkString("foo"),
  };
  auto val = CreateArray(outer);
  std::string result = RespSerializer::serialize(val);
  EXPECT_EQ(result,
            "*2\r\n"
            "*2\r\n"
            ":1\r\n"
            ":2\r\n"
            "$3\r\nfoo\r\n");
}

TEST_F(RespSerializerTest, SerializeArrayWithEmptyNestedArray) {
  std::vector<RespValue> outer = {
      CreateArray({}),
      CreateInteger(1),
  };
  auto val = CreateArray(outer);
  std::string result = RespSerializer::serialize(val);
  EXPECT_EQ(result,
            "*2\r\n"
            "*0\r\n"
            ":1\r\n");
}

TEST_F(RespSerializerTest, SerializeDeeplyNestedArrays) {
  // Create: *1\r\n*1\r\n*1\r\n:7\r\n
  std::vector<RespValue> level3 = {CreateInteger(7)};
  std::vector<RespValue> level2 = {CreateArray(level3)};
  std::vector<RespValue> level1 = {CreateArray(level2)};
  auto val = CreateArray(level1);

  std::string result = RespSerializer::serialize(val);
  EXPECT_EQ(result,
            "*1\r\n"
            "*1\r\n"
            "*1\r\n"
            ":7\r\n");
}

TEST_F(RespSerializerTest, SerializeVeryDeepNestedArraysWithoutRecursion) {
  constexpr std::size_t kDepth = 2000;

  RespValue current = CreateInteger(7);
  for (std::size_t i = 0; i < kDepth; ++i) {
    RespValue::RespArray elements;
    elements.emplace_back(std::move(current));
    current = RespValue(RespValue::Type::kArray, std::move(elements));
  }

  std::string result = RespSerializer::serialize(current);

  std::size_t offset = 0;
  for (std::size_t i = 0; i < kDepth; ++i) {
    ASSERT_LE(offset + 4, result.size());
    EXPECT_EQ(result.compare(offset, 4, "*1\r\n"), 0);
    offset += 4;
  }

  ASSERT_LE(offset + 4, result.size());
  EXPECT_EQ(result.compare(offset, 4, ":7\r\n"), 0);
  EXPECT_EQ(result.size(), offset + 4);
}

TEST_F(RespSerializerTest, SerializeComplexNestedStructure) {
  // Represents: ["SET", "key", "value"] command array
  std::vector<RespValue> cmd = {
      CreateBulkString("SET"),
      CreateBulkString("key"),
      CreateBulkString("value"),
  };
  auto val = CreateArray(cmd);
  std::string result = RespSerializer::serialize(val);
  EXPECT_EQ(result,
            "*3\r\n"
            "$3\r\nSET\r\n"
            "$3\r\nkey\r\n"
            "$5\r\nvalue\r\n");
}

// Round-trip Tests (ensuring parser can read serialized output)
TEST_F(RespSerializerTest, RoundTripSimpleString) {
  auto original = CreateSimpleString("OK");
  std::string serialized = RespSerializer::serialize(original);
  EXPECT_EQ(serialized, "+OK\r\n");
  // Verify format is correct
  EXPECT_TRUE(serialized[0] == '+');
  EXPECT_TRUE(serialized.substr(serialized.length() - 2) == "\r\n");
}

TEST_F(RespSerializerTest, RoundTripBulkString) {
  auto original = CreateBulkString("hello");
  std::string serialized = RespSerializer::serialize(original);
  EXPECT_EQ(serialized, "$5\r\nhello\r\n");
  // Verify format
  EXPECT_TRUE(serialized[0] == '$');
  EXPECT_TRUE(serialized.find("\r\n") != std::string::npos);
}

// Edge Cases
TEST_F(RespSerializerTest, SerializeUnknownType) {
  RespValue val;
  val.type = RespValue::Type::kUnknown;
  val.data = std::string();
  std::string result = RespSerializer::serialize(val);
  EXPECT_EQ(result, "-ERR unknown type\r\n");
}

TEST_F(RespSerializerTest, SerializeBulkStringWithTrailingNullBytes) {
  std::string data = "hello";
  data.push_back('\0');
  data += "world";
  auto val = CreateBulkString(data);
  std::string result = RespSerializer::serialize(val);
  EXPECT_EQ(result.size(), 18);  // "$11\r\n" + 11 bytes + "\r\n"
  EXPECT_TRUE(result.find("$11\r\n") == 0);
}

TEST_F(RespSerializerTest, SerializeArrayWithOneElement) {
  std::vector<RespValue> elements = {CreateInteger(42)};
  auto val = CreateArray(elements);
  std::string result = RespSerializer::serialize(val);
  EXPECT_EQ(result, "*1\r\n:42\r\n");
}

TEST_F(RespSerializerTest, SerializeLargeArray) {
  std::vector<RespValue> elements;
  elements.reserve(100);
  for (int i = 0; i < 100; ++i) {
    elements.push_back(CreateInteger(i));
  }
  auto val = CreateArray(elements);
  std::string result = RespSerializer::serialize(val);

  // Verify array header
  EXPECT_TRUE(result.find("*100\r\n") == 0);
  // Verify all elements are present
  for (int i = 0; i < 100; ++i) {
    EXPECT_TRUE(result.find(":" + std::to_string(i) + "\r\n") !=
                std::string::npos);
  }
}

// RESP Protocol Compliance Tests
TEST_F(RespSerializerTest, SimpleStringStartsWithPlus) {
  auto val = CreateSimpleString("test");
  std::string result = RespSerializer::serialize(val);
  EXPECT_EQ(result[0], '+');
}

TEST_F(RespSerializerTest, ErrorStartsWithMinus) {
  auto val = CreateError("test");
  std::string result = RespSerializer::serialize(val);
  EXPECT_EQ(result[0], '-');
}

TEST_F(RespSerializerTest, IntegerStartsWithColon) {
  auto val = CreateInteger(42);
  std::string result = RespSerializer::serialize(val);
  EXPECT_EQ(result[0], ':');
}

TEST_F(RespSerializerTest, BulkStringStartsWithDollar) {
  auto val = CreateBulkString("test");
  std::string result = RespSerializer::serialize(val);
  EXPECT_EQ(result[0], '$');
}

TEST_F(RespSerializerTest, ArrayStartsWithAsterisk) {
  auto val = CreateArray({CreateInteger(1)});
  std::string result = RespSerializer::serialize(val);
  EXPECT_EQ(result[0], '*');
}

TEST_F(RespSerializerTest, AllOutputsEndWithCRLF) {
  std::vector<RespValue> test_values = {
      CreateSimpleString("OK"), CreateError("ERR"),
      CreateInteger(42),        CreateBulkString("test"),
      CreateNullBulkString(),   CreateArray({CreateInteger(1)}),
  };

  for (const auto& val : test_values) {
    std::string result = RespSerializer::serialize(val);
    ASSERT_GE(result.length(), 2)
        << "Result too short to have CRLF: " << result;
    EXPECT_EQ(result.substr(result.length() - 2), "\r\n")
        << "Result does not end with CRLF: " << result;
  }
}

// Performance Test (no assertion, just ensure it doesn't crash)
TEST_F(RespSerializerTest, SerializeVeryLargeBulkString) {
  std::string large_str(100000, 'x');
  auto val = CreateBulkString(large_str);
  std::string result = RespSerializer::serialize(val);
  // "$100000\r\n" (9 bytes) + data (100000 bytes) + "\r\n" (2 bytes) = 100011 bytes
  EXPECT_EQ(result.size(), 100011);
}
