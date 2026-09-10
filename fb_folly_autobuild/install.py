#!/usr/bin/env python3
"""fbthrift benchmark environment setup."""

import os
import shlex
import shutil
import subprocess
import sys
import tarfile
import tempfile
from pathlib import Path


# =============================================================================
# CONFIG: edit here
# =============================================================================

SCRIPT_DIR = Path(__file__).resolve().parent

WORK = Path("/home/xxx/temp_")
INS = WORK / "ins"

# Re-entry behavior. Existing Git checkouts are reused by default so that local
# edits survive a rebuild. Set SKIP_SOURCE_CLONE when every source tree has
# already been prepared and no new clone should be attempted.
AUTO_REENTRY = True
SKIP_SOURCE_CLONE = False
INSTALL_PACKAGES = False
REGENERATE_THRIFT_SOURCES = False

# Enable on aarch machines.
THRIFT_ENABLE_ARM_SVE2 = True


# Keep system headers ahead of incompatible copies under /usr/local,
# without modifying shared machine files.
GLOG_OVERLAY = WORK / "glog-system-overlay"
FMT_OVERLAY = WORK / "fmt-system-overlay"
FMT_CMAKE_DIR_CANDIDATES = (
    Path("/usr/lib/cmake/fmt"),
    Path("/usr/lib/aarch64-linux-gnu/cmake/fmt"),
    Path("/usr/lib/x86_64-linux-gnu/cmake/fmt"),
    Path("/usr/lib64/cmake/fmt"),
)

CMAKE_IGNORE_PREFIX_PATH = "/usr/local"
CMAKE_IGNORE_PATH = ";".join((
    "/usr/local/include/glog",
    "/usr/local/include/fmt",
    "/usr/local/lib",
    "/usr/local/lib64",
    "/usr/local/lib/cmake/glog",
    "/usr/local/lib64/cmake/glog",
    "/usr/local/lib/cmake/fmt",
    "/usr/local/lib64/cmake/fmt",
))


# Bundled beside install.py. Ignored if a usable Clang 16 pair already exists.
CLANG16_TARBALL = SCRIPT_DIR / "clang+llvm-16.0.6-aarch64-linux-gnu.tar.xz"
CLANG16_DIR = WORK / "clang16"

# Required pending-PR patch, applied at the repository root.
FBTHRIFT_PATCH = SCRIPT_DIR / "fbthrift.patch"

# Source repositories.  Clang 16 remains the only local tarball path; every
# other source tree is obtained with HTTPS git clone.
FOLLY_URL = "https://gitcode.com/boostkit/folly.git"
FOLLY_REF = "dev_iouring"

FIZZ_URL = "https://github.com/facebookincubator/fizz.git"
FIZZ_REF = "v2022.11.14.00"

WANGLE_URL = "https://github.com/facebook/wangle.git"
WANGLE_REF = "v2022.11.14.00"

FBTHRIFT_URL = "https://gitcode.com/boostkit/fbthrift.git"
FBTHRIFT_REF = "dev_20221114"

BENCHMARK_URL = "https://gitcode.com/donghuanan/fbBench.git"
BENCHMARK_REF = "main"

# Fixed source/install layout.
FOLLY_DIR = WORK / "folly"
FOLLY_INS = INS / "folly"

FIZZ_DIR = WORK / "fizz"
FIZZ_INS = INS / "fizz"

WANGLE_DIR = WORK / "wangle"
WANGLE_INS = INS / "wangle"

FBTHRIFT_DIR = WORK / "fbthrift"
FBTHRIFT_INS = INS / "fbthrift"

BENCHMARK_DIR = WORK / "fbthrift_folly_benchmark"

BUILD_TYPE = "Release"
JOBS = min(os.cpu_count() or 4, 32)

DEBIAN_PACKAGES = """
build-essential git cmake make pkg-config xz-utils numactl
liburing-dev libboost-all-dev libdouble-conversion-dev libgflags-dev
libgoogle-glog-dev libevent-dev libsodium-dev liblz4-dev libsnappy-dev
libzstd-dev libfmt-dev liblzma-dev libgtest-dev libgmock-dev libssl-dev
libaio-dev libunwind-dev libdwarf-dev binutils-dev libiberty-dev
zlib1g-dev libbz2-dev
""".split()

OPENEULER_PACKAGES = """
gcc gcc-c++ git cmake make pkgconf-pkg-config numactl
fmt fmt-devel
glog glog-devel
gflags gflags-devel
libevent libevent-devel
double-conversion double-conversion-devel
boost boost-devel
libunwind libunwind-devel
lz4 lz4-devel
zstd zstd-devel
libsodium libsodium-devel
liburing liburing-devel
libatomic
zlib zlib-devel
openssl openssl-devel
""".split()


