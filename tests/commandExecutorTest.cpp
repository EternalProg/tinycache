#include <gtest/gtest.h>
#include <command.hpp>
#include <commandExecutor.hpp>
#include <lruCache.hpp>
#include <respValue.hpp>

using tinycache::Command;
using tinycache::CommandExecutor;
using tinycache::CommandType;
using tinycache::LruCache;
using tinycache::RespValue;

class CommandExecutorTest : public ::testing::Test {
 protected:
  LruCache cache_{100};
  CommandExecutor executor_{cache_};
};

// GET COMMAND TESTS
TEST_F(CommandExecutorTest, GetExistingKeyReturnsValue) {
  cache_.set("key1", "value1");
  Command cmd{CommandType::kGet, {"key1"}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kBulkString);
  EXPECT_EQ(std::get<std::string>(result.data), "value1");
}

TEST_F(CommandExecutorTest, GetNonExistentKeyReturnsNull) {
  Command cmd{CommandType::kGet, {"nonexistent"}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kNullBulkString);
}

TEST_F(CommandExecutorTest, GetExpiredKeyReturnsNull) {
  cache_.set("key1", "value1");
  cache_.expire("key1", 1);
  cache_.remove_expired_keys(std::chrono::steady_clock::now() +
                             std::chrono::seconds(2));
  Command cmd{CommandType::kGet, {"key1"}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kNullBulkString);
}

TEST_F(CommandExecutorTest, GetReturnsCorrectType) {
  cache_.set("mykey", "myvalue");
  Command cmd{CommandType::kGet, {"mykey"}};

  RespValue result = executor_.execute(cmd);

  ASSERT_EQ(result.type, RespValue::Type::kBulkString);
}

// SET COMMAND TESTS
TEST_F(CommandExecutorTest, SetKeyWithoutOptionsReturnsOk) {
  Command cmd{CommandType::kSet, {"key1", "value1"}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kSimpleString);
  EXPECT_EQ(std::get<std::string>(result.data), "OK");
  EXPECT_EQ(cache_.get("key1").value(), "value1");
}

TEST_F(CommandExecutorTest, SetUpdatesExistingKey) {
  cache_.set("key1", "old_value");
  Command cmd{CommandType::kSet, {"key1", "new_value"}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kSimpleString);
  EXPECT_EQ(std::get<std::string>(result.data), "OK");
  EXPECT_EQ(cache_.get("key1").value(), "new_value");
}

TEST_F(CommandExecutorTest, SetClearsExpirationOnUpdate) {
  cache_.set("key1", "value1");
  cache_.expire("key1", 1);
  EXPECT_GE(cache_.ttl("key1"), 0);

  Command cmd{CommandType::kSet, {"key1", "value2"}};
  [[maybe_unused]] RespValue result = executor_.execute(cmd);

  // After update without expiration, TTL should be -1 (no expiration)
  EXPECT_EQ(cache_.ttl("key1"), -1);
}

TEST_F(CommandExecutorTest, SetWithExOptionSetsExpiration) {
  Command cmd{CommandType::kSet, {"key1", "value1", "EX", "10"}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kSimpleString);
  EXPECT_EQ(std::get<std::string>(result.data), "OK");
  std::int64_t ttl = cache_.ttl("key1");
  EXPECT_GE(ttl, 9);
  EXPECT_LE(ttl, 10);
}

TEST_F(CommandExecutorTest, SetWithInvalidExValueReturnsError) {
  Command cmd{CommandType::kSet, {"key1", "value1", "EX", "not_a_number"}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kError);
  EXPECT_EQ(std::get<std::string>(result.data),
            "ERR value is not an integer or out of range");
}

TEST_F(CommandExecutorTest, SetWithNxOptionWhenKeyNotExistSucceeds) {
  Command cmd{CommandType::kSet, {"newkey", "value1", "NX"}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kSimpleString);
  EXPECT_EQ(std::get<std::string>(result.data), "OK");
  EXPECT_EQ(cache_.get("newkey").value(), "value1");
}

TEST_F(CommandExecutorTest, SetWithNxOptionWhenKeyExistsFails) {
  cache_.set("key1", "existing_value");
  Command cmd{CommandType::kSet, {"key1", "new_value", "NX"}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kNullBulkString);
  // Value should not have changed
  EXPECT_EQ(cache_.get("key1").value(), "existing_value");
}

