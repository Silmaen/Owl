
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
