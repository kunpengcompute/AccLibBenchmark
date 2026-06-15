#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage:
  ./apply_brpc_patch.sh [--dry-run] /path/to/brpc-1.15.0

Apply RPC/brpc overlay files to a brpc 1.15 source tree.

Options:
  --dry-run   Print files that would be copied without modifying target.

RPC/brpc mirrors the brpc source root. For example:
  RPC/brpc/CMakeLists.txt      -> <brpc>/CMakeLists.txt
  RPC/brpc/src/CMakeLists.txt  -> <brpc>/src/CMakeLists.txt
  RPC/brpc/example/...         -> <brpc>/example/...
  RPC/brpc/test/...            -> <brpc>/test/...
USAGE
}

dry_run=0
target=""

while (($# > 0)); do
  case "$1" in
    --dry-run)
      dry_run=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    -*)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
    *)
      if [[ -n "${target}" ]]; then
        echo "Only one brpc source directory can be specified." >&2
        usage >&2
        exit 2
      fi
      target="$1"
      shift
      ;;
  esac
done

if [[ -z "${target}" ]]; then
  usage >&2
  exit 2
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
overlay_dir="${script_dir}/brpc"
target_dir="$(cd "${target}" && pwd)"

if [[ ! -d "${overlay_dir}" ]]; then
  echo "Overlay directory not found: ${overlay_dir}" >&2
  exit 1
fi

if [[ ! -f "${target_dir}/CMakeLists.txt" || ! -d "${target_dir}/src" ]]; then
  echo "Target does not look like a brpc source root: ${target_dir}" >&2
  exit 1
fi

count=0
while IFS= read -r -d '' src_file; do
  rel_path="${src_file#${overlay_dir}/}"
  dst_file="${target_dir}/${rel_path}"
  count=$((count + 1))
  if [[ "${dry_run}" -eq 1 ]]; then
    printf '[dry-run] %s -> %s\n' "${rel_path}" "${dst_file}"
  else
    mkdir -p "$(dirname "${dst_file}")"
    cp -f "${src_file}" "${dst_file}"
    printf 'copied %s\n' "${rel_path}"
  fi
done < <(find -L "${overlay_dir}" -type f -print0 | sort -z)

if [[ "${dry_run}" -eq 1 ]]; then
  printf 'Would apply %d files from %s to %s\n' "${count}" "${overlay_dir}" "${target_dir}"
else
  printf 'Applied %d files from %s to %s\n' "${count}" "${overlay_dir}" "${target_dir}"
fi
