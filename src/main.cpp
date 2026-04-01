#include <spdlog/cfg/env.h>
#include <spdlog/spdlog.h>
#include <server.hpp>

int main() {
#ifdef NDEBUG
  spdlog::set_level(spdlog::level::warn);
#else
  spdlog::set_level(spdlog::level::debug);
#endif
  spdlog::cfg::load_env_levels();

  auto config = tinycache::get_config();

  tinycache::Server server(config);

  server.run();
}
