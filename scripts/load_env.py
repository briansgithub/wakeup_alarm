"""Load gitignored .env values into the PlatformIO build."""
from pathlib import Path

Import("env")  # type: ignore[name-defined]


def load_dotenv(path: Path) -> None:
    if not path.is_file():
        return
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        name, value = line.split("=", 1)
        name = name.strip()
        value = value.strip().strip('"').strip("'")
        env["ENV"].setdefault(name, value)


project_dir = Path(env["PROJECT_DIR"])
load_dotenv(project_dir / ".env")

ar_key = env["ENV"].get("WAKEUP_AUTOREMOTE_KEY", "")
if ar_key:
    env.Append(CPPDEFINES=[("WAKEUP_AUTOREMOTE_KEY", env.StringifyMacro(ar_key))])