# =============================================================================
# Helpers
# =============================================================================

def run(*args, env=None, cwd=None):
    cmd = [str(x) for x in args]

    if cwd is not None:
        print(
            "+ cd {} && {}".format(
                shlex.quote(str(cwd)),
                shlex.join(cmd),
            ),
            flush=True,
        )
    else:
        print("+", shlex.join(cmd), flush=True)

    subprocess.run(
        cmd,
        env=env,
        cwd=str(cwd) if cwd is not None else None,
        check=True,
    )

def install_packages():
    root = [] if os.geteuid() == 0 else ["sudo"]

    if shutil.which("apt-get"):
        run(*root, "apt-get", "update")
        run(*root, "apt-get", "install", "-y", *DEBIAN_PACKAGES)
    elif shutil.which("dnf"):
        run(*root, "dnf", "install", "-y", *OPENEULER_PACKAGES)
    else:
        raise RuntimeError("unsupported package manager: need apt-get or dnf")


def get_gcc_install_dir():
    crtbegin = subprocess.check_output(
        ["gcc", "-print-file-name=crtbeginS.o"],
        text=True,
    ).strip()

    crtbegin = Path(crtbegin)

    if not crtbegin.is_absolute() or not crtbegin.is_file():
        raise RuntimeError(
            "cannot locate GCC installation from crtbeginS.o: {}".format(
                crtbegin
            )
        )

    return crtbegin.parent

def clang_major(exe):
    result = subprocess.run(
        [str(exe), "--version"],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=True,
    )

    for line in result.stdout.splitlines():
        marker = "clang version "
        pos = line.lower().find(marker)
        if pos >= 0:
            version = line[pos + len(marker):].split(None, 1)[0]
            major = version.split(".", 1)[0]
            if major.isdigit():
                return int(major)

    raise RuntimeError(
        "cannot parse Clang version from stdout: {!r}; stderr: {!r}".format(
            result.stdout.strip(),
            result.stderr.strip(),
        )
    )


def probe_clang(C, CXX):
    with tempfile.TemporaryDirectory(prefix="clang16-probe-") as td:
        td = Path(td)

        c_src = td / "probe.c"
        c_exe = td / "probe-c"
        c_src.write_text(
            "int main(void){return 0;}\n",
            encoding="utf-8",
        )
        run(C, c_src, "-o", c_exe)
        run(c_exe)

        cxx_src = td / "probe.cpp"
        cxx_exe = td / "probe-cxx"
        cxx_src.write_text(
            "#include <vector>\n"
            "int main(){std::vector<int> v{1};return v.empty();}\n",
            encoding="utf-8",
        )
        run(CXX, "-std=c++17", cxx_src, "-o", cxx_exe)
        run(cxx_exe)


def find_clang16():
    # Find CXX first, then derive C from the same bin directory.
    for cxx_name, c_name in (
        ("clang++-16", "clang-16"),
        ("clang++", "clang"),
    ):
        cxx_found = shutil.which(cxx_name)
        if not cxx_found:
            continue

        CXX = Path(cxx_found).absolute()
        C = CXX.parent / c_name

        if not C.is_file():
            continue

        try:
            if clang_major(C) == 16 and clang_major(CXX) == 16:
                probe_clang(C, CXX)
                return C, CXX
        except (OSError, RuntimeError, subprocess.CalledProcessError):
            pass

    return None

def find_clang16_in(root):
    if not root.is_dir():
        return None

    for CXX in root.rglob("bin/clang++"):
        C = CXX.with_name("clang")

        if not C.is_file():
            continue

        try:
            if clang_major(C) == 16 and clang_major(CXX) == 16:
                return C.absolute(), CXX.absolute()
        except (OSError, RuntimeError, subprocess.CalledProcessError):
            continue

    return None


