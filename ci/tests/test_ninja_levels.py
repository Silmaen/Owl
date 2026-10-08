"""Log level given to the lines of a Ninja build (only real errors are errors)."""

from logging import ERROR, INFO, WARNING

from ci.utils.run import _ninja_level


def test_compiler_and_build_errors_are_errors() -> None:
    assert _ninja_level("src/a.cpp:12:3: error: use of undeclared identifier 'x'") == ERROR
    assert _ninja_level("src/a.cpp:1:10: fatal error: 'b.h' file not found") == ERROR
    assert _ninja_level("FAILED: [code=1] source/owl/CMakeFiles/a.cpp.o") == ERROR
    assert _ninja_level("ninja: build stopped: subcommand failed.") == ERROR
    assert _ninja_level("/usr/bin/ld: a.o: undefined reference to `foo()'") == ERROR


def test_colours_do_not_hide_an_error() -> None:
    assert _ninja_level("\x1b[1msrc/a.cpp:12:3: \x1b[0m\x1b[0;1;31merror: \x1b[0mbad") == ERROR


def test_warnings_are_warnings() -> None:
    assert _ninja_level("src/a.cpp:4:1: warning: unused variable 'y' [-Wunused-variable]") == WARNING


def test_progress_notes_and_commands_are_information() -> None:
    assert _ninja_level("[12/400] Building CXX object source/owl/CMakeFiles/OwlEngine.dir/a.cpp.o") == INFO
    assert _ninja_level("src/a.cpp:4:1: note: previous definition is here") == INFO
    assert _ninja_level("In file included from src/a.cpp:3:") == INFO
    assert _ninja_level("/usr/bin/clang++ -Werror -c src/error_handler.cpp") == INFO
