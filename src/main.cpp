#include <spdlog/spdlog.h>
#include <server.hpp>

int main() {
  spdlog::set_level(spdlog::level::debug);

  tinycache::Server server(8080);

  server.run();
}