def get_clang16():
    # 1. First try an existing Clang 16 in PATH.
    found = find_clang16()
    if found:
        print(
            "Using existing Clang 16 from PATH:",
            *found,
            flush=True,
        )
        return found

    # 2. Reuse a previously extracted bundled Clang.
    found = find_clang16_in(CLANG16_DIR)
    if found:
        C, CXX = found

        print(
            "Reusing extracted Clang 16:",
            CXX.parent.parent,
            flush=True,
        )

        # Both operations are intentionally re-entrant:
        # - configure_clang overwrites the same cfg files.
        # - disable_bundled_libunwind does nothing if already disabled.
        configure_clang(C, CXX)
        disable_bundled_libunwind(CXX)

        probe_clang(C, CXX)

        return C, CXX

    # 3. The directory exists, but contains no valid compiler.
    # Do not overwrite an unknown/broken partial installation.
    if CLANG16_DIR.exists():
        raise RuntimeError(
            "Clang directory exists but contains no usable "
            "Clang 16 pair: {}".format(CLANG16_DIR)
        )

    # 4. First installation: extract bundled Clang.
    if not CLANG16_TARBALL.is_file():
        raise FileNotFoundError(
            "Clang 16 not found and bundled tarball is missing: "
            "{}".format(CLANG16_TARBALL)
        )

    print(
        "Clang 16 not found; extracting:",
        CLANG16_TARBALL,
        flush=True,
    )

    CLANG16_DIR.mkdir(parents=True)

    with tarfile.open(str(CLANG16_TARBALL), "r:xz") as archive:
        archive.extractall(str(CLANG16_DIR))

    found = find_clang16_in(CLANG16_DIR)
    if not found:
        raise RuntimeError(
            "bundled tarball does not contain a usable Clang 16 pair"
        )

    C, CXX = found

    configure_clang(C, CXX)
    disable_bundled_libunwind(CXX)

    probe_clang(C, CXX)

    return C, CXX


def configure_clang(C, CXX):
    gcc_install_dir = get_gcc_install_dir()

    option = "--gcc-install-dir={}\n".format(gcc_install_dir)

    clang_cfg = C.parent / "clang.cfg"
    clangxx_cfg = CXX.parent / "clang++.cfg"

    clang_cfg.write_text(option, encoding="utf-8")
    clangxx_cfg.write_text(option, encoding="utf-8")

    print(
        "Configured Clang GCC installation:",
        gcc_install_dir,
        flush=True,
    )

def disable_bundled_libunwind(CXX):
    clang_root = CXX.parent.parent

    candidates = list(
        (clang_root / "lib").glob("*-linux-gnu")
    )

    for clang_lib in candidates:
        for name in ("libunwind.so", "libunwind.a"):
            path = clang_lib / name

            if path.exists() or path.is_symlink():
                disabled = path.with_name(name + ".disabled")

                print(
                    "Disabling bundled libunwind:",
                    path,
                    flush=True,
                )

                path.rename(disabled)


def clone_repo(url, ref, destination):
    """Clone one repository through HTTPS, including its submodules."""
    if not url.startswith("https://"):
        raise ValueError("repository URL must use HTTPS: {}".format(url))

    cmd = ["git", "clone", "--recurse-submodules"]
    if ref is not None:
        cmd += ["--branch", ref, "--single-branch"]
    run(*cmd, url, destination)


def prepare_source(destination, url, ref=None):
    """Reuse an existing Git checkout or clone it from the configured URL."""
    destination = Path(destination)

    if destination.exists():
        if not destination.is_dir():
            raise RuntimeError(
                "source destination exists but is not a directory: {}".format(
                    destination
                )
            )

        if not (destination / ".git").exists():
            raise RuntimeError(
                "existing source is not a Git checkout; move or remove it "
                "before using HTTPS clone mode: {}".format(destination)
            )

        if AUTO_REENTRY or SKIP_SOURCE_CLONE:
            print("Reusing existing Git checkout:", destination, flush=True)
            return

        raise RuntimeError(
            "source destination already exists and AUTO_REENTRY is disabled: "
            "{}".format(destination)
        )

    if SKIP_SOURCE_CLONE:
        raise RuntimeError(
            "SKIP_SOURCE_CLONE is enabled but source is missing: {}".format(
                destination
            )
        )

    clone_repo(url, ref, destination)


