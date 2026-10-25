"""Run the real native server and real Beast clients. Standard library only.

Ephemeral credentials stay in child-process environments/memory, never files.
"""
import os
import queue
import re
import secrets
import signal
import socket
import subprocess
import sys
import threading


def line_with_timeout(stream, timeout=8):
    result = queue.Queue(maxsize=1)
    threading.Thread(target=lambda: result.put(stream.readline()), daemon=True).start()
    return result.get(timeout=timeout)


def listening_addresses(pid, port):
    if os.name == "nt":
        output = subprocess.check_output(["netstat.exe", "-ano", "-p", "tcp"], text=True)
        return [words[1] for row in output.splitlines()
                if len(words := row.split()) >= 5 and words[-1] == str(pid)
                and words[3] == "LISTENING" and words[1].endswith(":" + str(port))]
    output = subprocess.check_output(["ss", "-ltnp"], text=True)
    return [words[3] for row in output.splitlines()
            if len(words := row.split()) >= 5 and f"pid={pid}," in row
            and words[3].endswith(":" + str(port))]


def stop(process):
    if process.poll() is None:
        process.send_signal(signal.CTRL_BREAK_EVENT if os.name == "nt" else signal.SIGTERM)


def main():
    server, tester = sys.argv[1:]
    secret = secrets.token_hex(32)
    env = os.environ.copy()
    env["PFME_DEV_SECRET"] = secret
    env["PFME_TEST_SECRET"] = secret
    absent = env.copy()
    absent.pop("PFME_DEV_SECRET")
    for args, config in [([], absent), (["--bind", "0.0.0.0"], env),
                         (["--bind", "192.168.1.2"], env), (["--bind", "::"], env),
                         (["--port", "-1"], env)]:
        result = subprocess.run([server, *args], env=config, capture_output=True, text=True, timeout=5)
        assert result.returncode == 2 and "listening=" not in result.stdout, "unsafe startup accepted"
        assert secret not in result.stdout + result.stderr, "startup leaked credential"
    flags = subprocess.CREATE_NEW_PROCESS_GROUP if os.name == "nt" else 0
    process = subprocess.Popen([server, "--port", "0", "--seed", "12345", "--run-for-ms", "65000"],
                               env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                               creationflags=flags)
    closer = None
    try:
        first = line_with_timeout(process.stdout)
        match = re.fullmatch(r"listening=127\.0\.0\.1:(\d+) protocol=1\n", first)
        assert match, "server did not start safely"
        port = int(match.group(1))
        addresses = listening_addresses(process.pid, port)
        assert addresses == [f"127.0.0.1:{port}"], "non-loopback or missing listener"
        print("verified_listener=127.0.0.1 (OS socket table)")
        result = subprocess.run([tester, str(port)], env=env, capture_output=True, text=True, timeout=50)
        assert secret not in result.stdout + result.stderr, "test credential leaked"
        if result.returncode:
            print(result.stdout, end="")
            print(result.stderr, end="", file=sys.stderr)
            raise AssertionError("network security tests failed")
        print(result.stdout, end="")
        closer = subprocess.Popen([tester, str(port), "--wait-close"], env=env,
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        assert line_with_timeout(closer.stdout) == "shutdown_client_ready\n", "shutdown client failed"
        stop(process)
        remaining, errors = process.communicate(timeout=6)
        closed, close_errors = closer.communicate(timeout=6)
        assert closer.returncode == 0 and "shutdown_client_closed" in closed, "closing handshake failed"
        assert process.returncode == 0 and "stopped\n" in remaining, "unclean shutdown"
        assert secret not in remaining + errors + closed + close_errors, "credential logged"
        assert not listening_addresses(process.pid, port), "listener remained after shutdown"
        with socket.socket() as probe:
            assert probe.connect_ex(("127.0.0.1", port)) != 0, "listener still accepts connections"
        print("startup_policy_checks=5 passed")
        print("clean_shutdown=true no_remaining_listener=true credential_logs=clean")
    finally:
        for child in (closer, process):
            if child is not None and child.poll() is None:
                try:
                    stop(child)
                    child.wait(timeout=4)
                except (OSError, subprocess.TimeoutExpired):
                    child.kill()
                    child.wait(timeout=4)


if __name__ == "__main__":
    main()
