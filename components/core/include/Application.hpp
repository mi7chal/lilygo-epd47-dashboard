#pragma once

#include <cstdint>
#include <string>
#include <variant>

#include "LifecycleManager.hpp"

namespace app::core {

/**
 * @brief App entry point, base app class
 *
 */
class Application {
 private:
  LifecycleManager& lifecycle_manager_;

 public:
  Application(LifecycleManager& lifecycle_manager);
  ~Application();

  /**
   * @brief Runs the app
   *
   */
  void run();
};

}  // namespace app::core