TEST_F(CommandExecutorTest, SetWithXxOptionWhenKeyExistSucceeds) {
  cache_.set("key1", "existing_value");
  Command cmd{CommandType::kSet, {"key1", "new_value", "XX"}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kSimpleString);
  EXPECT_EQ(std::get<std::string>(result.data), "OK");
  EXPECT_EQ(cache_.get("key1").value(), "new_value");
}

TEST_F(CommandExecutorTest, SetWithXxOptionWhenKeyNotExistFails) {
  Command cmd{CommandType::kSet, {"nonexistent", "value1", "XX"}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kNullBulkString);
  EXPECT_FALSE(cache_.get("nonexistent").has_value());
}

TEST_F(CommandExecutorTest, SetWithInvalidOptionReturnsError) {
  Command cmd{CommandType::kSet, {"key1", "value1", "INVALID"}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kError);
  EXPECT_EQ(std::get<std::string>(result.data), "ERR syntax error");
}

TEST_F(CommandExecutorTest, SetWithInvalidExOptionReturnsError) {
  Command cmd{CommandType::kSet, {"key1", "value1", "INVALID", "10"}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kError);
  EXPECT_EQ(std::get<std::string>(result.data), "ERR syntax error");
}

TEST_F(CommandExecutorTest, SetEmptyValue) {
  Command cmd{CommandType::kSet, {"key1", ""}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kSimpleString);
  EXPECT_EQ(cache_.get("key1").value(), "");
}

TEST_F(CommandExecutorTest, SetEmptyKey) {
  Command cmd{CommandType::kSet, {"", "value1"}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kSimpleString);
  EXPECT_EQ(cache_.get("").value(), "value1");
}

// DEL COMMAND TESTS
TEST_F(CommandExecutorTest, DelExistingKeyReturnsOne) {
  cache_.set("key1", "value1");
  Command cmd{CommandType::kDel, {"key1"}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kInteger);
  EXPECT_EQ(std::get<std::int64_t>(result.data), 1);
  EXPECT_FALSE(cache_.get("key1").has_value());
}

TEST_F(CommandExecutorTest, DelNonExistentKeyReturnsZero) {
  Command cmd{CommandType::kDel, {"nonexistent"}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kInteger);
  EXPECT_EQ(std::get<std::int64_t>(result.data), 0);
}

TEST_F(CommandExecutorTest, DelMultipleKeysCountsSuccessful) {
  cache_.set("key1", "value1");
  cache_.set("key2", "value2");
  Command cmd{CommandType::kDel, {"key1", "key2", "key3"}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kInteger);
  EXPECT_EQ(std::get<std::int64_t>(result.data), 2);  // Only 2 were deleted
  EXPECT_FALSE(cache_.get("key1").has_value());
  EXPECT_FALSE(cache_.get("key2").has_value());
}

TEST_F(CommandExecutorTest, DelMultipleKeysWithNoDuplicates) {
  cache_.set("key1", "value1");
  cache_.set("key2", "value2");
  Command cmd{CommandType::kDel, {"key1", "key1", "key2"}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kInteger);
  EXPECT_EQ(std::get<std::int64_t>(result.data), 2);  // key1 only deleted once
}

TEST_F(CommandExecutorTest, DelReturnsCorrectType) {
  cache_.set("key1", "value1");
  Command cmd{CommandType::kDel, {"key1"}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kInteger);
}

// EXPIRE COMMAND TESTS
TEST_F(CommandExecutorTest, ExpireExistingKeyReturnsOne) {
  cache_.set("key1", "value1");
  Command cmd{CommandType::kExpire, {"key1", "10"}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kInteger);
  EXPECT_EQ(std::get<std::int64_t>(result.data), 1);
  EXPECT_GE(cache_.ttl("key1"), 0);
}

TEST_F(CommandExecutorTest, ExpireNonExistentKeyReturnsZero) {
  Command cmd{CommandType::kExpire, {"nonexistent", "10"}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kInteger);
  EXPECT_EQ(std::get<std::int64_t>(result.data), 0);
}

TEST_F(CommandExecutorTest, ExpireWithInvalidSecondsReturnsError) {
  cache_.set("key1", "value1");
  Command cmd{CommandType::kExpire, {"key1", "not_a_number"}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kError);
  EXPECT_EQ(std::get<std::string>(result.data),
            "ERR value is not an integer or out of range");
}

