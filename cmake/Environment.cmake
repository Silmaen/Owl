# Define the path for the file containing environment variables
set(ENV_PATH "${PROJECT_SOURCE_DIR}/.env")

# read the environment variables define in .env file.
if (EXISTS ${ENV_PATH})
    message(STATUS "Loading environment variables from: ${ENV_PATH}")
    file(READ ${ENV_PATH} ENV_CONTENTS)
    string(REPLACE "\n" ";" ENV_LINES "${ENV_CONTENTS}")
    foreach (LINE IN LISTS ENV_LINES)
        if (LINE MATCHES "^[ ]*#")  # Skip comments
            continue()
        endif ()
        if (LINE MATCHES "^[ ]*$")  # Skip empty lines
            continue()
        endif ()
        string(REGEX MATCH "^[ ]*([^=]+)=(.*)$" _ "${LINE}")
        if (CMAKE_MATCH_1 AND CMAKE_MATCH_2)
            string(STRIP "${CMAKE_MATCH_1}" ENV_KEY)
            string(STRIP "${CMAKE_MATCH_2}" ENV_VALUE)
            set(ENV{${ENV_KEY}} "${ENV_VALUE}")
            message(STATUS "Loaded env variable: ${ENV_KEY}=${ENV_VALUE}")
        endif ()
    endforeach ()
else ()
    message(STATUS "No .env file found at: ${ENV_PATH}")
endif ()
