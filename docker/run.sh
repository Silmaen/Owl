#!/usr/bin/env bash
# Run a command inside the Owl build image, mirroring CLion's "Docker Owl" toolchain.
#
# Usage: docker/run.sh [--gui] [--perf] [--] <command...>
#   --gui   expose the GPU, X11/Wayland display and PulseAudio (run OwlNest, Vulkan/OpenGL tests).
#   --perf  allow perf / gdb / valgrind (ptrace + perf events).
#
# Examples:
#   docker/run.sh cmake --preset linux-clang-release -S .
#   docker/run.sh cmake --build output/build/linux-clang-release
#   docker/run.sh ctest --test-dir output/build/linux-clang-release --output-on-failure -j8
#   docker/run.sh poetry run python ci_action.py CodeStyle linux-clang-release
#   docker/run.sh --gui output/build/linux-clang-release/bin/OwlNest
#
# Environment overrides:
#   OWL_DOCKER_IMAGE  image to use (default: the CI builder image).
#   OWL_DOCKER_HOME   host directory mounted as $HOME (poetry venv, depmanager cache, ccache).
#   OWL_DOCKER_MOUNTS extra host directories (space separated), mounted at the same path (e.g. a scratch dir).
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
image="${OWL_DOCKER_IMAGE:-registry.argawaen.net/builder/devel-ubuntu2404:latest}"
docker_home="${OWL_DOCKER_HOME:-$(cd "${repo_root}/.." && pwd)/fake_home}"
uid="$(id -u)"
gid="$(id -g)"

gui=0
perf=0
while [[ $# -gt 0 ]]; do
	case "$1" in
	--gui) gui=1; shift ;;
	--perf) perf=1; shift ;;
	--) shift; break ;;
	*) break ;;
	esac
done
if [[ $# -eq 0 ]]; then
	set -- bash
fi

mkdir -p "${docker_home}"

# Build caches embed absolute paths: the repo is mounted at its host path, $HOME at /fhome like CLion.
workdir="${PWD}"
[[ "${workdir}" == "${repo_root}"* ]] || workdir="${repo_root}"

args=(run --rm --init --entrypoint=
	-u "${uid}:${gid}"
	-e HOME=/fhome
	-v "${repo_root}:${repo_root}"
	-v "${docker_home}:/fhome"
	-w "${workdir}")
if [[ -t 0 && -t 1 ]]; then
	args+=(-it)
elif [[ ! -t 0 ]]; then
	args+=(-i)
fi
for extra in ${OWL_DOCKER_MOUNTS:-}; do
	[[ -d "${extra}" ]] && args+=(-v "${extra}:${extra}")
done

if [[ ${gui} -eq 1 ]]; then
	args+=(--device /dev/dri --group-add video --group-add audio
		-e DISPLAY="${DISPLAY:-:0}"
		-v /tmp/.X11-unix:/tmp/.X11-unix
		-e XDG_RUNTIME_DIR="/run/user/${uid}"
		-v "/run/user/${uid}:/run/user/${uid}"
		-e PULSE_SERVER="unix:/run/user/${uid}/pulse/native")
	[[ -n "${WAYLAND_DISPLAY:-}" ]] && args+=(-e WAYLAND_DISPLAY="${WAYLAND_DISPLAY}")
	[[ -n "${XAUTHORITY:-}" && -f "${XAUTHORITY}" ]] && args+=(-e XAUTHORITY=/tmp/.xauth -v "${XAUTHORITY}:/tmp/.xauth:ro")
	[[ -e /dev/snd ]] && args+=(--device /dev/snd)
	docker info --format '{{json .Runtimes}}' 2>/dev/null | grep -q nvidia && args+=(--gpus all
		-e NVIDIA_DRIVER_CAPABILITIES=all)
fi

if [[ ${perf} -eq 1 ]]; then
	args+=(--cap-add SYS_PTRACE --cap-add PERFMON --cap-add SYS_ADMIN --security-opt seccomp=unconfined)
fi

exec docker "${args[@]}" "${image}" "$@"
