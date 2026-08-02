#include "app.hpp"
#include "renderer.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>

#ifndef RUNNER_SHADER_DIRECTORY
#define RUNNER_SHADER_DIRECTORY "shaders"
#endif

#ifndef RUNNER_ASSET_DIRECTORY
#define RUNNER_ASSET_DIRECTORY "assets"
#endif

#ifndef RUNNER_VERSION
#define RUNNER_VERSION "development"
#endif

namespace
{
    [[nodiscard]] bool is_down(SDL_MouseButtonFlags buttons, SDL_MouseButtonFlags button) noexcept
    {
        return (buttons & button) != 0;
    }

    [[nodiscard]] bool wants_version(int argc, char** argv) noexcept
    {
        return argc > 1 && argv != nullptr && argv[1] != nullptr
            && std::string_view(argv[1]) == "--version";
    }

    [[nodiscard]] bool wants_vulkan_diagnostic(int argc, char** argv) noexcept
    {
        return argc > 1
            && argv != nullptr
            && argv[1] != nullptr
            && std::string_view(argv[1]) == "--diagnose-vulkan";
    }

    [[nodiscard]] bool wants_package_diagnostic(int argc, char** argv) noexcept
    {
        return argc > 1
            && argv != nullptr
            && argv[1] != nullptr
            && std::string_view(argv[1]) == "--diagnose-package";
    }

    [[nodiscard]] std::filesystem::path executable_directory()
    {
        const char* const base_path = SDL_GetBasePath();
        if (base_path == nullptr || *base_path == '\0')
            return std::filesystem::current_path();
        return std::filesystem::path{ std::u8string{ reinterpret_cast<const char8_t*>(base_path) } };
    }

    [[nodiscard]] bool validate_runtime_layout(const std::filesystem::path& base_directory,
        std::string& error)
    {
        const std::array required_files{
            std::filesystem::path{ RUNNER_SHADER_DIRECTORY } / "flat.vert.spv",
            std::filesystem::path{ RUNNER_SHADER_DIRECTORY } / "flat.frag.spv"
        };
        std::error_code filesystem_error{};
        for (const std::filesystem::path& relative : required_files)
        {
            const std::filesystem::path absolute = base_directory / relative;
            if (!std::filesystem::is_regular_file(absolute, filesystem_error))
            {
                error = "Missing packaged runtime file: " + absolute.string();
                if (filesystem_error)
                    error += " (" + filesystem_error.message() + ")";
                return false;
            }
            filesystem_error.clear();
        }

        const std::filesystem::path asset_directory =
            base_directory / RUNNER_ASSET_DIRECTORY;
        if (!std::filesystem::is_directory(asset_directory, filesystem_error))
        {
            error = "Missing packaged asset directory: " + asset_directory.string();
            if (filesystem_error)
                error += " (" + filesystem_error.message() + ")";
            return false;
        }
        error.clear();
        return true;
    }

    [[nodiscard]] bool is_headless_surface_error(std::string_view error) noexcept
    {
        return error.find("VK_KHR_surface") != std::string_view::npos
            || error.find("VK_KHR_win32_surface") != std::string_view::npos;
    }
}

