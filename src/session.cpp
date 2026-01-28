#include <spdlog/spdlog.h>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/write.hpp>
#include <session.hpp>
#include <string>
#include "command.hpp"
#include "respEncoder.hpp"
#include "respParser.hpp"
#include "respValue.hpp"
#include "utils.hpp"

namespace tinycache {

Session::Session(asio::ip::tcp::socket socket)
    : socket_(std::move(socket)),
      strand_(asio::make_strand(socket_.get_executor())) {}

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
      // For now I won't send anything. Maybe it's responsibility of CLI;
      // co_await sendError("Protocol Error");
      continue;
    }

    auto command = Command::toCommand(value);
    if (!command.has_value()) {
      RespValue error;
      error.type = RespValue::Type::kError;
      error.data = std::string("ERR invalid command");
      co_await write(encodeRespValue(error));
      continue;
    }

    auto response = executor_.execute(*command);
    co_await write(encodeRespValue(response));
  }
  spdlog::debug("Session closed");
}

asio::awaitable<ReadResult> Session::read() {
  auto [ec, nbytes] =
      co_await asio::async_read(socket_, buffer_, asio::transfer_at_least(1),
                                asio::bind_executor(strand_, kAsTuple));

  if (ec == asio::error::eof) {
    spdlog::debug("Client closed");
    co_return ReadResult::kCloseConnection;
  }

  if (ec) {
    spdlog::error("Read: {}", ec.message());
    co_return ReadResult::kReadError;
  }

  // std::istream is(&buffer_);
  // std::string line;
  // std::getline(is, line);

  spdlog::debug("Read message ({} bytes)", nbytes);

  co_return ReadResult::kNewMessage;
}

asio::awaitable<void> Session::write(std::string message) {
  auto [ec, _] = co_await asio::async_write(
      socket_, asio::buffer(message), asio::bind_executor(strand_, kAsTuple));

  if (ec == asio::error::eof) {
    spdlog::debug("Connection is closing");
    co_return;
  }
  if (ec) {
    spdlog::error("Write: {}", ec.message());
    co_return;
  }

  spdlog::debug("Response sent: {}", message);
}

}  // namespace tinycache
