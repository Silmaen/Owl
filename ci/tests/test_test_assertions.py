"""Detection of gtest tests that assert nothing (CodeStyle `test-assertions`)."""

from pathlib import Path

from ci.utils.test_assertions import empty_tests


def test_only_tests_without_any_check_are_reported(tmp_path: Path) -> None:
    source = tmp_path / "a_test.cpp"
    source.write_text(
        "TEST(Suite, Checks) {\n  EXPECT_EQ(1, 1);\n}\n"
        "TEST_F(Fixture, OnlyRuns) {\n  int a = 0;\n  if (a) { a = 1; }\n}\n"
        "TEST(Suite, Smoke) {\n  EXPECT_NO_THROW(run());\n}\n"
        "TEST(Suite, Helper) {\n  expectSameMesh(a, b);\n}\n"
        'TEST_P(Param, Skips) {\n  GTEST_SKIP() << "no GPU";\n}\n'
    )

    assert [(t.line, t.name) for t in empty_tests([source])] == [(4, "Fixture.OnlyRuns")]
