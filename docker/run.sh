#!/usr/bin/env bash
# Run a command inside the Owl build image, mirroring CLion's "Docker Owl" toolchain.
#
# Usage: docker/run.sh [--gui] [--gpu=auto|intel|nvidia] [--platform=auto|wayland|x11] [--perf] [--] <command...>
#   --gui       expose the GPU, X11/Wayland display and PulseAudio (run OwlNest, Vulkan/OpenGL tests).
#   --gpu=      on a hybrid laptop, pin Vulkan, EGL and GLX to one GPU (nvidia = PRIME render offload); implies --gui.
#   --platform= windowing platform of the engine (sets OWL_WINDOW_PLATFORM); implies --gui.
#   --perf      allow perf / gdb / valgrind / TSan (ptrace + perf events, no seccomp/AppArmor confinement).
#
# Examples:
#   docker/run.sh cmake --preset linux-clang-release -S .
#   docker/run.sh cmake --build output/build/linux-clang-release
#   docker/run.sh ctest --test-dir output/build/linux-clang-release --output-on-failure -j8
#   docker/run.sh poetry run python ci_action.py CodeStyle linux-clang-release
#   docker/run.sh --gui output/build/linux-clang-release/bin/OwlNest
#   docker/run.sh --gpu=nvidia --platform=x11 output/build/linux-clang-release/bin/OwlRunner
#
# Environment overrides:
#   OWL_DOCKER_IMAGE  image to use (default: the CI builder image).
#   OWL_DOCKER_HOME   host directory mounted as $HOME (poetry venv, conan cache, ccache).
#   OWL_DOCKER_MOUNTS extra host directories (space separated), mounted at the same path (e.g. a scratch dir).
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
image="${OWL_DOCKER_IMAGE:-registry.argawaen.net/builder/devel-ubuntu2604:latest}"
# The default $HOME sits next to the main checkout, also when running from a git worktree.
main_root="${repo_root}"
if [[ -f "${repo_root}/.git" ]]; then
	common_dir="$(git -C "${repo_root}" rev-parse --path-format=absolute --git-common-dir 2>/dev/null || true)"
	[[ -n "${common_dir}" ]] && main_root="$(dirname "${common_dir}")"
fi
docker_home="${OWL_DOCKER_HOME:-$(cd "${main_root}/.." && pwd)/fake_home}"
uid="$(id -u)"
gid="$(id -g)"

gui=0
perf=0
gpu="auto"
platform="${OWL_WINDOW_PLATFORM:-}"
while [[ $# -gt 0 ]]; do
	case "$1" in
	--gui) gui=1; shift ;;
	--gpu=*) gpu="${1#--gpu=}"; gui=1; shift ;;
	--platform=*) platform="${1#--platform=}"; gui=1; shift ;;
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
# In a git worktree, .git is a file pointing at the main repository: mount its git dir so git works inside.
if [[ -f "${repo_root}/.git" ]]; then
	common_git="$(git -C "${repo_root}" rev-parse --path-format=absolute --git-common-dir 2>/dev/null || true)"
	[[ -n "${common_git}" && -d "${common_git}" && "${common_git}" != "${repo_root}"* ]] && args+=(-v "${common_git}:${common_git}")
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
	[[ -n "${platform}" ]] && args+=(-e OWL_WINDOW_PLATFORM="${platform}")
	case "${gpu}" in
	auto) ;;
	intel | mesa)
		args+=(-e VK_DRIVER_FILES=/usr/share/vulkan/icd.d/intel_icd.json
			-e __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json
			-e __GLX_VENDOR_LIBRARY_NAME=mesa) ;;
	nvidia)
		args+=(-e VK_DRIVER_FILES=/etc/vulkan/icd.d/nvidia_icd.json
			-e __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/10_nvidia.json
			-e __NV_PRIME_RENDER_OFFLOAD=1 -e __GLX_VENDOR_LIBRARY_NAME=nvidia) ;;
	*) echo "docker/run.sh: unknown --gpu=${gpu} (auto, intel or nvidia)." >&2; exit 2 ;;
	esac
	# A locked or hidden session gets no frame callbacks: vsync'ed Wayland swaps block, XWayland drops to ~1 fps.
	if [[ -n "${XDG_SESSION_ID:-}" ]] && command -v loginctl > /dev/null &&
		[[ "$(loginctl show-session "${XDG_SESSION_ID}" -p LockedHint --value 2>/dev/null)" == "yes" ]]; then
		echo "docker/run.sh: warning: the desktop session is locked, the compositor will not present frames." >&2
	fi
fi

if [[ ${perf} -eq 1 ]]; then
	args+=(--cap-add SYS_PTRACE --cap-add PERFMON --cap-add SYS_ADMIN --security-opt seccomp=unconfined --security-opt apparmor=unconfined)
fi

exec docker "${args[@]}" "${image}" "$@"
