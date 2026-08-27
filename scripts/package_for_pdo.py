#!/usr/bin/env python3
"""Remap libsnowflakeclient build output into the pdo_snowflake vendor layout.

Extract the resulting pdo-vendor/ tree over pdo_snowflake/ so paths such as
libsnowflakeclient/lib/linux/libsnowflakeclient.a land in the right place.

Azure libraries follow the current libsnowflakeclient build (azure-core,
azure-storage-common, azure-storage-blobs), not the older azure-storage-lite
name still listed in pdo_snowflake.
"""

from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path

BIN_SUFFIXES = {".a", ".lib"}

AWS_LINUX = [
    "libaws-cpp-sdk-s3.a",
    "libaws-cpp-sdk-core.a",
    "libaws-cpp-sdk-sts.a",
    "libaws-crt-cpp.a",
    "libaws-c-s3.a",
    "libaws-c-auth.a",
    "libaws-c-http.a",
    "libaws-c-mqtt.a",
    "libaws-c-compression.a",
    "libaws-checksums.a",
    "libaws-c-cal.a",
    "libaws-c-event-stream.a",
    "libaws-c-sdkutils.a",
    "libaws-c-io.a",
    "libaws-c-common.a",
    "libs2n.a",
]

AWS_DARWIN = [
    "libaws-cpp-sdk-s3.a",
    "libaws-cpp-sdk-core.a",
    "libaws-cpp-sdk-sts.a",
    "libaws-crt-cpp.a",
    "libaws-c-s3.a",
    "libaws-c-auth.a",
    "libaws-c-http.a",
    "libaws-c-mqtt.a",
    "libaws-c-compression.a",
    "libaws-checksums.a",
    "libaws-c-cal.a",
    "libaws-c-event-stream.a",
    "libaws-c-sdkutils.a",
    "libaws-c-io.a",
    "libaws-c-common.a",
]

AWS_WIN = [
    "aws-cpp-sdk-sts.lib",
    "aws-cpp-sdk-s3.lib",
    "aws-cpp-sdk-core.lib",
    "aws-crt-cpp.lib",
    "aws-c-s3.lib",
    "aws-c-auth.lib",
    "aws-c-http.lib",
    "aws-c-mqtt.lib",
    "aws-c-compression.lib",
    "aws-checksums.lib",
    "aws-c-cal.lib",
    "aws-c-event-stream.lib",
    "aws-c-sdkutils.lib",
    "aws-c-io.lib",
    "aws-c-common.lib",
]

BOOST_UNIX = [
    "libboost_filesystem.a",
    "libboost_regex.a",
    "libboost_system.a",
    "libboost_url.a",
]

BOOST_WIN = [
    "libboost_filesystem.lib",
    "libboost_regex.lib",
    "libboost_system.lib",
    "libboost_url.lib",
]

AZURE_UNIX = [
    "libazure-core.a",
    "libazure-storage-common.a",
    "libazure-storage-blobs.a",
]

AZURE_WIN = [
    "azure-core.lib",
    "azure-storage-common.lib",
    "azure-storage-blobs.lib",
]

