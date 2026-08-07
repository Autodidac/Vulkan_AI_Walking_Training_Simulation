#!/usr/bin/env python3
from pathlib import Path

path = Path('src/app.cpp')
text = path.read_text(encoding='utf-8')
old = '''            const ui_layout::Box layout_content = ui_layout::content_box(
                static_cast<float>(width), static_cast<float>(height));
            const Rect content{ { layout_content.x, layout_content.y },
                { layout_content.width, layout_content.height } };
'''
new = '''            const ui_layout::Box layout_content = ui_layout::content_box(
                static_cast<float>(width), static_cast<float>(height));
'''
if text.count(old) != 1:
    raise SystemExit(f'unused content rectangle matches: {text.count(old)}')
path.write_text(text.replace(old, new, 1), encoding='utf-8', newline='\n')

path = Path('tools/repository_audit.cmake')
audit = path.read_text(encoding='utf-8')
marker = '        tools/add_v0722_frame_diagnostic.py\n'
if marker in audit and '        tools/fix_v0722_validation.py\n' not in audit:
    audit = audit.replace(marker,
        marker + '        tools/fix_v0722_validation.py\n', 1)
path.write_text(audit, encoding='utf-8', newline='\n')
print('Runner v0.7.22 validation repair applied')
