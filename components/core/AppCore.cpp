#include "AppCore.hpp"
namespace app::core {

AppCore::AppCore(AppOrchestrator& orchestrator) : orchestrator_(orchestrator) {}

AppCore::~AppCore() = default;

void AppCore::run() {
  // todo
}

}  // namespace app::core