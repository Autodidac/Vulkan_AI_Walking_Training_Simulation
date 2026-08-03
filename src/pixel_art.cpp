#include "pixel_art.hpp"

#include <charconv>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <limits>
#include <string_view>

namespace runner::art
{
    namespace
    {
        constexpr std::size_t maximum_art_file_size = 4u * 1024u * 1024u;
        constexpr int maximum_dimension = 256;

        [[nodiscard]] constexpr bool ascii_space(unsigned char value) noexcept
        {
            return value == ' ' || value == '\t' || value == '\n'
                || value == '\r' || value == '\f' || value == '\v';
        }

        class P3Tokenizer
        {
        public:
            explicit P3Tokenizer(std::string_view bytes) noexcept
                : bytes_(bytes)
            {
                if (bytes_.size() >= 3u
                    && static_cast<unsigned char>(bytes_[0]) == 0xefu
                    && static_cast<unsigned char>(bytes_[1]) == 0xbbu
                    && static_cast<unsigned char>(bytes_[2]) == 0xbfu)
                    position_ = 3u;
            }

            [[nodiscard]] bool next(std::string_view& token) noexcept
            {
                skip_trivia();
                if (position_ >= bytes_.size())
                    return false;

                const std::size_t beginning = position_;
                while (position_ < bytes_.size())
                {
                    const unsigned char value =
                        static_cast<unsigned char>(bytes_[position_]);
                    if (ascii_space(value) || value == '#')
                        break;
                    ++position_;
                }
                token = bytes_.substr(beginning, position_ - beginning);
                return !token.empty();
            }

            [[nodiscard]] bool next_integer(int& value) noexcept
            {
                std::string_view token{};
                if (!next(token))
                    return false;
                const char* const beginning = token.data();
                const char* const ending = beginning + token.size();
                const auto [parsed_end, error] =
                    std::from_chars(beginning, ending, value);
                return error == std::errc{} && parsed_end == ending;
            }

            [[nodiscard]] bool exhausted() noexcept
            {
                skip_trivia();
                return position_ == bytes_.size();
            }

        private:
            void skip_trivia() noexcept
            {
                for (;;)
                {
                    while (position_ < bytes_.size()
                        && ascii_space(static_cast<unsigned char>(bytes_[position_])))
                        ++position_;

                    if (position_ >= bytes_.size() || bytes_[position_] != '#')
                        return;

                    while (position_ < bytes_.size()
                        && bytes_[position_] != '\n' && bytes_[position_] != '\r')
                        ++position_;
                }
            }

            std::string_view bytes_{};
            std::size_t position_{};
        };
    }

    bool load_p3_pixel_art(const std::filesystem::path& path,
        PixelArt& art, std::string& error)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input)
        {
            error = "Could not open Runner artwork: " + path.string();
            return false;
        }

        std::string bytes{ std::istreambuf_iterator<char>{ input }, {} };
        if (!input.eof() || bytes.empty() || bytes.size() > maximum_art_file_size)
        {
            error = "Runner artwork could not be read as a bounded file: "
                + path.string();
            return false;
        }

        P3Tokenizer tokenizer(bytes);
        std::string_view magic{};
        int width{};
        int height{};
        int maximum{};
        if (!tokenizer.next(magic) || magic != "P3"
            || !tokenizer.next_integer(width)
            || !tokenizer.next_integer(height)
            || !tokenizer.next_integer(maximum)
            || width <= 0 || height <= 0
            || width > maximum_dimension || height > maximum_dimension
            || maximum <= 0 || maximum > 65535)
        {
            error = "Runner artwork is not a valid bounded P3 PPM: "
                + path.string();
            return false;
        }

        const std::size_t pixel_count = static_cast<std::size_t>(width)
            * static_cast<std::size_t>(height);
        if (pixel_count > static_cast<std::size_t>(
                std::numeric_limits<int>::max() / 3))
        {
            error = "Runner artwork pixel count is out of range: " + path.string();
            return false;
        }

        PixelArt loaded{};
        loaded.width = width;
        loaded.height = height;
        loaded.pixels.reserve(pixel_count);
        const float inverse_maximum = 1.0f / static_cast<float>(maximum);
        for (std::size_t index = 0; index < pixel_count; ++index)
        {
            int red{};
            int green{};
            int blue{};
            if (!tokenizer.next_integer(red)
                || !tokenizer.next_integer(green)
                || !tokenizer.next_integer(blue)
                || red < 0 || green < 0 || blue < 0
                || red > maximum || green > maximum || blue > maximum)
            {
                error = "Runner artwork has incomplete or invalid pixel data: "
                    + path.string();
                return false;
            }
            loaded.pixels.push_back({
                static_cast<float>(red) * inverse_maximum,
                static_cast<float>(green) * inverse_maximum,
                static_cast<float>(blue) * inverse_maximum,
                1.0f
            });
        }

        if (!tokenizer.exhausted())
        {
            error = "Runner artwork contains unexpected trailing data: "
                + path.string();
            return false;
        }

        art = std::move(loaded);
        error.clear();
        return true;
    }
}
