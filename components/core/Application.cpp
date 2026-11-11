#include "Application.hpp"
namespace app::core {

Application::Application(LifecycleManager& lifecycle_manager) : lifecycle_manager_(lifecycle_manager) {}

Application::~Application() = default;

void Application::run() {
  lifecycle_manager_.init();

  lifecycle_manager_.run();

  lifecycle_manager_.hibernate();
}

}  // namespace app::core