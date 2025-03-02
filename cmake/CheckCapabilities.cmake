
# -------------------------------------------------------
# Check if std::format is available:
# -------------------------------------------------------
file(WRITE "${CMAKE_BINARY_DIR}/check_format.cpp" "
#include <format>
#include <string>

int main() {
    std::string s = std::format(\"Hello, {}!\", \"world\");
    return 0;
}
")
try_compile(${PRJPREFIX}_HAS_STD_FORMAT
        "${CMAKE_BINARY_DIR}/check_format"
        "${CMAKE_BINARY_DIR}/check_format.cpp"
)
if (${PRJPREFIX}_HAS_STD_FORMAT)
    message(STATUS "std::format is available")
    target_compile_definitions(${CMAKE_PROJECT_NAME}_Base INTERFACE ${PRJPREFIX}_HAS_STD_FORMAT)
else ()
    message(STATUS "std::format is available")
endif ()
# -------------------------------------------------------
