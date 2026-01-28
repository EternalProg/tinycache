#include <gtest/gtest.h>
#include <command.hpp>
#include <commandExecutor.hpp>
#include <string>

using tinycache::Command;
using tinycache::CommandExecutor;
using tinycache::CommandType;
using tinycache::RespValue;

namespace {

RespValue execute(CommandType type, std::initializer_list<std::string> args) {
  Command command;
  command.type = type;
  command.args.assign(args.begin(), args.end());
  CommandExecutor executor;
  return executor.execute(command);
}

}  // namespace

TEST(CommandExecutorTest, GetWithoutKeyReturnsError) {
  auto response = execute(CommandType::kGet, {});

  EXPECT_EQ(response.type, RespValue::Type::kError);
  EXPECT_EQ(std::get<std::string>(response.data),
            "ERR wrong number of arguments for 'get'");
}

TEST(CommandExecutorTest, GetWithKeyReturnsOk) {
  auto response = execute(CommandType::kGet, {"foo"});

  EXPECT_EQ(response.type, RespValue::Type::kSimpleString);
  EXPECT_EQ(std::get<std::string>(response.data), "OK");
}

TEST(CommandExecutorTest, SetWithoutValueReturnsError) {
  auto response = execute(CommandType::kSet, {"foo"});

  EXPECT_EQ(response.type, RespValue::Type::kError);
  EXPECT_EQ(std::get<std::string>(response.data),
            "ERR wrong number of arguments for 'set'");
}

TEST(CommandExecutorTest, SetWithKeyValueReturnsOk) {
  auto response = execute(CommandType::kSet, {"foo", "bar"});

  EXPECT_EQ(response.type, RespValue::Type::kSimpleString);
  EXPECT_EQ(std::get<std::string>(response.data), "OK");
}

TEST(CommandExecutorTest, UnknownCommandReturnsError) {
  auto response = execute(CommandType::kUnknown, {"foo"});

  EXPECT_EQ(response.type, RespValue::Type::kError);
  EXPECT_EQ(std::get<std::string>(response.data), "ERR unknown command");
}

TEST(CommandExecutorTest, DelWithoutKeyReturnsError) {
  auto response = execute(CommandType::kDel, {});

  EXPECT_EQ(response.type, RespValue::Type::kError);
  EXPECT_EQ(std::get<std::string>(response.data),
            "ERR wrong number of arguments for 'del'");
}

TEST(CommandExecutorTest, ExpireWithoutTimeoutReturnsError) {
  auto response = execute(CommandType::kExpire, {"foo"});

  EXPECT_EQ(response.type, RespValue::Type::kError);
  EXPECT_EQ(std::get<std::string>(response.data),
            "ERR wrong number of arguments for 'expire'");
}

TEST(CommandExecutorTest, TtlWithoutKeyReturnsError) {
  auto response = execute(CommandType::kTtl, {});

  EXPECT_EQ(response.type, RespValue::Type::kError);
  EXPECT_EQ(std::get<std::string>(response.data),
            "ERR wrong number of arguments for 'ttl'");
}
