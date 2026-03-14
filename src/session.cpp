#include <spdlog/spdlog.h>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/write.hpp>
#include <command.hpp>
#include <respParser.hpp>
#include <respValue.hpp>
#include <session.hpp>
#include <utils.hpp>
#include "respSerializer.hpp"

namespace tinycache {

inline constexpr std::size_t kMaxMessageSize = 1024;

Session::Session(asio::ip::tcp::socket socket, ShardPool& shard_pool)
    : socket_(std::move(socket)),
      strand_(asio::make_strand(socket_.get_executor())),
      executor_(shard_pool) {}

asio::awaitable<void> Session::run() {
  for (;;) {
    auto reading_result = co_await read();

    if (reading_result != ReadResult::kNewMessage) {
      break;
    }

    RespValue value;
    auto parsing_result = RespParser::parse(buffer_, value);

    if (parsing_result == ParsingResult::kNeedMoreData) {
      continue;
    }

    if (parsing_result == ParsingResult::kError) {
      co_await write(RespSerializer::serialize(
          RespValue(RespValue::Type::kError, "ERR malformed request")));
      continue;
    }

    std::optional<Command> command = Command::toCommand(value);

    if (!command.has_value()) {
      co_await write(RespSerializer::serialize(
          RespValue(RespValue::Type::kError, "ERR invalid command")));
      continue;
    }

    auto response = co_await executor_.execute(*command);
    co_await write(RespSerializer::serialize(response));
  }
  spdlog::debug("Session closed");
}

asio::awaitable<ReadResult> Session::read() {
  auto [ec, nbytes] =
      co_await asio::async_read(socket_, buffer_, asio::transfer_at_least(1),
                                asio::bind_executor(strand_, kAsTuple));

  if (ec == asio::error::eof) {
    spdlog::debug("Client closed (EOF)");
    co_return ReadResult::kNewMessage;  // allow parsing
  }

  if (ec) {
    spdlog::error("Read error: {}", ec.message());
    co_return ReadResult::kReadError;
  }

  co_return ReadResult::kNewMessage;
}

asio::awaitable<void> Session::write(std::string_view message) {
  auto [ec, _] = co_await asio::async_write(
      socket_, asio::buffer(message), asio::bind_executor(strand_, kAsTuple));

  if (ec == asio::error::eof) {
    spdlog::debug("Connection is closing");
    co_return;
  }
  if (ec) {
    spdlog::error("Read: {}", ec.message());
    co_return;
  }

  spdlog::debug("Response sent: {}", message);
}

}  // namespace tinycache
