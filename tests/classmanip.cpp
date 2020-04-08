#include "common.hpp"
#include <catch2/catch.hpp>

TEST_CASE("string handling", "[str]")
{
	REQUIRE(run("stringmanip.as", "void string_ref()") == "hello\n");
	REQUIRE(run("stringmanip.as", "void string_concat()") == "hello world\n");
	REQUIRE(run("stringmanip.as", "void string_concat2()") == "hello world\n");
	REQUIRE(run("stringmanip.as", "void string_manylocals_concat()") == "hello world\n");
	REQUIRE(run("stringmanip.as", "void string_function_reference()") == "hello\n");
	// REQUIRE(run("stringmanip.as", "void string_function_value()") == "hello\n");
}