int main(int argc, char** argv)
{
    if (wants_version(argc, argv))
    {
        std::printf("Runner %s\n", RUNNER_VERSION);
        return 0;
    }
    const bool package_diagnostic = wants_package_diagnostic(argc, argv);
    const bool diagnostic = wants_vulkan_diagnostic(argc, argv) || package_diagnostic;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    const std::filesystem::path base_directory = executable_directory();
    const std::filesystem::path shader_directory =
        base_directory / RUNNER_SHADER_DIRECTORY;
    const std::filesystem::path asset_directory =
        base_directory / RUNNER_ASSET_DIRECTORY;
    if (package_diagnostic)
    {
        std::string layout_error{};
        if (!validate_runtime_layout(base_directory, layout_error))
        {
            std::fprintf(stderr, "Runner package diagnostic failed: %s\n",
                layout_error.c_str());
            SDL_Quit();
            return 1;
        }
    }

    if (!SDL_Vulkan_LoadLibrary(nullptr))
    {
        const std::string vulkan_error = SDL_GetError();
        if (diagnostic && is_headless_surface_error(vulkan_error))
        {
            const char* video_driver = SDL_GetCurrentVideoDriver();
            std::printf(
                package_diagnostic
                    ? "Runner " RUNNER_VERSION " package diagnostic passed: runtime files present, backend enabled, video_driver=%s; the CI runner has no Vulkan presentation surface (%s)\n"
                    : "Runner " RUNNER_VERSION " SDL3 Vulkan diagnostic passed: backend enabled, video_driver=%s; the CI runner has no Vulkan presentation surface (%s)\n",
                video_driver != nullptr ? video_driver : "unknown",
                vulkan_error.c_str());
            SDL_Quit();
            return 0;
        }

        std::fprintf(
            stderr,
            "SDL3 Vulkan support unavailable: %s\n"
            "This build requires the vcpkg sdl3[vulkan] feature and a Vulkan-capable display driver.\n",
            vulkan_error.c_str());
        SDL_Quit();
        return 1;
    }

    Uint32 instance_extension_count{};
    const char* const* instance_extensions = SDL_Vulkan_GetInstanceExtensions(&instance_extension_count);
    if (instance_extensions == nullptr || instance_extension_count == 0)
    {
        std::fprintf(stderr, "SDL_Vulkan_GetInstanceExtensions failed: %s\n", SDL_GetError());
        SDL_Vulkan_UnloadLibrary();
        SDL_Quit();
        return 1;
    }

    if (diagnostic)
    {
        const char* video_driver = SDL_GetCurrentVideoDriver();
        std::printf(
            package_diagnostic
                ? "Runner " RUNNER_VERSION " package diagnostic passed: runtime files present, video_driver=%s, instance_extensions=%u\n"
                : "Runner " RUNNER_VERSION " SDL3 Vulkan diagnostic passed: video_driver=%s, instance_extensions=%u\n",
            video_driver != nullptr ? video_driver : "unknown",
            static_cast<unsigned int>(instance_extension_count));
        SDL_Vulkan_UnloadLibrary();
        SDL_Quit();
        return 0;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Runner v" RUNNER_VERSION " - Autonomous Physics Locomotion Trainer",
        1900,
        1180,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window == nullptr)
    {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Vulkan_UnloadLibrary();
        SDL_Quit();
        return 1;
    }
    SDL_SetWindowMinimumSize(window, 1280, 820);

    runner::render::VulkanRenderer renderer{};
    std::string error{};
    if (!renderer.initialize(window, shader_directory, error))
    {
        std::fprintf(stderr, "Vulkan initialization failed: %s\n", error.c_str());
        SDL_DestroyWindow(window);
        SDL_Vulkan_UnloadLibrary();
        SDL_Quit();
        return 1;
    }

    runner::Application application{};
    if (!application.initialize(asset_directory, error))
    {
        std::fprintf(stderr, "Application initialization failed: %s\n", error.c_str());
        renderer.shutdown();
        SDL_DestroyWindow(window);
        SDL_Vulkan_UnloadLibrary();
        SDL_Quit();
        return 1;
    }

    bool running = true;
    std::uint64_t previous_ticks = SDL_GetTicksNS();
    runner::Vec2 previous_mouse{};

    while (running && !application.wants_quit())
    {
        runner::InputState input{};
        SDL_Event event{};
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                input.left_pressed = input.left_pressed || event.button.button == SDL_BUTTON_LEFT;
                input.right_pressed = input.right_pressed || event.button.button == SDL_BUTTON_RIGHT;
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                input.left_released = input.left_released || event.button.button == SDL_BUTTON_LEFT;
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                input.wheel += event.wheel.y;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (!event.key.repeat)
                {
                    switch (event.key.scancode)
                    {
                    case SDL_SCANCODE_ESCAPE: input.escape_pressed = true; break;
                    case SDL_SCANCODE_SPACE: input.space_pressed = true; break;
                    case SDL_SCANCODE_DELETE: input.delete_pressed = true; break;
                    case SDL_SCANCODE_1: input.key_1_pressed = true; break;
                    case SDL_SCANCODE_2: input.key_2_pressed = true; break;
                    case SDL_SCANCODE_3: input.key_3_pressed = true; break;
                    case SDL_SCANCODE_S: input.save_pressed = true; break;
                    case SDL_SCANCODE_L: input.load_pressed = true; break;
                    case SDL_SCANCODE_R: input.reset_pressed = true; break;
                    default: break;
                    }
                }
                break;
            default:
                break;
            }
        }

        float mouse_x{};
        float mouse_y{};
        const SDL_MouseButtonFlags mouse_buttons = SDL_GetMouseState(&mouse_x, &mouse_y);
        int logical_width{};
        int logical_height{};
        int drawable_width{};
        int drawable_height{};
        SDL_GetWindowSize(window, &logical_width, &logical_height);
        SDL_GetWindowSizeInPixels(window, &drawable_width, &drawable_height);
        const float mouse_scale_x = logical_width > 0
            ? static_cast<float>(drawable_width) / static_cast<float>(logical_width) : 1.0f;
        const float mouse_scale_y = logical_height > 0
            ? static_cast<float>(drawable_height) / static_cast<float>(logical_height) : 1.0f;
        input.mouse = { mouse_x * mouse_scale_x, mouse_y * mouse_scale_y };
        input.mouse_delta = input.mouse - previous_mouse;
        previous_mouse = input.mouse;
        input.left_down = is_down(mouse_buttons, SDL_BUTTON_LMASK);
        const SDL_Keymod modifiers = SDL_GetModState();
        input.shift = (modifiers & SDL_KMOD_SHIFT) != 0;
        input.control = (modifiers & SDL_KMOD_CTRL) != 0;
        input.alt = (modifiers & SDL_KMOD_ALT) != 0;

        const std::uint64_t current_ticks = SDL_GetTicksNS();
        const float dt = std::clamp(static_cast<float>(current_ticks - previous_ticks) / 1'000'000'000.0f,
            1.0f / 240.0f, 1.0f / 15.0f);
        previous_ticks = current_ticks;

        SDL_GetWindowSizeInPixels(window, &drawable_width, &drawable_height);
        application.frame(input, dt, drawable_width, drawable_height);
        if (!renderer.render(application.vertices(), drawable_width, drawable_height, error))
        {
            std::fprintf(stderr, "Render failure: %s\n", error.c_str());
            running = false;
        }
    }

    renderer.wait_idle();
    renderer.shutdown();
    SDL_DestroyWindow(window);
    SDL_Vulkan_UnloadLibrary();
    SDL_Quit();
    return 0;
}
