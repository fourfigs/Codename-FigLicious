Import("env")

from pathlib import Path


# Seeed's PlatformIO builder uses hyphenated options while its bundled esptool
# exposes underscore spellings. Put the small compatibility launcher first.
launcher_path = Path(env.subst("$PROJECT_DIR")) / "scripts" / "esptool_compat"
env.PrependENVPath("PYTHONPATH", str(launcher_path))
