#include <gtest/gtest.h>
#include <command.hpp>

using tinycache::Command;
using tinycache::CommandType;
using tinycache::RespValue;

class CommandTest : public ::testing::Test {
 protected:
  static RespValue makeArray(std::initializer_list<RespValue> elements) {
    RespValue v;
    v.type = RespValue::Type::kArray;
    v.data = std::vector<RespValue>(elements);
    return v;
  }

  static RespValue bulk(std::string s) {
    return RespValue{RespValue::Type::kBulkString, std::move(s)};
  }

  static RespValue simple(std::string s) {
    return RespValue{RespValue::Type::kSimpleString, std::move(s)};
  }

  static RespValue integer(std::int64_t v) {
    return RespValue{RespValue::Type::kInteger, v};
  }
};

// Parser each command correctly
TEST_F(CommandTest, ParsesGetCommand) {
  RespValue v = makeArray({bulk("GET"), bulk("key")});

  auto cmd = Command::toCommand(v);

  ASSERT_TRUE(cmd.has_value());
  EXPECT_EQ(cmd->type, CommandType::kGet);
  ASSERT_EQ(cmd->args.size(), 1);
  EXPECT_EQ(cmd->args[0], "key");
}

TEST_F(CommandTest, ParsesSetCommand) {
  RespValue v = makeArray({bulk("SET"), bulk("key"), bulk("value")});

  auto cmd = Command::toCommand(v);

  ASSERT_TRUE(cmd.has_value());
  EXPECT_EQ(cmd->type, CommandType::kSet);
  ASSERT_EQ(cmd->args.size(), 2);
  EXPECT_EQ(cmd->args[0], "key");
  EXPECT_EQ(cmd->args[1], "value");
}

TEST_F(CommandTest, ParsesDelCommand) {
  RespValue v = makeArray({bulk("DEL"), bulk("key")});

  auto cmd = Command::toCommand(v);

  ASSERT_TRUE(cmd.has_value());
  EXPECT_EQ(cmd->type, CommandType::kDel);
  ASSERT_EQ(cmd->args.size(), 1);
  EXPECT_EQ(cmd->args[0], "key");
}

TEST_F(CommandTest, ParsesExpireCommand) {
  RespValue v = makeArray({bulk("EXPIRE"), bulk("key"), bulk("60")});

  auto cmd = Command::toCommand(v);

  ASSERT_TRUE(cmd.has_value());
  EXPECT_EQ(cmd->type, CommandType::kExpire);
  ASSERT_EQ(cmd->args.size(), 2);
  EXPECT_EQ(cmd->args[0], "key");
  EXPECT_EQ(cmd->args[1], "60");
}

TEST_F(CommandTest, ParsesTtlCommand) {
  RespValue v = makeArray({bulk("TTL"), bulk("key")});

  auto cmd = Command::toCommand(v);

  ASSERT_TRUE(cmd.has_value());
  EXPECT_EQ(cmd->type, CommandType::kTtl);
  ASSERT_EQ(cmd->args.size(), 1);
  EXPECT_EQ(cmd->args[0], "key");
}

TEST_F(CommandTest, CommandIsCaseInsensitive) {
  RespValue v = makeArray({bulk("gEt"), bulk("key")});

  auto cmd = Command::toCommand(v);

  ASSERT_TRUE(cmd.has_value());
  EXPECT_EQ(cmd->type, CommandType::kGet);
}

TEST_F(CommandTest, SimpleStringCommandNameIsAllowed) {
  RespValue v = makeArray({simple("GET"), bulk("key")});

  auto cmd = Command::toCommand(v);

  ASSERT_TRUE(cmd.has_value());
  EXPECT_EQ(cmd->type, CommandType::kGet);
}

// Invalid cases
TEST_F(CommandTest, NonArrayReturnsNullopt) {
  RespValue v = bulk("GET");

  auto cmd = Command::toCommand(v);

  EXPECT_FALSE(cmd.has_value());
}

TEST_F(CommandTest, EmptyArrayReturnsNullopt) {
  RespValue v = makeArray({});

  auto cmd = Command::toCommand(v);

  EXPECT_FALSE(cmd.has_value());
}

TEST_F(CommandTest, UnknownCommandReturnsNullopt) {
  RespValue v = makeArray({bulk("UNKNOWN")});

  auto cmd = Command::toCommand(v);

  EXPECT_FALSE(cmd.has_value());
}

TEST_F(CommandTest, NonStringArgumentReturnsNullopt) {
  RespValue v = makeArray({bulk("GET"), integer(42)});

  auto cmd = Command::toCommand(v);

  EXPECT_FALSE(cmd.has_value());
}

TEST_F(CommandTest, NullBulkStringArgumentReturnsNullopt) {
  RespValue null_bulk;
  null_bulk.type = RespValue::Type::kNullBulkString;

  RespValue v = makeArray({bulk("GET"), null_bulk});

  auto cmd = Command::toCommand(v);

  EXPECT_FALSE(cmd.has_value());
}

TEST_F(CommandTest, DelWithMultipleKeysIsAccepted) {
  RespValue v = makeArray({bulk("DEL"), bulk("key1"), bulk("key2")});

  auto cmd = Command::toCommand(v);

  ASSERT_TRUE(cmd.has_value());
  EXPECT_EQ(cmd->type, CommandType::kDel);
  EXPECT_EQ(cmd->args.size(), 2);
}

TEST_F(CommandTest, ExpireWithoutSecondsReturnsNullopt) {
  RespValue v = makeArray({bulk("EXPIRE"), bulk("key")});

  auto cmd = Command::toCommand(v);

  EXPECT_FALSE(cmd.has_value());
}
