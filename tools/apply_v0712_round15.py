from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text.replace("\r\n", "\n").rstrip() + "\n", encoding="utf-8")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected one {label}, found {count}")
    return text.replace(old, new, 1)


def patch_fused_cluster_test() -> None:
    text = read("tests/core_tests.cpp")
    marker = '''        static float minimum_semantic_support_clearance(
            const Environment& environment) noexcept
'''
    helper = '''        static float semantic_support_cluster_gap(
            const Environment& environment) noexcept
        {
            float left = 0.0f;
            float right = 0.0f;
            std::size_t left_count = 0u;
            std::size_t right_count = 0u;
            for (std::size_t index = 0; index < environment.particles_.size(); ++index)
            {
                if (environment.blueprint_.is_left_support_seed(index))
                {
                    left += environment.particles_[index].position.x;
                    ++left_count;
                }
                if (environment.blueprint_.is_right_support_seed(index))
                {
                    right += environment.particles_[index].position.x;
                    ++right_count;
                }
            }
            if (left_count == 0u || right_count == 0u)
                return 0.0f;
            return std::abs(right / static_cast<float>(right_count)
                - left / static_cast<float>(left_count));
        }

'''
    text = replace_once(text, marker, helper + marker,
        "semantic foot cluster gap helper")
    old = '''        require(sim::EnvironmentTestAccess::minimum_semantic_support_clearance(environment)
                > -0.005f,
            "a preset can retain fused semantic supports");'''
    new = '''        require(sim::EnvironmentTestAccess::semantic_support_cluster_gap(environment)
                > 0.18f,
            "a preset can retain fused left/right foot clusters");'''
    text = replace_once(text, old, new,
        "articulated fused-foot cluster assertion")
    write("tests/core_tests.cpp", text)


def patch_crouch_diagnostics() -> None:
    text = read("src/acceptance.cpp")
    text = replace_once(text,
        '''            std::uint32_t rejection_mask{};
            sim::InvalidMotion last_invalid{ sim::InvalidMotion::none };''',
        '''            std::uint32_t rejection_mask{};
            std::uint32_t supported_seeds{};
            std::uint32_t intact_seeds{};
            float lowest_upright{ 1.0f };
            int first_grounded_nonfoot{ -1 };
            sim::InvalidMotion last_invalid{ sim::InvalidMotion::none };''',
        "static crouch final-state diagnostics")

    old = '''                result.rejection_mask |= qualification.rejection_mask;
                result.last_invalid = environment.invalid_reason();
                result.shortest_duck = std::min(result.shortest_duck,
                    seed_longest_duck);'''
    new = '''                result.rejection_mask |= qualification.rejection_mask;
                result.supported_seeds += (environment.left_supported()
                    || environment.right_supported()) ? 1u : 0u;
                result.intact_seeds += environment.body_integrity_valid() ? 1u : 0u;
                result.lowest_upright = std::min(result.lowest_upright,
                    environment.uprightness());
                if (result.first_grounded_nonfoot < 0)
                {
                    const auto particles = environment.particles();
                    for (std::size_t node = 0; node < particles.size(); ++node)
                    {
                        if (particles[node].grounded
                            && !environment.blueprint().is_support_seed(node))
                        {
                            result.first_grounded_nonfoot = static_cast<int>(node);
                            break;
                        }
                    }
                }
                result.last_invalid = environment.invalid_reason();
                result.shortest_duck = std::min(result.shortest_duck,
                    seed_longest_duck);'''
    text = replace_once(text, old, new,
        "static crouch final-state diagnostic aggregation")

    old_detail = '''                << ", rejection=" << result.rejection_mask
                << ", invalid=" << static_cast<int>(result.last_invalid);'''
    new_detail = '''                << ", rejection=" << result.rejection_mask
                << ", supported=" << result.supported_seeds
                << ", intact=" << result.intact_seeds
                << ", upright=" << result.lowest_upright
                << ", grounded_nonfoot=" << result.first_grounded_nonfoot
                << ", invalid=" << static_cast<int>(result.last_invalid);'''
    text = replace_once(text, old_detail, new_detail,
        "static crouch diagnostic detail")
    write("src/acceptance.cpp", text)


def main() -> None:
    patch_fused_cluster_test()
    patch_crouch_diagnostics()
    Path(__file__).unlink()


if __name__ == "__main__":
    main()
