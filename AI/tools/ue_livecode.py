"""
ue_livecode.py — Live Coding compile in the RUNNING editor, and wait for the result.

  python AI/tools/ue_livecode.py

Triggers LiveCoding.Compile through ue_remote (remote Python), then watches
Game/Saved/Logs/PartyButtons.log for the outcome. Exit 0 on success (or "no
changes"), 1 on failure, 2 on timeout / no editor. Compiler errors are printed.

Caveat worth knowing before relying on it: a class or struct ADDED by Live
Coding is fully live in C++ and reflection, but the editor's Python module does
not grow a wrapper for it (unreal.NewThing stays missing until the next editor
start). Scripts can still reach it with unreal.load_class('/Script/Module.Name')
and obj.call_method('UFunctionName', args); they cannot construct its USTRUCTs.
"""

import os
import re
import subprocess
import sys
import time

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
LOG = os.path.join(REPO_ROOT, "Game", "Saved", "Logs", "PartyButtons.log")
REMOTE = os.path.join(os.path.dirname(__file__), "ue_remote.py")

DONE = re.compile(r"LogLiveCoding.*(Live coding succeeded|No changes applied|Live coding failed|"
                  r"Compile failed|Live coding compile failed|already compiling)", re.I)
ERROR = re.compile(r"(error C\d+|: error |fatal error)", re.I)


def main(timeout=600):
    start = os.path.getsize(LOG) if os.path.exists(LOG) else 0

    trigger = subprocess.run(
        [sys.executable, REMOTE, "-c",
         "import unreal; unreal.SystemLibrary.execute_console_command(None, 'LiveCoding.Compile')"])
    if trigger.returncode != 0:
        return 2

    deadline = time.time() + timeout
    errors = []
    while time.time() < deadline:
        with open(LOG, encoding="utf-8", errors="replace") as f:
            f.seek(start)
            new = f.read()
        for line in new.splitlines():
            if ERROR.search(line) and line not in errors:
                errors.append(line)
            m = DONE.search(line)
            if m:
                for e in errors:
                    print(e)
                print(line.strip())
                ok = "succeeded" in m.group(1).lower() or "no changes" in m.group(1).lower()
                return 0 if ok else 1
        time.sleep(1)

    print("ue_livecode: timed out waiting for a Live Coding result", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main())
