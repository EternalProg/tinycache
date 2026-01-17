#ifndef TINYCACHE_RESP_PARSER_HPP
#define TINYCACHE_RESP_PARSER_HPP

#include <boost/asio/streambuf.hpp>
#include <cstdint>
#include "respValue.hpp"

namespace tinycache {
enum class ParsingResult : std::uint8_t { kReady, kNeedMoreData, kError };

/**
 * @brief RESP protocol streaming parser.
 *
 * Parses a single RESP value from the provided stream buffer.
 * The parser operates incrementally and may require multiple
 * invocations until the value is fully parsed.
 */
struct RespParser {
  /**
   * @brief Attempts to parse a RESP value from the input buffer.
   *
   * This function examines the contents of the buffer and tries to parse
   * exactly one RESP value. The buffer is only consumed if the parsing
   * either succeeds or fails with an unrecoverable error.
   *
   * @param buffer Input buffer containing RESP-encoded data.
   *               The buffer may contain partial data.
   *
   * @param outValue Output parameter that receives the parsed value.
   *                 This parameter is modified only if the function
   *                 returns ParsingResult::kReady.
   *
   * @return ParsingResult::kReady if a complete RESP value was parsed.
   * @return ParsingResult::kNeedMoreData if the buffer does not contain
   *         enough data to complete parsing. The buffer is left unchanged.
   * @return ParsingResult::kError if the input data is malformed.
   *         The buffer is consumed up to the point of error.
   *
   * @note The function guarantees strong exception safety:
   *       outValue is not modified unless parsing completes successfully.
   *
   * @warning On ParsingResult::kError the buffer content is considered
   *          invalid and should typically be discarded.
   */
  static ParsingResult parse(boost::asio::streambuf& buffer,
                             RespValue& outValue);
};

}  // namespace tinycache

#endif
