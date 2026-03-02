Import("env")

from pathlib import Path

project_dir = Path(env.subst("$PROJECT_DIR"))
data_dir = project_dir / "data"

# Ensure PlatformIO always finds a valid source directory for SPIFFS
# when running: pio run --target buildfs/uploadfs
data_dir.mkdir(parents=True, exist_ok=True)
