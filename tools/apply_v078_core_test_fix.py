from pathlib import Path
path = Path(__file__).resolve().parents[1] / 'tests/core_tests.cpp'
text = path.read_text(encoding='utf-8')
old = '        static_assert(sim::observation_count == 40);\n'
if text.count(old) != 1:
    raise RuntimeError(f'core_tests.cpp: expected one 40-channel assertion, found {text.count(old)}')
path.write_text(text.replace(old,
    '        static_assert(sim::observation_count == 50);\n', 1), encoding='utf-8')
Path(__file__).unlink()