def command_succeeds(*args, cwd=None):
    result = subprocess.run(
        [str(x) for x in args],
        cwd=str(cwd) if cwd is not None else None,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    return result.returncode == 0


def apply_patch(repo, patch_file):
    repo = Path(repo)
    patch_file = Path(patch_file)

    if not patch_file.is_file():
        raise FileNotFoundError("patch not found: {}".format(patch_file))

    if not (repo / ".git").exists():
        raise RuntimeError(
            "fbthrift source is not a Git checkout: {}".format(repo)
        )

    # Check the reverse patch first so re-entry does not fail when the patch
    # has already been applied.
    if command_succeeds(
        "git", "-C", repo, "apply", "--reverse", "--check", patch_file
    ):
        print("Patch already applied; skipping:", patch_file, flush=True)
        return

    run("git", "-C", repo, "apply", "--3way", patch_file)


def cmake_prefix_path():
    return ";".join(
        str(x) for x in (
            FOLLY_INS,
            FIZZ_INS,
            WANGLE_INS,
            FBTHRIFT_INS,
        )
    )


def prepare_header_overlay(name, system_headers, overlay_root):
    system_headers = Path(system_headers)
    overlay_root = Path(overlay_root)

    if not system_headers.is_dir():
        raise FileNotFoundError(
            "system {} headers not found: {}".format(name, system_headers)
        )

    overlay_root.mkdir(parents=True, exist_ok=True)
    overlay_link = overlay_root / name

    if overlay_link.is_symlink():
        if overlay_link.resolve() != system_headers.resolve():
            overlay_link.unlink()
    elif overlay_link.exists():
        raise RuntimeError(
            "{} overlay path exists and is not a symlink: {}".format(
                name, overlay_link
            )
        )

    if not overlay_link.exists():
        overlay_link.symlink_to(system_headers, target_is_directory=True)

    print(
        "Using system {} header overlay:".format(name),
        overlay_link,
        flush=True,
    )


def get_system_fmt_cmake_dir():
    override = os.environ.get("FMT_CMAKE_DIR")
    candidates = []
    if override:
        candidates.append(Path(override))
    candidates.extend(FMT_CMAKE_DIR_CANDIDATES)

    for directory in candidates:
        if ((directory / "fmt-config.cmake").is_file() or
                (directory / "fmtConfig.cmake").is_file()):
            return directory

    raise FileNotFoundError(
        "system fmt CMake config not found; checked: {}".format(
            ", ".join(str(x) for x in candidates)
        )
    )


def prepare_dependency_overlays():
    prepare_header_overlay("glog", "/usr/include/glog", GLOG_OVERLAY)
    prepare_header_overlay("fmt", "/usr/include/fmt", FMT_OVERLAY)
    print(
        "Using system fmt CMake package:",
        get_system_fmt_cmake_dir(),
        flush=True,
    )


def build_env(C, CXX):
    env = os.environ.copy()
    env["CC"] = str(C)
    env["CXX"] = str(CXX)
    env["CMAKE_PREFIX_PATH"] = cmake_prefix_path()

    overlay_flags = " ".join((
        "-I{}".format(GLOG_OVERLAY),
        "-I{}".format(FMT_OVERLAY),
    ))
    env["CFLAGS"] = "{} {}".format(
        overlay_flags,
        env.get("CFLAGS", ""),
    ).strip()
    env["CXXFLAGS"] = "{} {}".format(
        overlay_flags,
        env.get("CXXFLAGS", ""),
    ).strip()

    libs = []
    for prefix in (FOLLY_INS, FIZZ_INS, WANGLE_INS, FBTHRIFT_INS):
        libs.append(str(prefix / "lib"))
        libs.append(str(prefix / "lib64"))

    if env.get("LD_LIBRARY_PATH"):
        libs.append(env["LD_LIBRARY_PATH"])

    env["LD_LIBRARY_PATH"] = ":".join(libs)
    return env


def cmake(source, build, install, C, CXX, *extra):
    """Configure a CMake project without compiling it."""
    env = build_env(C, CXX)

    args = [
        "-S", source,
        "-B", build,
        "-DCMAKE_BUILD_TYPE=" + BUILD_TYPE,
        "-DCMAKE_CXX_STANDARD=17",
        "-DCMAKE_C_COMPILER=" + str(C),
        "-DCMAKE_CXX_COMPILER=" + str(CXX),
        "-DCMAKE_C_FLAGS=" + env["CFLAGS"],
        "-DCMAKE_CXX_FLAGS=" + env["CXXFLAGS"],
        "-DCMAKE_PREFIX_PATH=" + cmake_prefix_path(),
        "-DCMAKE_IGNORE_PREFIX_PATH=" + CMAKE_IGNORE_PREFIX_PATH,
        "-DCMAKE_IGNORE_PATH=" + CMAKE_IGNORE_PATH,
        "-Dfmt_DIR=" + str(get_system_fmt_cmake_dir()),
    ]
    if install is not None:
        args.append("-DCMAKE_INSTALL_PREFIX=" + str(install))
    args.extend(extra)
    run("cmake", *args, env=env)


def cmake_build(build, C, CXX, install=None):
    """Clean generated targets, compile, and optionally install."""
    env = build_env(C, CXX)

    # Keep CMakeCache.txt and the generator configuration, but remove all
    # generated objects before rebuilding edited source trees.
    run(
        "cmake",
        "--build",
        build,
        "--target",
        "clean",
        env=env,
    )
    run("cmake", "--build", build, "-j", JOBS, env=env)

    if install is not None:
        run("cmake", "--install", build, env=env)


def prepare_benchmark_source():
    """Reuse or clone the benchmark repository."""
    prepare_source(BENCHMARK_DIR, BENCHMARK_URL, BENCHMARK_REF)


# =============================================================================
# Main
# =============================================================================

def main():
    if sys.version_info < (3, 8):
        raise RuntimeError("Python 3.8+ is required")

    WORK.mkdir(parents=True, exist_ok=True)
    INS.mkdir(parents=True, exist_ok=True)

    if INSTALL_PACKAGES:
        install_packages()
    else:
        print("Skipping system package installation.", flush=True)

    prepare_dependency_overlays()

    C, CXX = get_clang16()
    print("C   =", C, flush=True)
    print("CXX =", CXX, flush=True)

    prepare_source(FOLLY_DIR, FOLLY_URL, FOLLY_REF)
    cmake(
        FOLLY_DIR,
        FOLLY_DIR / "_build",
        FOLLY_INS,
        C,
        CXX,
        "-DBUILD_BENCHMARKS=OFF",
        "-DBUILD_TESTS=OFF",
        "-DBUILD_SHARED_LIBS=ON",
    )
    cmake_build(FOLLY_DIR / "_build", C, CXX, FOLLY_INS)

    prepare_source(FIZZ_DIR, FIZZ_URL, FIZZ_REF)
    cmake(
        FIZZ_DIR / "fizz",
        FIZZ_DIR / "build_",
        FIZZ_INS,
        C,
        CXX,
        "-DBUILD_BENCHMARKS=OFF",
        "-DBUILD_TESTS=OFF",
        "-DBUILD_SHARED_LIBS=ON",
    )
    cmake_build(FIZZ_DIR / "build_", C, CXX, FIZZ_INS)

    prepare_source(WANGLE_DIR, WANGLE_URL, WANGLE_REF)
    cmake(
        WANGLE_DIR / "wangle",
        WANGLE_DIR / "build_",
        WANGLE_INS,
        C,
        CXX,
        "-DBUILD_BENCHMARKS=OFF",
        "-DBUILD_TESTS=OFF",
        "-DBUILD_SHARED_LIBS=ON",
    )
    cmake_build(WANGLE_DIR / "build_", C, CXX, WANGLE_INS)

    prepare_source(FBTHRIFT_DIR, FBTHRIFT_URL, FBTHRIFT_REF)
    apply_patch(FBTHRIFT_DIR, FBTHRIFT_PATCH)

    cmake(
        FBTHRIFT_DIR,
        FBTHRIFT_DIR / "build_",
        FBTHRIFT_INS,
        C,
        CXX,
        "-DTHRIFT_ENABLE_ARM_SVE2="
        + ("ON" if THRIFT_ENABLE_ARM_SVE2 else "OFF"),
    )
    cmake_build(FBTHRIFT_DIR / "build_", C, CXX, FBTHRIFT_INS)

    prepare_benchmark_source()

    thrift1 = FBTHRIFT_INS / "bin" / "thrift1"
    if not thrift1.is_file():
        raise RuntimeError(
            "thrift1 not found: {}".format(thrift1)
        )

    press_thrift = BENCHMARK_DIR / "press.thrift"
    if not press_thrift.is_file():
        raise RuntimeError(
            "press.thrift not found: {}".format(press_thrift)
        )

    gen_cpp2 = BENCHMARK_DIR / "gen-cpp2"
    if REGENERATE_THRIFT_SOURCES or not gen_cpp2.is_dir():
        print(
            "Generating benchmark thrift sources...",
            flush=True,
        )
        run(
            thrift1,
            "--gen", "mstch_cpp2",
            "press.thrift",
            cwd=BENCHMARK_DIR,
        )
    else:
        print(
            "Reusing existing generated thrift sources:",
            gen_cpp2,
            flush=True,
        )

    if not gen_cpp2.is_dir():
        raise RuntimeError(
            "thrift1 did not create gen-cpp2: {}".format(gen_cpp2)
        )

    cmake(
        BENCHMARK_DIR,
        BENCHMARK_DIR / "build",
        None,
        C,
        CXX,
    )
    cmake_build(BENCHMARK_DIR / "build", C, CXX)

    print("\nSetup completed.")
    print("Benchmark build:", BENCHMARK_DIR / "build")


if __name__ == "__main__":
    main()
