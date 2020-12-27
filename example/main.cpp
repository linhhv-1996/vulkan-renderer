#include "inexor/vulkan-renderer/application.hpp"

#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <taskflow/taskflow.hpp>

#include <memory>

int main(int argc, char *argv[]) {

    // Setup logging.
    auto setup_logging = []() {
        spdlog::init_thread_pool(8192, 2);

        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("vulkan-renderer.log", true);
        auto vulkan_renderer_log =
            std::make_shared<spdlog::async_logger>("vulkan-renderer", spdlog::sinks_init_list{console_sink, file_sink},
                                                   spdlog::thread_pool(), spdlog::async_overflow_policy::block);
        vulkan_renderer_log->set_level(spdlog::level::trace);
        vulkan_renderer_log->set_pattern("%Y-%m-%d %T.%f %^%l%$ %5t [%-10n] %v");
        vulkan_renderer_log->flush_on(spdlog::level::debug); // TODO: as long as we don't have a flush on crash

        spdlog::set_default_logger(vulkan_renderer_log);

        spdlog::debug("Inexor vulkan-renderer, BUILD " + std::string(__DATE__) + ", " + __TIME__);
        spdlog::debug("Parsing command line arguments.");
    };

    std::unique_ptr<inexor::vulkan_renderer::Application> renderer;

    auto setup_renderer = [&renderer, argc, argv]() {
        renderer = std::make_unique<inexor::vulkan_renderer::Application>(argc, argv);
    };

    tf::Executor executor;
    tf::Taskflow taskflow;

    // Create list of tasks: What must be done?
    auto [A, B, C, D] = taskflow.emplace(
        setup_logging, setup_renderer, [&renderer]() { renderer->run(); },
        [&renderer]() { renderer->calculate_memory_budget(); });

    // Declare dependencies.
    A.precede(B, C, D); // A runs before B and C.
    B.precede(C, D);    // B runs before C and D.
    D.succeed(C);       // D runs after C.

    taskflow.dump(std::cout);

    executor.run(taskflow).wait();
}
