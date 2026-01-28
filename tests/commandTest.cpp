#include <gtest/gtest.h>
#include <command.hpp>
#include <string>
#include <vector>

using tinycache::Command;
using tinycache::CommandType;
using tinycache::RespValue;

namespace {

RespValue makeBulkString(std::string value) {
  RespValue resp;
  resp.type = RespValue::Type::kBulkString;
  resp.data = std::move(value);
  return resp;
}

}  // namespace

TEST(CommandTest, ParsesGetCommandFromArray) {
  RespValue value;
  value.type = RespValue::Type::kArray;
  value.data =
      std::vector<RespValue>{makeBulkString("GET"), makeBulkString("foo")};

  auto command = Command::toCommand(value);

  ASSERT_TRUE(command.has_value());
  EXPECT_EQ(command->type, CommandType::kGet);
  ASSERT_EQ(command->args.size(), 1u);
  EXPECT_EQ(command->args[0], "foo");
}

TEST(CommandTest, RejectsNonArrayValue) {
  RespValue value;
  value.type = RespValue::Type::kSimpleString;
  value.data = std::string("GET");

  auto command = Command::toCommand(value);

  EXPECT_FALSE(command.has_value());
}

TEST(CommandTest, ParsesSetCommandFromArray) {
  RespValue value;
  value.type = RespValue::Type::kArray;
  value.data = std::vector<RespValue>{makeBulkString("SET"),
                                      makeBulkString("foo"),
                                      makeBulkString("bar")};

  auto command = Command::toCommand(value);

  ASSERT_TRUE(command.has_value());
  EXPECT_EQ(command->type, CommandType::kSet);
  ASSERT_EQ(command->args.size(), 2u);
  EXPECT_EQ(command->args[0], "foo");
  EXPECT_EQ(command->args[1], "bar");
}

TEST(CommandTest, ParsesDelCommandFromArray) {
  RespValue value;
  value.type = RespValue::Type::kArray;
  value.data =
      std::vector<RespValue>{makeBulkString("DEL"), makeBulkString("foo")};

  auto command = Command::toCommand(value);

  ASSERT_TRUE(command.has_value());
  EXPECT_EQ(command->type, CommandType::kDel);
  ASSERT_EQ(command->args.size(), 1u);
  EXPECT_EQ(command->args[0], "foo");
}

TEST(CommandTest, ParsesExpireCommandFromArray) {
  RespValue value;
  value.type = RespValue::Type::kArray;
  value.data =
      std::vector<RespValue>{makeBulkString("EXPIRE"), makeBulkString("foo"),
                             makeBulkString("10")};

  auto command = Command::toCommand(value);

  ASSERT_TRUE(command.has_value());
  EXPECT_EQ(command->type, CommandType::kExpire);
  ASSERT_EQ(command->args.size(), 2u);
  EXPECT_EQ(command->args[0], "foo");
  EXPECT_EQ(command->args[1], "10");
}

TEST(CommandTest, ParsesTtlCommandFromArray) {
  RespValue value;
  value.type = RespValue::Type::kArray;
  value.data =
      std::vector<RespValue>{makeBulkString("TTL"), makeBulkString("foo")};

  auto command = Command::toCommand(value);

  ASSERT_TRUE(command.has_value());
  EXPECT_EQ(command->type, CommandType::kTtl);
  ASSERT_EQ(command->args.size(), 1u);
  EXPECT_EQ(command->args[0], "foo");
}

TEST(CommandTest, ParsesUnknownCommandType) {
  RespValue value;
  value.type = RespValue::Type::kArray;
  value.data =
      std::vector<RespValue>{makeBulkString("NOPE"), makeBulkString("foo")};

  auto command = Command::toCommand(value);

  ASSERT_TRUE(command.has_value());
  EXPECT_EQ(command->type, CommandType::kUnknown);
  ASSERT_EQ(command->args.size(), 1u);
  EXPECT_EQ(command->args[0], "foo");
}