# dep name -> destination lib folder name used by PDO
# "lib64" only for Linux AWS; everything else is "lib" (lib64 source is remapped).
PLATFORMS = {
    "linux": {
        "src_deps": Path("deps-build/linux/Release"),
        "dest_deps": Path("libsnowflakeclient/deps-build/linux"),
        "client_src": Path("deps-build/linux/Release/libsnowflakeclient/lib/libsnowflakeclient.a"),
        "client_dst": Path("libsnowflakeclient/lib/linux/libsnowflakeclient.a"),
        "deps": [
            "openssl",
            "curl",
            "oob",
            "aws",
            "azure",
            "uuid",
            "arrow",
            "arrow_deps",
            "boost",
        ],
        "aws_libdir": "lib64",
        "required": (
            ["openssl/lib/libcrypto.a", "openssl/lib/libssl.a", "curl/lib/libcurl.a", "oob/lib/libtelemetry.a"]
            + [f"aws/lib64/{name}" for name in AWS_LINUX]
            + [f"azure/lib/{name}" for name in AZURE_UNIX]
            + [
                "uuid/lib/libuuid.a",
                "arrow/lib/libarrow.a",
                "arrow_deps/lib/libjemalloc_pic.a",
            ]
            + [f"boost/lib/{name}" for name in BOOST_UNIX]
        ),
    },
    "linux-aarch64": {
        "src_deps": Path("deps-build/linux/Release"),
        "dest_deps": Path("libsnowflakeclient/deps-build/linux/aarch64"),
        "client_src": Path("deps-build/linux/Release/libsnowflakeclient/lib/libsnowflakeclient.a"),
        "client_dst": Path("libsnowflakeclient/lib/linux/aarch64/libsnowflakeclient.a"),
        "deps": [
            "openssl",
            "curl",
            "oob",
            "aws",
            "azure",
            "uuid",
            "arrow",
            "arrow_deps",
            "boost",
        ],
        "aws_libdir": "lib64",
        "required": (
            ["openssl/lib/libcrypto.a", "openssl/lib/libssl.a", "curl/lib/libcurl.a", "oob/lib/libtelemetry.a"]
            + [f"aws/lib64/{name}" for name in AWS_LINUX]
            + [f"azure/lib/{name}" for name in AZURE_UNIX]
            + [
                "uuid/lib/libuuid.a",
                "arrow/lib/libarrow.a",
                "arrow_deps/lib/libjemalloc_pic.a",
            ]
            + [f"boost/lib/{name}" for name in BOOST_UNIX]
        ),
    },
    "darwin": {
        "src_deps": Path("deps-build/darwin/Release"),
        "dest_deps": Path("libsnowflakeclient/deps-build/darwin"),
        "client_src": Path("deps-build/darwin/Release/libsnowflakeclient/lib/libsnowflakeclient.a"),
        "client_dst": Path("libsnowflakeclient/lib/darwin/libsnowflakeclient.a"),
        "deps": [
            "openssl",
            "curl",
            "oob",
            "aws",
            "azure",
            "arrow",
            "arrow_deps",
            "boost",
        ],
        "aws_libdir": "lib",
        "required": (
            ["openssl/lib/libcrypto.a", "openssl/lib/libssl.a", "curl/lib/libcurl.a", "oob/lib/libtelemetry.a"]
            + [f"aws/lib/{name}" for name in AWS_DARWIN]
            + [f"azure/lib/{name}" for name in AZURE_UNIX]
            + [
                "arrow/lib/libarrow.a",
                "arrow_deps/lib/libjemalloc_pic.a",
            ]
            + [f"boost/lib/{name}" for name in BOOST_UNIX]
        ),
    },
    "win64-vs17": {
        "src_deps": Path("deps-build/win64/vs17/Release"),
        "dest_deps": Path("libsnowflakeclient/deps-build/win64/vs17"),
        "client_src": Path("deps-build/win64/vs17/Release/libsnowflakeclient/lib/snowflakeclient.lib"),
        "client_dst": Path("libsnowflakeclient/lib/win64/vs17/snowflakeclient.lib"),
        "deps": [
            "aws",
            "azure",
            "curl",
            "oob",
            "openssl",
            "zlib",
            "arrow",
            "boost",
        ],
        "aws_libdir": "lib",
        "required": (
            [f"aws/lib/{name}" for name in AWS_WIN]
            + [f"azure/lib/{name}" for name in AZURE_WIN]
            + [
                "curl/lib/libcurl_a.lib",
                "oob/lib/libtelemetry_a.lib",
                "openssl/lib/libssl_a.lib",
                "openssl/lib/libcrypto_a.lib",
                "zlib/lib/zlib_a.lib",
                "arrow/lib/arrow_static.lib",
            ]
            + [f"boost/lib/{name}" for name in BOOST_WIN]
        ),
    },
}


def iter_binaries(directory: Path):
    if not directory.is_dir():
        return
    for path in directory.iterdir():
        if path.is_file() and path.suffix.lower() in BIN_SUFFIXES:
            yield path


def copy_file(src: Path, dst: Path) -> None:
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)


def source_lib_dirs(src_dep: Path, dest_libdir: str):
    """Yield (source_dir, dest_subdir) pairs for a dependency."""
    lib = src_dep / "lib"
    lib64 = src_dep / "lib64"
    if dest_libdir == "lib64":
        if lib64.is_dir():
            yield lib64, "lib64"
        elif lib.is_dir():
            yield lib, "lib64"
        return
    if lib.is_dir():
        yield lib, "lib"
    elif lib64.is_dir():
        yield lib64, "lib"


