from pathlib import Path

root = Path(__file__).resolve().parents[1]
path = root / "src/simulation.cpp"
text = path.read_text(encoding="utf-8")
old = '''            if (particle.position.y < minimum_y)
            {
                particle.position.y = minimum_y;
                particle.grounded = true;'''
new = '''            // A constraint iteration can leave a perfectly resting rigid foot
            // exactly on the contact plane. Preserve support within a tiny solver
            // tolerance instead of clearing grounded until it penetrates again.
            if (particle.position.y <= minimum_y + 0.0025f)
            {
                particle.position.y = minimum_y;
                particle.grounded = true;'''
if new in text:
    print("resting contact tolerance already materialized")
elif old in text:
    path.write_text(text.replace(old, new, 1), encoding="utf-8", newline="\n")
    print("materialized resting rigid-foot contact tolerance")
else:
    raise RuntimeError("solve_ground contact predicate not found")
