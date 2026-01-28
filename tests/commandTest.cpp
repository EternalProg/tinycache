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
