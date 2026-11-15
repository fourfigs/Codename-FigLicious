"""Translate legacy Seeed flags, then run the bundled esptool package."""

import importlib.util
from pathlib import Path
import sys


shim_dir = Path(__file__).resolve().parent
actual_init = next(
    Path(entry) / "esptool" / "__init__.py"
    for entry in sys.path
    if entry and Path(entry).resolve() != shim_dir.parent
    and (Path(entry) / "esptool" / "__init__.py").is_file()
)
spec = importlib.util.spec_from_file_location(
    "esptool", actual_init, submodule_search_locations=[str(actual_init.parent)]
)
esptool = importlib.util.module_from_spec(spec)
sys.modules["esptool"] = esptool
spec.loader.exec_module(esptool)

translations = {
    "--flash-mode": "--flash_mode",
    "--flash-freq": "--flash_freq",
    "--flash-size": "--flash_size",
}
sys.argv = [sys.argv[0], *(translations.get(arg, arg) for arg in sys.argv[1:])]
esptool._main()
