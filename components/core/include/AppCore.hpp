#pragma once

#include <cstdint>
#include <string>
#include <variant>

#include "AppOrchestrator.hpp"

namespace app::core {

class AppCore {
 private:
  AppOrchestrator& orchestrator_;

 public:
  AppCore(AppOrchestrator& orchestrator);
  ~AppCore();

  void run();
};

}  // namespace app::core