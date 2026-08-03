from pathlib import Path

path = Path('src/ppo.hpp')
text = path.read_text(encoding='utf-8')
text = text.replace('const sim::Vec2 reference =', 'const Vec2 reference =')
text = text.replace('const sim::Vec2 driven =', 'const Vec2 driven =')
text = text.replace('sim::Vec2 compact =', 'Vec2 compact =')
text = text.replace('sim::length(reference)', 'length(reference)')
text = text.replace('sim::length(driven)', 'length(driven)')
text = text.replace('sim::signed_angle(reference, compact)', 'signed_angle(reference, compact)')
path.write_text(text, encoding='utf-8', newline='\n')
Path(__file__).unlink()
print('fixed v0.7.7 math namespace')
