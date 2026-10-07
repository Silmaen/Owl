#!/usr/bin/env bash
#
# Wayland smoke test: OwlRunner opens a window on a headless weston and presents frames, Vulkan (lavapipe) then
# OpenGL (llvmpipe), on the Wayland platform. Exits 77 (CTest "skipped") when weston is not installed.
# Usage: wayland_smoke.sh <OwlRunner> <repository root> <work directory>
#
set -u

runner=$1
root=$2
work=$3

if ! command -v weston >/dev/null 2>&1; then
	echo "Wayland smoke test: weston not found, skipped."
	exit 77
fi
mkdir -p "${work}"
XDG_RUNTIME_DIR="$(mktemp -d "${work}/xdg.XXXXXX")"
export XDG_RUNTIME_DIR
chmod 700 "${XDG_RUNTIME_DIR}"
socket=owl-smoke
weston --backend=headless --renderer=pixman --socket="${socket}" --width=1280 --height=720 --idle-time=0 \
	>"${work}/weston.log" 2>&1 &
weston_pid=$!
trap 'kill "${weston_pid}" 2>/dev/null; wait "${weston_pid}" 2>/dev/null; rm -rf "${XDG_RUNTIME_DIR}"' EXIT
for _ in $(seq 100); do
	[[ -S "${XDG_RUNTIME_DIR}/${socket}" ]] && break
	sleep 0.1
done
if [[ ! -S "${XDG_RUNTIME_DIR}/${socket}" ]]; then
	echo "Wayland smoke test: weston did not start."
	cat "${work}/weston.log"
	exit 1
fi

export WAYLAND_DISPLAY="${socket}" OWL_WINDOW_PLATFORM=wayland
# Same software-driver setup as the image tests (test/render_tests/RenderImage_test.cpp): capped CPU, no shader cache.
export GALLIUM_OVERRIDE_CPU_CAPS=sse4.1 MESA_SHADER_CACHE_DISABLE=true
unset DISPLAY
status=0
for backend in vulkan opengl; do
	log="${work}/wayland_${backend}.log"
	if [[ ${backend} == vulkan ]]; then
		driver=(VK_ICD_FILENAMES="${OWL_RENDER_TESTS_VK_ICD:-/usr/share/vulkan/icd.d/lvp_icd.json}")
	else
		driver=(LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json)
	fi
	(cd "${work}" && env "${driver[@]}" "${runner}" --frame-bench "${root}/test/render_tests/scenes/sprites.owl" \
		--project "${root}/sample_project" --backend "${backend}" --frames 30 --warmup 5 --size 640x360 \
		--out "${work}/wayland_${backend}.json") >"${log}" 2>&1
	code=$?
	if [[ ${code} -ne 0 ]] || ! grep -q "Using the wayland platform" "${log}" || grep -q "\[error\]" "${log}"; then
		echo "Wayland smoke test: ${backend} failed (exit ${code}), log ${log}:"
		cat "${log}"
		status=1
	else
		echo "Wayland smoke test: ${backend} presented its frames on weston."
	fi
done
exit "${status}"
