#include <spdlog/spdlog.h>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/write.hpp>
#include <session.hpp>
#include <string>
#include <string_view>
#include <vector>
#include "command.hpp"
#include "respParser.hpp"
#include "respValue.hpp"
#include "utils.hpp"

namespace tinycache {

namespace {

std::string encodeSimpleString(std::string_view data) {
  std::string output;
  output.reserve(data.size() + 3);
  output.push_back('+');
  output.append(data);
  output.append("\r\n");
  return output;
}

std::string encodeError(std::string_view data) {
  std::string output;
  output.reserve(data.size() + 3);
  output.push_back('-');
  output.append(data);
  output.append("\r\n");
  return output;
}

std::string encodeRespValue(const RespValue& value) {
  if (value.type == RespValue::Type::kSimpleString &&
      std::holds_alternative<std::string>(value.data)) {
    return encodeSimpleString(std::get<std::string>(value.data));
  }

  if (value.type == RespValue::Type::kError &&
      std::holds_alternative<std::string>(value.data)) {
    return encodeError(std::get<std::string>(value.data));
  }

  if (value.type == RespValue::Type::kBulkString &&
      std::holds_alternative<std::string>(value.data)) {
    const auto& data = std::get<std::string>(value.data);
    std::string output;
    output.reserve(data.size() + 32);
    output.push_back('$');
    output.append(std::to_string(data.size()));
    output.append("\r\n");
    output.append(data);
    output.append("\r\n");
    return output;
  }

  if (value.type == RespValue::Type::kInteger &&
      std::holds_alternative<std::int64_t>(value.data)) {
    std::string output;
    output.reserve(32);
    output.push_back(':');
    output.append(std::to_string(std::get<std::int64_t>(value.data)));
    output.append("\r\n");
    return output;
  }

  if (value.type == RespValue::Type::kNullBulkString) {
    return "$-1\r\n";
  }

  if (value.type == RespValue::Type::kArray &&
      std::holds_alternative<std::vector<RespValue>>(value.data)) {
    const auto& elements = std::get<std::vector<RespValue>>(value.data);
    std::string output;
    output.reserve(32);
    output.push_back('*');
    output.append(std::to_string(elements.size()));
    output.append("\r\n");
    for (const auto& element : elements) {
      output.append(encodeRespValue(element));
    }
    return output;
  }

  return encodeError("ERR unsupported response");
}

}  // namespace

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
      co_await write(encodeError("ERR invalid command"));
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
    spdlog::error("Read: {}", ec.message());
    co_return;
  }

  spdlog::debug("Response sent: {}", message);
}

}  // namespace tinycache
