"""
ue_remote.py — run Python inside the RUNNING Unreal editor.

Uses the engine's own PythonScriptPlugin remote execution (UDP multicast
discovery + a TCP command channel). Requires Project Settings > Python >
Enable Remote Execution, which is on for this project
(bRemoteExecution=True in Game/Config/DefaultEngine.ini).

This is the preferred way to make level changes: the edit lands in the level
the user has open, with undo, instead of a headless commandlet saving the map
behind the editor's back.

Usage:
  python AI/tools/ue_remote.py path/to/script.py      # run a file
  python AI/tools/ue_remote.py -c "import unreal; ..." # run a snippet

The script runs with __file__ unset (it is sent as text), so scripts meant for
this should not derive paths from __file__ — use absolute paths.

Output from print()/unreal.log() comes back and is printed here. Exit code is
non-zero if the editor wasn't found or the command raised.
"""

import argparse
import os
import sys
import time

ENGINE_PY = "C:/Program Files/Epic Games/UE_5.7/Engine/Plugins/Experimental/PythonScriptPlugin/Content/Python"
sys.path.insert(0, ENGINE_PY)
import remote_execution  # noqa: E402


def find_node(remote, timeout):
    deadline = time.time() + timeout
    while time.time() < deadline:
        nodes = remote.remote_nodes
        if nodes:
            return nodes[0]
        time.sleep(0.1)
    return None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("file", nargs="?")
    parser.add_argument("-c", "--command")
    parser.add_argument("--timeout", type=float, default=5.0, help="seconds to wait for editor discovery")
    args = parser.parse_args()

    if args.command:
        code = args.command
    elif args.file:
        with open(args.file, encoding="utf-8") as f:
            code = f.read()
    else:
        parser.error("give a script file or -c")

    remote = remote_execution.RemoteExecution()
    remote.start()
    try:
        node = find_node(remote, args.timeout)
        if not node:
            print("ue_remote: no editor answered — is it running with remote execution enabled?", file=sys.stderr)
            return 2

        remote.open_command_connection(node)
        # EXECUTE_FILE runs a multi-line script in the editor's global scope;
        # despite the name it accepts literal code as well as a path.
        result = remote.run_command(code, unattended=True, exec_mode=remote_execution.MODE_EXEC_FILE)

        for entry in result.get("output", []):
            print(f"[{entry.get('type', '')}] {entry.get('output', '').rstrip()}")
        if result.get("result") not in (None, "None", ""):
            print(result["result"])

        return 0 if result.get("success") else 1
    finally:
        remote.stop()


if __name__ == "__main__":
    sys.exit(main())
