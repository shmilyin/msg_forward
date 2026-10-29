"""项目内隔离的安装、固件构建与主机编译器检查入口。"""
import argparse
import hashlib
import concurrent.futures
import json
import os
from pathlib import Path
import subprocess
import sys
import threading
import zipfile

ROOT = Path(__file__).resolve().parent
LOCK = json.loads((ROOT / "toolchain-lock.json").read_text(encoding="utf-8"))
CLI_VERSION = LOCK["arduino_cli"]
CORE = LOCK["core"]
LIBRARIES = [f"{name}@{version}" for name, version in LOCK["libraries"].items()]
FQBN = LOCK["fqbn"]
CLI = ROOT / LOCK["paths"]["cli"]
HOST = ROOT / LOCK["host"]["compiler"]
HOST_SHA256 = LOCK["host"]["sha256"]
ENV = os.environ.copy()
for key, value in {"DATA": "data", "DOWNLOADS": "downloads", "USER": "user"}.items():
    ENV[f"ARDUINO_DIRECTORIES_{key}"] = str(ROOT / ".arduino" / value)
ENV["ARDUINO_BUILD_CACHE_PATH"] = str(ROOT / "build/cache")


def run(args, **kwargs):
    print("运行:", subprocess.list2cmdline([str(x) for x in args]), flush=True)
    return subprocess.run([str(x) for x in args], cwd=ROOT, env=ENV, check=True, **kwargs)


def arduino(*args):
    return run([CLI, "--config-file", ROOT / "arduino-cli.yaml", *args])


def download(url, destination):
    run(["curl", "--ssl-revoke-best-effort", "-fL", "--retry", "3", url, "-o", destination])


def prefetch():
    """按官方索引并行下载分片，完整校验后交给 CLI 安装。"""
    packages = []
    for name in ("package_esp32_index.json", "package_index.json"):
        packages.extend(json.loads((ROOT / ".arduino/data" / name).read_text(encoding="utf-8"))["packages"])
    esp = next(p for p in packages if p["name"] == "esp32")
    platform = next(p for p in esp["platforms"] if p["version"] == CORE.split("@")[-1])
    archives = [platform]
    for dep in platform["toolsDependencies"]:
        package = next(p for p in packages if p["name"] == dep["packager"])
        tool = next(t for t in package["tools"] if t["name"] == dep["name"] and t["version"] == dep["version"])
        systems = {s["host"]: s for s in tool["systems"]}
        archives.append(next(systems[h] for h in ("x86_64-mingw32", "i686-mingw32", "i686-mingw32msvc") if h in systems))
    cache = ROOT / ".arduino/downloads/packages"
    cache.mkdir(parents=True, exist_ok=True)
    workers = int(os.environ.get("ARDUINO_DOWNLOAD_WORKERS", "4"))
    curl_options = ["--noproxy", "*"] if os.environ.get("ARDUINO_DOWNLOAD_DIRECT") == "1" else []
    for archive in archives:
        target = cache / archive["archiveFileName"]
        expected = archive["checksum"].split(":")[-1]
        if target.exists() and hashlib.file_digest(target.open("rb"), "sha256").hexdigest() == expected:
            continue
        size = int(archive["size"])
        parts = ROOT / ".tools/parts" / archive["archiveFileName"]
        parts.mkdir(parents=True, exist_ok=True)
        chunk = 4 * 1024 * 1024
        url = archive["url"]
        if os.environ.get("ARDUINO_DOWNLOAD_MIRROR") == "espressif" and url.startswith("https://github.com/espressif/"):
            url = url.replace("https://github.com/", "https://dl.espressif.com/github_assets/", 1)
        jobs = [(url, start, min(size, start + chunk) - 1, parts / str(start))
                for start in range(0, size, chunk)]

        failed = threading.Event()

        def fetch(job):
            if failed.is_set():
                return
            url, start, end, part = job
            length = end - start + 1
            for attempt in range(3):
                have = part.stat().st_size if part.exists() else 0
                if have == length:
                    return
                if have > length:
                    raise RuntimeError(f"分片超出预期长度: {part}")
                incoming = part.with_suffix(".incoming")
                headers = part.with_suffix(".headers")
                result = subprocess.run([
                    "curl", "--ssl-revoke-best-effort", "-fsSL", *curl_options,
                    "--connect-timeout", "20", "--max-time", "180",
                    "--speed-limit", "1024", "--speed-time", "30",
                    "--range", f"{start + have}-{end}", "-D", str(headers),
                    url, "-o", str(incoming)])
                response_headers = headers.read_text(errors="replace") if headers.exists() else ""
                content_range = f"content-range: bytes {start + have}-{end}/{size}"
                if incoming.exists() and content_range in response_headers.lower():
                    if incoming.stat().st_size > length - have:
                        raise RuntimeError(f"服务器返回分片超长: {part}")
                    with part.open("ab") as output, incoming.open("rb") as source:
                        while data := source.read(1024 * 1024):
                            output.write(data)
                if part.exists() and part.stat().st_size == length:
                    print(f"下载分片完成: {part.parent.name} {start}", flush=True)
                    return
                print(f"分片重试 {attempt + 1}/3: {part.name}, curl={result.returncode}", flush=True)
            failed.set()
            raise RuntimeError(f"分片下载失败，保留进度: {part}")

        print(f"获取: {target.name}, 并行数={workers}", flush=True)
        with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as pool:
            list(pool.map(fetch, jobs))
        assembled = target.with_suffix(target.suffix + ".assembling")
        with assembled.open("wb") as output:
            for start in range(0, size, chunk):
                output.write((parts / str(start)).read_bytes())
        with assembled.open("rb") as source:
            actual = hashlib.file_digest(source, "sha256").hexdigest()
        if actual != expected:
            raise RuntimeError(f"SHA256 不符: {assembled}; 预期={expected}, 实际={actual}")
        assembled.replace(target)
        print(f"校验通过: {target.name} {actual}", flush=True)