def copy_dep(src_deps: Path, dest_deps: Path, name: str, aws_libdir: str) -> None:
    src_dep = src_deps / name
    if not src_dep.is_dir():
        raise SystemExit(f"missing dependency directory: {src_dep}")
    dest_libdir = aws_libdir if name == "aws" else "lib"
    copied = 0
    for src_dir, dest_subdir in source_lib_dirs(src_dep, dest_libdir):
        for binary in iter_binaries(src_dir):
            copy_file(binary, dest_deps / name / dest_subdir / binary.name)
            copied += 1
    # Windows oob stages libtelemetry_a.lib as a file named "release"/"debug".
    if name == "oob":
        for staged_name in ("release", "debug"):
            staged = src_dep / "lib" / staged_name
            dest = dest_deps / name / "lib" / "libtelemetry_a.lib"
            if staged.is_file() and not dest.is_file():
                copy_file(staged, dest)
                copied += 1
    if copied == 0:
        raise SystemExit(f"no .a/.lib files found under {src_dep}/lib or lib64")


def copy_headers(repo: Path, dest_root: Path) -> None:
    src = repo / "include" / "snowflake"
    if not src.is_dir():
        raise SystemExit(f"missing headers: {src}")
    dest = dest_root / "libsnowflakeclient" / "include" / "snowflake"
    dest.mkdir(parents=True, exist_ok=True)
    for path in src.iterdir():
        if path.is_file():
            shutil.copy2(path, dest / path.name)


def copy_cacert(repo: Path, dest_root: Path) -> None:
    src = repo / "cacert.pem"
    if src.is_file():
        copy_file(src, dest_root / "libsnowflakeclient" / "cacert.pem")


def package(platform: str, repo: Path, dest_root: Path) -> None:
    spec = PLATFORMS[platform]
    src_deps = repo / spec["src_deps"]
    dest_deps = dest_root / spec["dest_deps"]
    client_src = repo / spec["client_src"]
    client_dst = dest_root / spec["client_dst"]

    if dest_root.exists():
        shutil.rmtree(dest_root)
    dest_root.mkdir(parents=True)

    if not client_src.is_file():
        raise SystemExit(f"missing libsnowflakeclient binary: {client_src}")
    copy_file(client_src, client_dst)

    copy_headers(repo, dest_root)
    copy_cacert(repo, dest_root)

    for name in spec["deps"]:
        copy_dep(src_deps, dest_deps, name, spec["aws_libdir"])

    missing = []
    for relative in spec["required"]:
        path = dest_deps / relative
        if not path.is_file():
            missing.append(str(path))
    if missing:
        raise SystemExit("missing PDO-required libraries:\n  " + "\n  ".join(missing))

    print(f"packaged {platform} -> {dest_root / 'libsnowflakeclient'}")


def merge(inputs: list[Path], dest_root: Path) -> None:
    """Overlay vendor trees. Later inputs win (Windows last => CRLF headers)."""
    if dest_root.exists():
        shutil.rmtree(dest_root)
    dest_root.mkdir(parents=True)
    for src in inputs:
        vendor = src / "libsnowflakeclient"
        if not vendor.is_dir():
            raise SystemExit(f"expected {vendor} in merge input")
        shutil.copytree(vendor, dest_root / "libsnowflakeclient", dirs_exist_ok=True)
    print(f"merged {len(inputs)} trees -> {dest_root / 'libsnowflakeclient'}")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    pkg = sub.add_parser("package", help="remap one platform into pdo-vendor/")
    pkg.add_argument(
        "--platform",
        required=True,
        choices=sorted(PLATFORMS),
        help="platform identity matching the local deps-build tree",
    )
    pkg.add_argument("--repo", type=Path, default=Path("."), help="libsnowflakeclient repo root")
    pkg.add_argument("--output", type=Path, default=Path("pdo-vendor"), help="output directory")

    mrg = sub.add_parser("merge", help="overlay several pdo-vendor trees")
    mrg.add_argument("--inputs", type=Path, nargs="+", required=True)
    mrg.add_argument("--output", type=Path, default=Path("pdo-vendor"))

    args = parser.parse_args(argv)
    if args.command == "package":
        package(args.platform, args.repo.resolve(), args.output.resolve())
    else:
        merge([p.resolve() for p in args.inputs], args.output.resolve())
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
