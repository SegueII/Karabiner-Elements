#include "components_manager.hpp"
#include "constants.hpp"
#include "filesystem.hpp"
#include "grabber_alerts_monitor.hpp"
#include "karabiner_version.h"
#include "logger.hpp"
#include "migration.hpp"
#include "process_utility.hpp"
#include "spdlog_utility.hpp"
#include "thread_utility.hpp"
#include "update_utility.hpp"
#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>

namespace krbn {
class karabiner_console_user_server final {
public:
  karabiner_console_user_server(void) {
    {
      auto log_directory = constants::get_user_log_directory();
      if (!log_directory.empty()) {
        filesystem::create_directory_with_intermediate_directories(log_directory, 0700);

        if (filesystem::is_directory(log_directory)) {
          std::string log_file_path = log_directory + "/console_user_server.log";
          auto l = spdlog::rotating_logger_mt<spdlog::async_factory>("console_user_server",
                                                                     log_file_path.c_str(),
                                                                     256 * 1024,
                                                                     3);
          l->flush_on(spdlog::level::info);
          l->set_pattern(spdlog_utility::get_pattern());
          logger::set_logger(l);
        }
      }
    }

    logger::get_logger().info("version {0}", karabiner_version);

    if (!process_utility::lock_single_application_with_user_pid_file("karabiner_console_user_server.pid")) {
      std::string message("Exit since another process is running.");
      logger::get_logger().info(message);
      std::cerr << message << std::endl;
      exit(0);
    }

    // ========================================

    grabber_alerts_monitor_ = std::make_unique<grabber_alerts_monitor>(constants::get_grabber_alerts_json_file_path());

    grabber_alerts_monitor_->alerts_changed.connect([](auto&& alerts) {
      logger::get_logger().info("karabiner_grabber_alerts.json is updated.");
      if (!alerts.empty()) {
        application_launcher::launch_preferences();
      }
    });

    grabber_alerts_monitor_->start();

    // ========================================

    migration::migrate_v1();

    // ========================================

    krbn::filesystem::create_directory_with_intermediate_directories(constants::get_user_configuration_directory(), 0700);
    krbn::filesystem::create_directory_with_intermediate_directories(constants::get_user_complex_modifications_assets_directory(), 0700);

    components_manager_ = std::make_unique<components_manager>();
  }

  ~karabiner_console_user_server(void) {
    components_manager_ = nullptr;
    grabber_alerts_monitor_ = nullptr;
  }

private:
  std::unique_ptr<grabber_alerts_monitor> grabber_alerts_monitor_;
  std::unique_ptr<components_manager> components_manager_;
};
} // namespace krbn

int main(int argc, const char* argv[]) {
  signal(SIGUSR1, SIG_IGN);
  signal(SIGUSR2, SIG_IGN);
  krbn::thread_utility::register_main_thread();

  krbn::karabiner_console_user_server karabiner_console_user_server;

  krbn::update_utility::check_for_updates_on_startup();

  CFRunLoopRun();

  return 0;
}
