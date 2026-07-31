from pathlib import Path


path = Path("src/app.cpp")
text = path.read_text(encoding="utf-8")
text = text.replace("""if (text[cursor] == '
')""", r"""if (text[cursor] == '\n')""")
text = text.replace("""text.find_first_of(" 
", cursor)""", r"""text.find_first_of(" \n", cursor)""")
if "text.find_first_of(\" \\n\", cursor)" not in text:
    raise SystemExit("wrapped-text newline repair did not apply")
path.write_text(text, encoding="utf-8")