def setup():
    (ROOT / ".tools").mkdir(exist_ok=True)
    if not CLI.exists():
        archive = ROOT / f".tools/arduino-cli_{CLI_VERSION}_Windows_64bit.zip"
        download(f"https://downloads.arduino.cc/arduino-cli/arduino-cli_{CLI_VERSION}_Windows_64bit.zip", archive)
        with zipfile.ZipFile(archive) as z:
            z.extractall(CLI.parent)
    arduino("version")
    arduino("core", "update-index")
    arduino("core", "install", CORE)
    arduino("lib", "install", *LIBRARIES)


def host_setup():
    (ROOT / ".tools").mkdir(exist_ok=True)
    if not HOST.exists():
        archive = ROOT / ".tools/w64devkit-x64-2.4.0.7z.exe"
        if not archive.exists():
            download("https://github.com/skeeto/w64devkit/releases/download/v2.4.0/w64devkit-x64-2.4.0.7z.exe", archive)
        if hashlib.sha256(archive.read_bytes()).hexdigest() != HOST_SHA256:
            raise RuntimeError("主机编译器压缩包 SHA256 不匹配")
        run([archive, "-y", "-o" + str(ROOT / ".tools")])
    run([HOST, "--version"])
    out = ROOT / "build/native-smoke.exe"
    out.parent.mkdir(exist_ok=True)
    run([HOST, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-x", "c++", "-", "-o", out],
        input=b'#include <vector>\n#include <numeric>\nint main(){std::vector<int> v{1,2,3};return std::accumulate(v.begin(),v.end(),0)==6?0:1;}\n')
    run([out])


def compile_firmware():
    output = ROOT / "build/esp32c3"
    output.mkdir(parents=True, exist_ok=True)
    args = [CLI, "--config-file", ROOT / "arduino-cli.yaml", "compile", "--fqbn", FQBN,
            "--build-path", output, "--warnings", "all", ROOT / "code"]
    print("运行:", subprocess.list2cmdline([str(x) for x in args]), flush=True)
    with (ROOT / "build/compile.log").open("w", encoding="utf-8") as log:
        process = subprocess.Popen([str(x) for x in args], cwd=ROOT, env=ENV,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   text=True, encoding="utf-8", errors="replace")
        for line in process.stdout:
            print(line, end="", flush=True)
            log.write(line)
        if process.wait():
            raise SystemExit(process.returncode)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=["setup", "prefetch", "host", "compile", "versions"])
    command = parser.parse_args().command
    try:
        if command == "setup":
            setup()
        elif command == "prefetch":
            prefetch()
        elif command == "host":
            host_setup()
        elif command == "compile":
            compile_firmware()
        else:
            arduino("version")
            arduino("core", "list")
            arduino("lib", "list")
            run([HOST, "--version"])
    except subprocess.CalledProcessError as exc:
        sys.exit(exc.returncode)
