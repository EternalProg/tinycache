#include <spdlog/spdlog.h>
#include <server.hpp>

int main() {
  spdlog::set_level(spdlog::level::debug);

  auto config = tinycache::get_config();

  tinycache::Server server(config);

  server.run();
}
