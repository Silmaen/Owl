
# Check if Poetry is installed
execute_process(
        COMMAND poetry --version
        RESULT_VARIABLE POETRY_CHECK_RESULT
        OUTPUT_VARIABLE POETRY_VERSION_OUTPUT
        ERROR_VARIABLE POETRY_ERROR_OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE
)

if (POETRY_CHECK_RESULT EQUAL 0)
    message(STATUS "Poetry is installed: ${POETRY_VERSION_OUTPUT}")

    # CI-only escape hatch: wipe every Poetry virtualenv tied to this project before sync.
    #
    # Poetry's default cache lives in `~/.cache/pypoetry/virtualenvs/` and names venvs from
    # `(project name, pyproject, python version)` without the architecture.  CI agents running
    # different architectures on a shared `$HOME` (or a bind-mounted workspace that itself holds
    # the venv) end up sharing a single venv path and installing wheels from the wrong arch —
    # e.g. `cryptography/_rust.abi3.so` compiled for x86_64 then loaded on ARM64 fails with
    # `cannot open shared object file: No such file or directory` because its dynamic deps
    # can't be resolved.  `ci_action.py` diffs the stored platform signature (written by this
    # module after every successful sync — see below) against the live host and sets
    # `OWL_CI_REFRESH_VENV=1` only when they disagree.  Local dev and matching-platform reruns
    # keep the cached venv.
    if (DEFINED ENV{OWL_CI_REFRESH_VENV} AND NOT "$ENV{OWL_CI_REFRESH_VENV}" STREQUAL "0")
        message(STATUS "OWL_CI_REFRESH_VENV set — purging Poetry venv(s) for this project")
        execute_process(
                COMMAND poetry env remove --all --quiet
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                RESULT_VARIABLE POETRY_PURGE_RESULT
                OUTPUT_QUIET
                ERROR_QUIET
        )
        # Non-fatal: the command also exits non-zero when there is nothing to remove.
    endif ()

    execute_process(
            COMMAND poetry sync --no-root
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            RESULT_VARIABLE POETRY_INSTALL_RESULT
            OUTPUT_VARIABLE POETRY_INSTALL_OUTPUT
            ERROR_VARIABLE POETRY_INSTALL_ERROR
    )

    if (NOT POETRY_INSTALL_RESULT EQUAL 0)
        message(WARNING "Poetry sync failed or Poetry not installed. Attempting to continue...")
        message(STATUS "Poetry output: ${POETRY_INSTALL_OUTPUT}")
        message(STATUS "Poetry error: ${POETRY_INSTALL_ERROR}")
    else ()
        # Stamp the venv with the current platform so the next CI run can decide whether to
        # purge.  Delegated to `ci.utils.venv.write_marker` so the signature format stays in a
        # single place (see `ci/utils/venv.py`).
        execute_process(
                COMMAND poetry run python -c "from ci.utils.venv import get_poetry_venv_path, write_marker; p = get_poetry_venv_path(); p and write_marker(p)"
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                RESULT_VARIABLE POETRY_STAMP_RESULT
                OUTPUT_QUIET
                ERROR_QUIET
        )
    endif ()
    set(Poetry_PREFIX poetry run CACHE PATH "Path to the Poetry prefix")
    set(Python3_EXECUTABLE ${Poetry_PREFIX} python CACHE PATH "Path to the Python interpreter managed by Poetry")
    string(REPLACE ";" " " Python3_EXE "${Python3_EXECUTABLE}")
else ()
    message(WARNING "Poetry is not installed. Please install Poetry to manage Building Tools dependencies.
see https://python-poetry.org/docs/#installing-with-the-official-installer")
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    set(Python3_EXE ${Python3_EXECUTABLE})
endif ()

message(STATUS "Using Python interpreter at: ${Python3_EXE} ")
