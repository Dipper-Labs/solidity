/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
/**
 * Unit tests for Solidity's test expectation parser.
 */

#include <functional>
#include <string>
#include <tuple>
#include <boost/test/unit_test.hpp>
#include <liblangutil/Exceptions.h>
#include <test/libsolidity/SolidityExecutionFramework.h>

#include <test/libsolidity/util/TestFileParser.h>

using namespace std;
using namespace dev::test;

namespace dev
{
namespace solidity
{
namespace test
{

using FunctionCall = TestFileParser::FunctionCall;
using Expectation = TestFileParser::FunctionCallExpectations;

vector<FunctionCall> parse(string const& _source)
{
	istringstream stream{_source, ios_base::out};
	TestFileParser parser{stream};
	return parser.parseFunctionCalls();
}

BOOST_AUTO_TEST_SUITE(TestFileParserTest)

BOOST_AUTO_TEST_CASE(smoke_test)
{
	char const* source = R"()";
	BOOST_CHECK_EQUAL(parse(source).size(), 0);
}

BOOST_AUTO_TEST_CASE(simple_function_call_success)
{
	char const* source = R"(
		// f()
		// -> 1
	)";
	BOOST_CHECK_EQUAL(parse(source).size(), 1);
	BOOST_CHECK_EQUAL(parse(source).at(0).signature, "f()");
	BOOST_CHECK_EQUAL(parse(source).at(0).expectations.output, "-> 1");
	BOOST_CHECK_EQUAL(parse(source).at(0).expectations.raw, "1");
}

BOOST_AUTO_TEST_CASE(simple_function_call_revert)
{
	char const* source = R"(
		// i_am_not_there()
		// REVERT
	)";
	BOOST_CHECK_EQUAL(parse(source).size(), 1);
	BOOST_CHECK_EQUAL(parse(source).at(0).signature, "i_am_not_there()");
	BOOST_CHECK_EQUAL(parse(source).at(0).expectations.output, "REVERT");
	BOOST_CHECK_EQUAL(parse(source).at(0).expectations.raw, "");
}

BOOST_AUTO_TEST_CASE(simple_function_call_comments)
{
	char const* source = R"(
		// f() # This is a comment
		// -> 1 # This is another comment
	)";
	BOOST_CHECK_EQUAL(parse(source).size(), 1);
	BOOST_CHECK_EQUAL(parse(source).at(0).signature, "f()");
	BOOST_CHECK_EQUAL(parse(source).at(0).arguments.comment, "This is a comment");
	BOOST_CHECK_EQUAL(parse(source).at(0).expectations.output, "-> 1");
	BOOST_CHECK_EQUAL(parse(source).at(0).expectations.raw, "1");
	BOOST_CHECK_EQUAL(parse(source).at(0).expectations.comment, "This is another comment");
}

BOOST_AUTO_TEST_CASE(function_call_arguments)
{
	char const* source = R"(
		// f(uint256): 1
		// -> 1
	)";
	BOOST_CHECK_EQUAL(parse(source).size(), 1);
	BOOST_CHECK_EQUAL(parse(source).at(0).signature, "f(uint256)");
	BOOST_CHECK_EQUAL(parse(source).at(0).arguments.raw, "1");
	BOOST_CHECK_EQUAL(parse(source).at(0).expectations.output, "-> 1");
	BOOST_CHECK_EQUAL(parse(source).at(0).expectations.raw, "1");
}


BOOST_AUTO_TEST_SUITE_END()

}
}
}
