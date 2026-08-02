#include <shaderc/shaderc.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <span>
#include <string>
#include <system_error>
#include <vector>

namespace runner::tools
{
    [[nodiscard]] shaderc_shader_kind shader_kind_for(const std::filesystem::path& source_path)
    {
        const std::string extension = source_path.extension().string();
        if (extension == ".vert")
            return shaderc_vertex_shader;
        if (extension == ".frag")
            return shaderc_fragment_shader;
        if (extension == ".comp")
            return shaderc_compute_shader;

        return shaderc_glsl_infer_from_source;
    }

    [[nodiscard]] int compile_shader(std::span<char*> arguments)
    {
        if (arguments.size() != 3U)
        {
            std::cerr << "Usage: RunnerShaderCompiler <input.glsl> <output.spv>\n";
            return 2;
        }

        const std::filesystem::path source_path{ arguments[1] };
        const std::filesystem::path output_path{ arguments[2] };
        const shaderc_shader_kind shader_kind = shader_kind_for(source_path);
        if (shader_kind == shaderc_glsl_infer_from_source)
        {
            std::cerr << "Unsupported shader extension: " << source_path.extension().string() << '\n';
            return 3;
        }

        std::ifstream source_stream{ source_path, std::ios::binary };
        if (!source_stream)
        {
            std::cerr << "Failed to open shader source: " << source_path.string() << '\n';
            return 4;
        }

        const std::string source{
            std::istreambuf_iterator<char>{ source_stream },
            std::istreambuf_iterator<char>{}
        };
        if (!source_stream.eof() && source_stream.fail())
        {
            std::cerr << "Failed to read shader source: " << source_path.string() << '\n';
            return 5;
        }

        shaderc::Compiler compiler;
        shaderc::CompileOptions options;
        options.SetSourceLanguage(shaderc_source_language_glsl);
        options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
        options.SetTargetSpirv(shaderc_spirv_version_1_6);
        options.SetOptimizationLevel(shaderc_optimization_level_performance);
        options.SetWarningsAsErrors();

        const std::string source_name = source_path.generic_string();
        const shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(
            source,
            shader_kind,
            source_name.c_str(),
            "main",
            options);

        if (result.GetCompilationStatus() != shaderc_compilation_status_success)
        {
            std::cerr << result.GetErrorMessage();
            return 6;
        }

        const std::vector<std::uint32_t> spirv{ result.cbegin(), result.cend() };
        if (spirv.size() > static_cast<std::size_t>((std::numeric_limits<std::streamsize>::max)()) / sizeof(std::uint32_t))
        {
            std::cerr << "Compiled shader is too large: " << source_path.string() << '\n';
            return 7;
        }

        if (output_path.has_parent_path())
        {
            std::error_code error;
            std::filesystem::create_directories(output_path.parent_path(), error);
            if (error)
            {
                std::cerr << "Failed to create shader output directory: " << error.message() << '\n';
                return 8;
            }
        }

        std::ofstream output_stream{ output_path, std::ios::binary | std::ios::trunc };
        if (!output_stream)
        {
            std::cerr << "Failed to open shader output: " << output_path.string() << '\n';
            return 9;
        }

        const std::size_t byte_count = spirv.size() * sizeof(std::uint32_t);
        output_stream.write(
            reinterpret_cast<const char*>(spirv.data()),
            static_cast<std::streamsize>(byte_count));
        if (!output_stream)
        {
            std::cerr << "Failed to write shader output: " << output_path.string() << '\n';
            return 10;
        }

        return 0;
    }
}

int main(int argc, char** argv)
{
    return runner::tools::compile_shader(
        std::span<char*>{ argv, static_cast<std::size_t>(argc) });
}