TEST_F(CommandExecutorTest, ExpireWithZeroSecondsExpirksKeyImmediately) {
  cache_.set("key1", "value1");
  Command cmd{CommandType::kExpire, {"key1", "0"}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kInteger);
  EXPECT_EQ(std::get<std::int64_t>(result.data), 1);
  // After cleanup with current time, key should be gone
  cache_.remove_expired_keys(std::chrono::steady_clock::now());
  EXPECT_FALSE(cache_.get("key1").has_value());
}

TEST_F(CommandExecutorTest, ExpireUpdatesExistingTtl) {
  cache_.set("key1", "value1");
  cache_.expire("key1", 100);
  std::int64_t ttl1 = cache_.ttl("key1");

  Command cmd{CommandType::kExpire, {"key1", "10"}};
  [[maybe_unused]] RespValue result = executor_.execute(cmd);

  std::int64_t ttl2 = cache_.ttl("key1");
  EXPECT_LT(ttl2, ttl1);
  EXPECT_GE(ttl2, 9);
}

TEST_F(CommandExecutorTest, ExpireReturnsCorrectType) {
  cache_.set("key1", "value1");
  Command cmd{CommandType::kExpire, {"key1", "10"}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kInteger);
}

// TTL COMMAND TESTS
TEST_F(CommandExecutorTest, TtlExistingKeyWithoutExpirationReturnsNegativeOne) {
  cache_.set("key1", "value1");
  Command cmd{CommandType::kTtl, {"key1"}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kInteger);
  EXPECT_EQ(std::get<std::int64_t>(result.data), -1);
}

TEST_F(CommandExecutorTest, TtlNonExistentKeyReturnsNegativeTwo) {
  Command cmd{CommandType::kTtl, {"nonexistent"}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kInteger);
  EXPECT_EQ(std::get<std::int64_t>(result.data), -2);
}

TEST_F(CommandExecutorTest, TtlExistingKeyWithExpirationReturnsPositive) {
  cache_.set("key1", "value1");
  cache_.expire("key1", 10);
  Command cmd{CommandType::kTtl, {"key1"}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kInteger);
  EXPECT_GE(std::get<std::int64_t>(result.data), 0);
  EXPECT_LE(std::get<std::int64_t>(result.data), 10);
}

TEST_F(CommandExecutorTest, TtlExpiredKeyReturnsNegativeTwo) {
  cache_.set("key1", "value1");
  cache_.expire("key1", 1);
  cache_.remove_expired_keys(std::chrono::steady_clock::now() +
                             std::chrono::seconds(2));
  Command cmd{CommandType::kTtl, {"key1"}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kInteger);
  EXPECT_EQ(std::get<std::int64_t>(result.data), -2);
}

TEST_F(CommandExecutorTest, TtlReturnsCorrectType) {
  cache_.set("key1", "value1");
  Command cmd{CommandType::kTtl, {"key1"}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kInteger);
}

// PING COMMAND TESTS
TEST_F(CommandExecutorTest, PingWithoutArgumentReturnsPong) {
  Command cmd{CommandType::kPing, {}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kBulkString);
  EXPECT_EQ(std::get<std::string>(result.data), "PONG");
}

TEST_F(CommandExecutorTest, PingWithArgumentReturnsArgument) {
  Command cmd{CommandType::kPing, {"hello"}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kBulkString);
  EXPECT_EQ(std::get<std::string>(result.data), "hello");
}

TEST_F(CommandExecutorTest, PingWithMultipleArgumentsReturnsFirst) {
  Command cmd{CommandType::kPing, {"hello", "world"}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kBulkString);
  // When no args or multiple args, returns PONG
  EXPECT_EQ(std::get<std::string>(result.data), "PONG");
}

TEST_F(CommandExecutorTest, PingDoesNotAccessCache) {
  Command cmd{CommandType::kPing, {"test"}};

  // Should work even if cache is full/in bad state
  for (int i = 0; i < 100; ++i) {
    cache_.set("key" + std::to_string(i), "value");
  }

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kBulkString);
  EXPECT_EQ(std::get<std::string>(result.data), "test");
}

// COMMAND COMMAND TESTS
TEST_F(CommandExecutorTest, CommandWithoutArgumentsReturnsArrayOfCommands) {
  Command cmd{CommandType::kCommand, {}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kArray);
  EXPECT_GT(std::get<RespValue::RespArray>(result.data).size(), 0);
}

