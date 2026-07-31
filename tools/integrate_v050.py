from pathlib import Path


def replace_function(text: str, marker: str, replacement: str) -> str:
    start = text.find(marker)
    if start < 0:
        raise RuntimeError(f"missing function marker: {marker}")
    brace = text.find("{", start)
    if brace < 0:
        raise RuntimeError(f"missing function body: {marker}")
    depth = 0
    for index in range(brace, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[:start] + replacement.rstrip() + "\n" + text[index + 1:]
    raise RuntimeError(f"unterminated function: {marker}")


def integrate_ppo() -> None:
    path = Path("src/ppo_trainer.cpp")
    text = path.read_text(encoding="utf-8")

    if "initialize_parallel_workers();" not in text:
        constructor = r'''    PpoTrainer::PpoTrainer(const sim::CreatureBlueprint& blueprint, std::size_t environment_count,
            bool enable_rollout_workers)
            : blueprint_(blueprint), preview_(blueprint, 0xDEADBEEFu), policy_(0xC0FFEEu)
        {
            environment_count = std::clamp<std::size_t>(environment_count, 8, 256);
            const std::size_t hardware = std::max<std::size_t>(1, std::thread::hardware_concurrency());
            const std::size_t available = hardware > 2 ? hardware - 2 : hardware;
            rollout_worker_count_ = enable_rollout_workers
                ? std::clamp<std::size_t>(available, 1, std::min<std::size_t>(16, environment_count))
                : 0;

            environments_.reserve(environment_count);
            for (std::size_t index = 0; index < environment_count; ++index)
            {
                environments_.emplace_back(blueprint_, 0x1000u + index * 7919u);
                environments_.back().set_course(course_stage_, course_difficulty_);
            }
            preview_.set_course(course_stage_, course_difficulty_);
            episode_rewards_.assign(environment_count, 0.0f);
            episode_distances_.assign(environment_count, 0.0f);
            adam_.first_moment.assign(policy_.parameter_count(), 0.0f);
            adam_.second_moment.assign(policy_.parameter_count(), 0.0f);
            rollout_worker_totals_.resize(rollout_worker_count_);
            rollout_workers_.reserve(rollout_worker_count_);
            for (std::size_t worker = 0; worker < rollout_worker_count_; ++worker)
            {
                rollout_workers_.emplace_back([this, worker](std::stop_token stop_token)
                {
                    rollout_worker_main(worker, stop_token);
                });
            }
            set_cpu_mode(4);
            initialize_parallel_workers();
        }'''
        text = replace_function(text, "    PpoTrainer::PpoTrainer(", constructor)

    if "shutdown_parallel_workers();" not in text:
        destructor = r'''    PpoTrainer::~PpoTrainer()
        {
            shutdown_parallel_workers();
            for (std::jthread& worker : rollout_workers_)
                worker.request_stop();
            rollout_start_cv_.notify_all();
            rollout_workers_.clear();
        }'''
        text = replace_function(text, "    PpoTrainer::~PpoTrainer()", destructor)

    if "rollout_active_worker_count_;" not in text:
        rollout_worker = r'''    void PpoTrainer::rollout_worker_main(std::size_t worker_index, std::stop_token stop_token)
        {
            std::uint64_t observed_generation = 0;
            while (!stop_token.stop_requested())
            {
                std::uint64_t update_seed = 0;
                std::size_t active_workers = 1;
                {
                    std::unique_lock lock(rollout_mutex_);
                    rollout_start_cv_.wait(lock, stop_token, [this, observed_generation]
                    {
                        return rollout_generation_ != observed_generation;
                    });
                    if (stop_token.stop_requested())
                        return;
                    observed_generation = rollout_generation_;
                    update_seed = rollout_update_seed_;
                    active_workers = rollout_active_worker_count_;
                }

                RolloutTotals totals{};
                if (worker_index < active_workers)
                    totals = collect_rollout_partition(worker_index, active_workers, update_seed);
                {
                    std::scoped_lock lock(rollout_mutex_);
                    rollout_worker_totals_[worker_index] = totals;
                    ++rollout_completed_;
                }
                rollout_done_cv_.notify_one();
            }
        }'''
        text = replace_function(text, "    void PpoTrainer::rollout_worker_main(", rollout_worker)

    if "rollout_active_worker_count_ =" not in text:
        old = """                rollout_completed_ = 0;
                rollout_update_seed_ = update_seed;
                ++rollout_generation_;"""
        new = """                rollout_completed_ = 0;
                rollout_update_seed_ = update_seed;
                rollout_active_worker_count_ = std::min(active_worker_count_, rollout_worker_count_);
                ++rollout_generation_;"""
        if old not in text:
            raise RuntimeError("missing rollout launch block")
        text = text.replace(old, new, 1)

    if "parallel_accumulate_batch(" not in text:
        update_policy = r'''    void PpoTrainer::update_policy()
        {
            constexpr std::size_t epochs = 4;
            constexpr std::size_t minibatch_size = 256;
            constexpr float clip_range = 0.20f;
            constexpr float value_coefficient = 0.50f;
            constexpr float entropy_coefficient = 0.0020f;
            constexpr float max_gradient_norm = 0.70f;

            std::vector<std::size_t> indices(rollout_.size());
            std::iota(indices.begin(), indices.end(), 0);
            float total_policy_loss = 0.0f;
            float total_value_loss = 0.0f;
            float total_entropy = 0.0f;
            std::size_t sample_count = 0;

            for (std::size_t epoch = 0; epoch < epochs; ++epoch)
            {
                for (std::size_t index = indices.size(); index > 1; --index)
                {
                    const std::size_t other = static_cast<std::size_t>(
                        random_uniform() * static_cast<float>(index));
                    std::swap(indices[index - 1], indices[std::min(other, index - 1)]);
                }
                for (std::size_t begin = 0; begin < indices.size(); begin += minibatch_size)
                {
                    const std::size_t end = std::min(indices.size(), begin + minibatch_size);
                    float batch_policy_loss = 0.0f;
                    float batch_value_loss = 0.0f;
                    float batch_entropy = 0.0f;
                    parallel_accumulate_batch(
                        indices, begin, end, clip_range, value_coefficient, entropy_coefficient,
                        batch_policy_loss, batch_value_loss, batch_entropy);

                    const float inverse_batch = 1.0f / static_cast<float>(end - begin);
                    float norm_squared = 0.0f;
                    for (const float gradient : policy_.gradients())
                    {
                        const float scaled = gradient * inverse_batch;
                        norm_squared += scaled * scaled;
                    }
                    const float norm = std::sqrt(norm_squared);
                    const float clip_scale = norm > max_gradient_norm
                        ? max_gradient_norm / norm
                        : 1.0f;
                    const float learning_rate = metrics_.learning_rate
                        * std::max(0.10f, 1.0f - static_cast<float>(metrics_.update) / 5000.0f);
                    apply_adam(learning_rate, inverse_batch * clip_scale);
                    total_policy_loss += batch_policy_loss;
                    total_value_loss += batch_value_loss;
                    total_entropy += batch_entropy;
                    sample_count += end - begin;
                }
            }
            const float inverse_samples = sample_count > 0
                ? 1.0f / static_cast<float>(sample_count)
                : 0.0f;
            metrics_.policy_loss = total_policy_loss * inverse_samples;
            metrics_.value_loss = total_value_loss * inverse_samples;
            metrics_.entropy = total_entropy * inverse_samples;
        }'''
        text = replace_function(text, "    void PpoTrainer::update_policy()", update_policy)

    old_evaluation_marker = "    void PpoTrainer::evaluate_policy()"
    evaluation_start = text.find(old_evaluation_marker)
    if evaluation_start < 0:
        raise RuntimeError("missing evaluate_policy")
    evaluation_end = text.find("    bool PpoTrainer::restore_best_policy()", evaluation_start)
    if evaluation_end < 0:
        raise RuntimeError("missing evaluate_policy end")
    if "parallel_evaluate_policy();" not in text[evaluation_start:evaluation_end]:
        evaluation = r'''    void PpoTrainer::evaluate_policy()
        {
            parallel_evaluate_policy();
        }'''
        text = replace_function(text, old_evaluation_marker, evaluation)

    path.write_text(text, encoding="utf-8")


def integrate_runtime() -> None:
    path = Path("src/autonomy_runtime.cpp")
    text = path.read_text(encoding="utf-8")
    if "worker_.set_cpu_mode(" not in text:
        old = """        {
            std::scoped_lock lock(worker_mutex_);
            worker_.train_one_update();
        }"""
        new = """        {
            std::scoped_lock lock(worker_mutex_);
            worker_.set_cpu_mode(updates_per_cycle_.load(std::memory_order_relaxed));
            worker_.train_one_update();
        }"""
        if old not in text:
            raise RuntimeError("missing trainer update block")
        text = text.replace(old, new, 1)
    path.write_text(text, encoding="utf-8")


def integrate_app() -> None:
    path = Path("src/app.cpp")
    text = path.read_text(encoding="utf-8")
    text = text.replace("epochrunner-v041-autosave.eppo", "epochrunner-v050-autosave.eppo")
    text = text.replace("epochrunner-v041-evolved.epochrig", "epochrunner-v050-evolved.epochrig")
    text = text.replace("epochrunner-v041-autonomy.state", "epochrunner-v050-autonomy.state")
    text = text.replace(
        '            set_status(std::format("{} - CONTROLLER TRANSFERRED AND RECALIBRATING", reason));',
        '            set_status(std::format("{} - QUEUED; TRAINER RECALIBRATES WITHOUT BLOCKING", reason));')

    stale_start = text.find("            if (!dragging_node && !rig_edit_pending")
    if stale_start >= 0:
        stale_end = text.find("            canvas.clear();", stale_start)
        if stale_end < 0:
            raise RuntimeError("stale rig overwrite block has no end")
        text = text[:stale_start] + text[stale_end:]

    telemetry_marker = '            add_text(canvas, cursor, std::format("TRAIN {:.2f} UPDATES/S'
    if telemetry_marker not in text:
        anchor = '''            add_text(canvas, cursor, std::format("{} CPU ROLLOUT THREADS / {} ENVIRONMENTS",
                autonomy.rollout_threads, autonomy.environment_count), 1.12f, muted);
            cursor.y += 23.0f;
'''
        telemetry = anchor + '''            add_text(canvas, cursor, std::format("TRAIN {:.2f} UPDATES/S   MODE {}   QUEUED {}",
                autonomy.updates_per_second, autonomy.speed_mode, autonomy.pending_commands), 1.10f, white);
            cursor.y += 23.0f;
            add_text(canvas, cursor, autonomy.worker_busy ? "TRAINER BUSY" : "TRAINER IDLE", 1.04f,
                autonomy.worker_busy ? yellow : green);
            cursor.y += 23.0f;
'''
        if anchor not in text:
            raise RuntimeError("missing UI telemetry anchor")
        text = text.replace(anchor, telemetry, 1)

    path.write_text(text, encoding="utf-8")


def integrate_tests() -> None:
    path = Path("tests/core_tests.cpp")
    text = path.read_text(encoding="utf-8")
    text = text.replace("epochrunner-v041-core-test.eppo", "epochrunner-v050-core-test.eppo")

    if "MAX CPU does not enable the full persistent worker pool" not in text:
        anchor = '    require(trainer.rollout_worker_count() >= 1, "parallel rollout worker count is invalid");\n'
        addition = anchor + '''    trainer.set_cpu_mode(1);
    const std::size_t normal_workers = trainer.rollout_worker_count();
    trainer.set_cpu_mode(2);
    const std::size_t faster_workers = trainer.rollout_worker_count();
    trainer.set_cpu_mode(4);
    const std::size_t maximum_workers = trainer.rollout_worker_count();
    require(normal_workers <= faster_workers && faster_workers <= maximum_workers,
        "speed modes do not increase persistent worker budget");
    require(maximum_workers == trainer.maximum_worker_count(),
        "MAX CPU does not enable the full persistent worker pool");
'''
        if anchor not in text:
            raise RuntimeError("missing worker test anchor")
        text = text.replace(anchor, addition, 1)

    if "hip edit blocked the caller on active training work" not in text:
        anchor = '        require(autonomous.metrics().update >= 1, "coroutine background worker did not process requested update");\n'
        addition = anchor + '''
        autonomous.set_updates_per_cycle(4);
        autonomous.set_background_enabled(true);
        sim::CreatureBlueprint edited = humanoid;
        edited.nodes[1].x += 0.01f;
        edited.rebuild_rest_lengths();
        const auto command_started = std::chrono::steady_clock::now();
        autonomous.set_blueprint(edited, true);
        const auto command_elapsed = std::chrono::steady_clock::now() - command_started;
        require(command_elapsed < std::chrono::milliseconds(20),
            "hip edit blocked the caller on active training work");
        for (int attempt = 0; attempt < 1200 && autonomous.rig_signature() != edited.signature(); ++attempt)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            autonomous.synchronize();
        }
        require(autonomous.rig_signature() == edited.signature(),
            "queued hip edit was not eventually published");
        autonomous.set_updates_per_cycle(1);
        require(autonomous.updates_per_cycle() == 1, "NORMAL speed mode did not latch");
        autonomous.set_updates_per_cycle(2);
        require(autonomous.updates_per_cycle() == 2, "FASTER speed mode did not latch");
        autonomous.set_updates_per_cycle(4);
        require(autonomous.updates_per_cycle() == 4, "MAX CPU speed mode did not latch");
'''
        if anchor not in text:
            raise RuntimeError("missing autonomous test anchor")
        text = text.replace(anchor, addition, 1)

    text = text.replace(
        'std::cout << "EpochRunner v0.4.1 autonomous gait and curriculum tests passed\\n";',
        'std::cout << "EpochRunner v0.5.0 concurrency, gait, and rig-edit tests passed\\n";')
    path.write_text(text, encoding="utf-8")


def integrate_metadata() -> None:
    manifest = Path("vcpkg.json")
    text = manifest.read_text(encoding="utf-8")
    manifest.write_text(text.replace('"version-semver": "0.4.1"', '"version-semver": "0.5.0"'), encoding="utf-8")

    readme = Path("README.md")
    text = readme.read_text(encoding="utf-8")
    text = text.replace("Version 0.4.1", "Version 0.5.0")
    text = text.replace("Version 0.4", "Version 0.5")
    text = text.replace(
        "- A coroutine-driven background supervisor handles training cycles, deterministic evaluation, curriculum transitions, autosaves, rollback, and bounded rig evolution.",
        "- A C++23 coroutine supervisor stages command application, parallel PPO work, curriculum handling, immutable publication, and speed throttling.\n- Persistent workers now handle rollout simulation, PPO minibatch gradients, and deterministic policy evaluation.\n- Rig edits are coalesced through a non-blocking command queue and never wait on a training update.")
    text = text.replace(
        "- Rollouts are divided across up to 16 CPU workers while reserving CPU capacity for the application and Vulkan presentation.",
        "- NORMAL, FASTER, and MAX CPU select increasing persistent worker budgets and duty cycles while reserving capacity for Vulkan presentation.")
    readme.write_text(text, encoding="utf-8")


integrate_ppo()
integrate_runtime()
integrate_app()
integrate_tests()
integrate_metadata()

for obsolete in (
    ".github/workflows/v050-release.yml",
    ".github/workflows/integrate-v050-concurrency.yml",
    ".github/workflows/integrate-v050-concurrency-retry.yml",
    "tools/integrate_v050.py",
):
    Path(obsolete).unlink(missing_ok=True)
