from pathlib import Path
R=Path(__file__).resolve().parents[1]
def x(p,o,n):
 t=(R/p).read_text(); c=t.count(o)
 if c!=1: raise RuntimeError(f'{p}: {c} matches')
 (R/p).write_text(t.replace(o,n,1))
x('src/simulation.hpp','#include "math.hpp"\n','#include "math.hpp"\n#include "deformable_terrain.hpp"\n')
x('src/simulation.hpp','    inline constexpr std::size_t observation_count = 40;\n','    inline constexpr std::size_t observation_count = 50;\n')
x('src/simulation.hpp','    inline constexpr std::size_t course_stage_count = 8;\n\n','''    inline constexpr std::size_t course_stage_count = 8;

    [[nodiscard]] inline bool stage_uses_deformable_terrain(CourseStage stage) noexcept
    {
        return stage == CourseStage::uneven
            || stage == CourseStage::crouch_walk
            || stage == CourseStage::hurdles
            || stage == CourseStage::moving_hazards;
    }

''')
x('src/simulation.hpp','''        press_penetration,
        duck_body_contact
    };
''','''        press_penetration,
        duck_body_contact,
        buried_no_escape
    };
''')
x('src/simulation.hpp','''        case InvalidMotion::press_penetration: return "DUCK PRESS PENETRATION";
        case InvalidMotion::duck_body_contact: return "DUCK CONTACT - FEET ONLY";
        }
''','''        case InvalidMotion::press_penetration: return "DUCK PRESS PENETRATION";
        case InvalidMotion::duck_body_contact: return "DUCK CONTACT - FEET ONLY";
        case InvalidMotion::buried_no_escape: return "BURIED / NO ESCAPE SPACE";
        }
''')
x('src/simulation.hpp','''    struct DistanceConstraint
    {
''','''    enum class MaterialKind : std::uint8_t
    {
        sand,
        rock,
        debris
    };

    struct MaterialParticle
    {
        MaterialKind kind{ MaterialKind::sand };
        Vec2 position{};
        Vec2 velocity{};
        float radius{ 0.08f };
        float density{ 0.45f };
        bool active{ true };
    };

    struct DistanceConstraint
    {
''')
x('src/simulation.hpp','''        [[nodiscard]] std::span<const CourseFeature> course_features() const noexcept { return course_features_; }
        [[nodiscard]] CourseStage course_stage() const noexcept { return course_stage_; }
''','''        [[nodiscard]] std::span<const CourseFeature> course_features() const noexcept { return course_features_; }
        [[nodiscard]] std::span<const MaterialParticle> material_particles() const noexcept
        {
            return material_particles_;
        }
        [[nodiscard]] CourseStage course_stage() const noexcept { return course_stage_; }
''')
x('src/simulation.hpp','''        [[nodiscard]] float ground_height() const noexcept { return 0.0f; }
        [[nodiscard]] float ground_height_at(float x) const noexcept;
''','''        [[nodiscard]] float ground_height() const noexcept { return 0.0f; }
        [[nodiscard]] float ground_height_at(float x) const noexcept;
        [[nodiscard]] float terrain_firmness_at(float x) const noexcept;
        [[nodiscard]] float terrain_looseness_at(float x) const noexcept;
        [[nodiscard]] float burial_depth() const noexcept { return burial_depth_; }
        [[nodiscard]] float free_space_direction() const noexcept { return free_space_direction_; }
        [[nodiscard]] Vec2 incoming_material_velocity() const noexcept { return incoming_material_velocity_; }
        [[nodiscard]] float incoming_time_to_impact() const noexcept { return incoming_time_to_impact_; }
        [[nodiscard]] float incoming_material_density() const noexcept { return incoming_material_density_; }
        [[nodiscard]] std::uint32_t material_event_count() const noexcept { return material_event_sequence_; }
        [[nodiscard]] std::uint8_t obstruction_mask() const noexcept { return obstruction_mask_; }
''')
x('src/simulation.hpp','''        void solve_ground(float dt) noexcept;
        void solve_course() noexcept;
        void rebuild_course_features() noexcept;
''','''        void solve_ground(float dt) noexcept;
        void solve_course() noexcept;
        void apply_support_pressure(float dt) noexcept;
        void update_materials(float dt) noexcept;
        void append_material_features() noexcept;
        void update_material_metrics(float dt) noexcept;
        void rebuild_course_features() noexcept;
''')
x('src/simulation.hpp','''        CreatureBlueprint blueprint_{};
        std::vector<Particle> particles_{};
        std::vector<CourseFeature> course_features_{};
''','''        CreatureBlueprint blueprint_{};
        std::vector<Particle> particles_{};
        std::vector<CourseFeature> course_features_{};
        DeformableTerrain terrain_{};
        std::vector<MaterialParticle> material_particles_{};
''')
x('src/simulation.hpp','''        std::uint32_t recovery_events_{};
        std::uint32_t recovery_successes_{};
        InvalidMotion invalid_reason_{ InvalidMotion::none };
''','''        std::uint32_t recovery_events_{};
        std::uint32_t recovery_successes_{};
        float next_material_event_seconds_{ 1.50f };
        std::uint32_t material_event_sequence_{};
        float terrain_firmness_{ 1.0f };
        float terrain_looseness_{};
        float burial_depth_{};
        float previous_burial_depth_{};
        float buried_no_escape_seconds_{};
        float free_space_direction_{};
        Vec2 incoming_material_velocity_{};
        float incoming_time_to_impact_{ 10.0f };
        float incoming_material_density_{};
        std::uint8_t obstruction_mask_{};
        InvalidMotion invalid_reason_{ InvalidMotion::none };
''')
x('src/ppo.hpp',"    inline constexpr std::uint32_t training_semantics_version = 0x0007'0700u;\n","    inline constexpr std::uint32_t training_semantics_version = 0x0007'0800u;\n")
x('src/ppo.hpp','        static_assert(sim::observation_count == 40);\n','        static_assert(sim::observation_count == 50);\n')
x('src/autonomy_persistence.cpp','            output << "RUNAUTONOMY 12\\n";\n','            output << "RUNAUTONOMY 13\\n";\n')
x('src/autonomy_persistence.cpp','            if (!input || magic != "RUNAUTONOMY" || version != 12\n','            if (!input || magic != "RUNAUTONOMY" || version != 13\n')
Path(__file__).unlink()