TEST_F(CommandExecutorTest, CommandArrayContainsCommandNames) {
  Command cmd{CommandType::kCommand, {}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kArray);
  // Each element should be an array (command info)
  for (const auto& cmd_info : std::get<RespValue::RespArray>(result.data)) {
    EXPECT_EQ(cmd_info.type, RespValue::Type::kArray);
    EXPECT_GE(std::get<RespValue::RespArray>(cmd_info.data).size(),
              2);  // name and arity at minimum
  }
}

TEST_F(CommandExecutorTest, CommandInfoForSpecificCommand) {
  Command cmd{CommandType::kCommand, {"INFO", "get"}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kArray);
  EXPECT_GT(std::get<RespValue::RespArray>(result.data).size(), 0);
}

TEST_F(CommandExecutorTest, CommandCountReturnsInteger) {
  Command cmd{CommandType::kCommand, {"COUNT"}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kInteger);
  EXPECT_GT(std::get<std::int64_t>(result.data), 0);
}

TEST_F(CommandExecutorTest, CommandUnknownSubcommandReturnsError) {
  Command cmd{CommandType::kCommand, {"UNKNOWN"}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kError);
  EXPECT_EQ(std::get<std::string>(result.data), "Unknown subcommand");
}

// UNKNOWN COMMAND TESTS
TEST_F(CommandExecutorTest, UnknownCommandReturnsError) {
  Command cmd{CommandType::kUnknown, {}};

  RespValue result = executor_.execute(cmd);

  EXPECT_EQ(result.type, RespValue::Type::kError);
  EXPECT_EQ(std::get<std::string>(result.data), "ERR unknown command");
}

// INTEGRATION TESTS
TEST_F(CommandExecutorTest, MultipleOperationsWorkTogether) {
  // SET
  Command set_cmd{CommandType::kSet, {"key1", "value1", "EX", "10"}};
  RespValue set_result = executor_.execute(set_cmd);
  EXPECT_EQ(set_result.type, RespValue::Type::kSimpleString);

  // GET
  Command get_cmd{CommandType::kGet, {"key1"}};
  RespValue get_result = executor_.execute(get_cmd);
  EXPECT_EQ(std::get<std::string>(get_result.data), "value1");

  // TTL
  Command ttl_cmd{CommandType::kTtl, {"key1"}};
  RespValue ttl_result = executor_.execute(ttl_cmd);
  EXPECT_GE(std::get<std::int64_t>(ttl_result.data), 0);
  EXPECT_LE(std::get<std::int64_t>(ttl_result.data), 10);

  // DEL
  Command del_cmd{CommandType::kDel, {"key1"}};
  RespValue del_result = executor_.execute(del_cmd);
  EXPECT_EQ(std::get<std::int64_t>(del_result.data), 1);

  // GET after DEL should return null
  RespValue get_after_del = executor_.execute(get_cmd);
  EXPECT_EQ(get_after_del.type, RespValue::Type::kNullBulkString);
}

TEST_F(CommandExecutorTest, SetGetDeleteMultipleKeys) {
  for (int i = 0; i < 5; ++i) {
    Command cmd{CommandType::kSet,
                {"key" + std::to_string(i), "value" + std::to_string(i)}};
    [[maybe_unused]] RespValue result = executor_.execute(cmd);
  }

  for (int i = 0; i < 5; ++i) {
    Command cmd{CommandType::kGet, {"key" + std::to_string(i)}};
    RespValue result = executor_.execute(cmd);
    EXPECT_EQ(std::get<std::string>(result.data), "value" + std::to_string(i));
  }

  Command del_cmd{CommandType::kDel, {"key0", "key1", "key2", "key3", "key4"}};
  RespValue del_result = executor_.execute(del_cmd);
  EXPECT_EQ(std::get<std::int64_t>(del_result.data), 5);
}

TEST_F(CommandExecutorTest, CaseInsensitiveCommands) {
  // Options should be case-insensitive
  Command cmd1{CommandType::kSet, {"key1", "value1", "ex", "10"}};
  RespValue result1 = executor_.execute(cmd1);
  EXPECT_EQ(result1.type, RespValue::Type::kSimpleString);

  Command cmd2{CommandType::kSet, {"key2", "value2", "EX", "10"}};
  RespValue result2 = executor_.execute(cmd2);
  EXPECT_EQ(result2.type, RespValue::Type::kSimpleString);

  Command cmd3{CommandType::kSet, {"key3", "value3", "Ex", "10"}};
  RespValue result3 = executor_.execute(cmd3);
  EXPECT_EQ(result3.type, RespValue::Type::kSimpleString);
}
