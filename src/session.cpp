#include <spdlog/spdlog.h>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/write.hpp>
#include <command.hpp>
#include <respParser.hpp>
#include <respValue.hpp>
#include <session.hpp>
#include <utils.hpp>
#include "respSerializer.hpp"

namespace tinycache {

Session::Session(asio::ip::tcp::socket socket, ShardPool& shard_pool,
                 std::size_t home_shard, std::size_t max_message_size)
    : socket_(std::move(socket)),
      strand_(asio::make_strand(socket_.get_executor())),
      executor_(shard_pool),
      home_shard_(home_shard),
      max_message_size_(max_message_size == 0 ? 1 : max_message_size) {}

asio::awaitable<void> Session::run() {
  for (;;) {
    bool close_session = false;

    while (buffer_.size() > 0) {
      RespValue value;
      auto parsing_result = RespParser::parse(buffer_, value);

      if (parsing_result == ParsingResult::kNeedMoreData) {
        break;
      }

      if (parsing_result == ParsingResult::kError) {
        co_await write(RespSerializer::serialize(
            RespValue(RespValue::Type::kError, "ERR malformed request")));
        close_session = true;
        break;
      }

      std::optional<Command> command = Command::toCommand(value);

      if (!command.has_value()) {
        co_await write(RespSerializer::serialize(
            RespValue(RespValue::Type::kError, "ERR invalid command")));
        continue;
      }

      auto response = co_await executor_.execute(*command, home_shard_);
      co_await write(RespSerializer::serialize(response));
    }

    if (close_session) {
      break;
    }

    auto reading_result = co_await read();

    if (reading_result == ReadResult::kMessageTooLarge) {
      co_await write(RespSerializer::serialize(
          RespValue(RespValue::Type::kError, "ERR request is too large")));
      break;
    }

    if (reading_result != ReadResult::kNewMessage) {
      break;
    }
  }
  SPDLOG_DEBUG("Session closed");
}

asio::awaitable<ReadResult> Session::read() {
  if (buffer_.size() >= max_message_size_) {
    co_return ReadResult::kMessageTooLarge;
  }

  const std::size_t available_capacity = max_message_size_ - buffer_.size();
  auto mutable_buffer = buffer_.prepare(available_capacity);

  boost::system::error_code ec;
  auto nbytes = co_await socket_.async_read_some(
      mutable_buffer,
      asio::bind_executor(strand_,
                          asio::redirect_error(asio::use_awaitable, ec)));

  if (ec == asio::error::eof) {
    buffer_.commit(0);
    SPDLOG_DEBUG("Client closed (EOF)");
    co_return ReadResult::kCloseConnection;
  }

  if (ec) {
    buffer_.commit(0);
    spdlog::error("Read error: {}", ec.message());
    co_return ReadResult::kReadError;
  }

  buffer_.commit(nbytes);

  co_return ReadResult::kNewMessage;
}

asio::awaitable<void> Session::write(std::string message) {
  boost::system::error_code ec;
  [[maybe_unused]] auto nbytes = co_await asio::async_write(
      socket_, asio::buffer(message),
      asio::bind_executor(strand_,
                          asio::redirect_error(asio::use_awaitable, ec)));

  if (ec == asio::error::eof) {
    SPDLOG_DEBUG("Connection is closing");
    co_return;
  }
  if (ec) {
    spdlog::error("Read: {}", ec.message());
    co_return;
  }

  SPDLOG_DEBUG("Response sent: {}", message);
}

}  // namespace tinycache
