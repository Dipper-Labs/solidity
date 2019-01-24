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

#include <test/libsolidity/util/TestFileParser.h>

#include <test/Options.h>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/throw_exception.hpp>
#include <fstream>
#include <memory>
#include <stdexcept>

using namespace dev;
using namespace langutil;
using namespace solidity;
using namespace dev::solidity::test;
using namespace std;

namespace
{
	void expect(string::iterator& _it, string::iterator _end, string::value_type _c)
	{
		if (_it == _end || *_it != _c)
			throw runtime_error(string("Invalid test expectation. Expected: \"") + _c + "\".");
		++_it;
	}

	template<typename IteratorType>
	void skipWhitespace(IteratorType& _it, IteratorType _end)
	{
		while (_it != _end && isspace(*_it))
			++_it;
	}

	template<typename IteratorType>
	void skipSlashes(IteratorType& _it, IteratorType _end)
	{
		while (_it != _end && *_it == '/')
			++_it;
	}
}

string TestFileParser::bytesToString(bytes const& _bytes, TestFileParser::ByteFormat const& _format)
{
	// TODO: Convert from compact big endian if padded
	bytes byteRange{_bytes};
	stringstream resultStream;
	switch(_format.type)
	{
		case TestFileParser::ByteFormat::SignedDec:
			if (*_bytes.begin() & 0x80)
			{
				for (auto& v: byteRange)
					v ^= 0xFF;
				resultStream << "-" << fromBigEndian<u256>(byteRange) + 1;
			}
			else
				resultStream << fromBigEndian<u256>(byteRange);
			break;
		case TestFileParser::ByteFormat::UnsignedDec:
			resultStream << fromBigEndian<u256>(byteRange);
			break;
	}
	return resultStream.str();
}

pair<bytes, TestFileParser::ByteFormat> TestFileParser::stringToBytes(string _string)
{
	bytes result;
	ByteFormat format;
	auto it = _string.begin();
	while (it != _string.end())
	{
		if (isdigit(*it) || (*it == '-' && (it + 1) != _string.end() && isdigit(*(it + 1))))
		{
			format.type = TestFileParser::ByteFormat::UnsignedDec;

			bool isNegative = (*it == '-');
			if (isNegative)
				format.type = TestFileParser::ByteFormat::SignedDec;

			auto valueBegin = it;
			while (it != _string.end() && !isspace(*it) && *it != ',')
				++it;

			bytes newBytes;
			u256 numberValue(string(valueBegin, it));
			// TODO: Convert to compact big endian if padded
			if (numberValue == u256(0))
				newBytes = bytes{0};
			else
				newBytes = toBigEndian(numberValue);
			result += newBytes;
		}
		else
			BOOST_THROW_EXCEPTION(runtime_error("Test expectations contain invalidly formatted data."));

		skipWhitespace(it, _string.end());
		if (it != _string.end())
			expect(it, _string.end(), ',');
		skipWhitespace(it, _string.end());
	}
	return make_pair(result, format);
}

vector<TestFileParser::FunctionCall> TestFileParser::parseFunctionCalls()
{
	vector<TestFileParser::FunctionCall> calls;
	while (advanceLine())
	{
		if (m_scanner.eol())
			continue;

		TestFileParser::FunctionCall call;
		call.signature = parseFunctionCallSignature();
		call.value = parseFunctionCallValue();
		call.arguments = parseFunctionCallArgument();

		if (!advanceLine())
			throw runtime_error("Invalid test expectation. No result specified.");

		call.expectations = parseFunctionCallExpectations();

		if (call.expectations.status)
			call.expectations.output = "-> " + call.expectations.raw;
		else
			call.expectations.output = "REVERT";

		calls.emplace_back(std::move(call));
	}
	return calls;
}

string TestFileParser::parseFunctionCallSignature()
{
	auto signatureBegin = m_scanner.position();
	while (!m_scanner.eol() && m_scanner.current() != ')')
		m_scanner.advance();
	expectCharacter(')');

	return string{signatureBegin, m_scanner.position()};
}

TestFileParser::FunctionCallArgs TestFileParser::parseFunctionCallArgument()
{
	skipWhitespaces();

	FunctionCallArgs arguments;
	if (!m_scanner.eol())
	{
		if (m_scanner.current() != '#')
		{
			expectCharacter(':');
			skipWhitespaces();

			auto argumentBegin = m_scanner.position();
			// TODO: allow # in quotes
			while (!m_scanner.eol() && m_scanner.current() != '#')
				m_scanner.advance();
			arguments.raw = string{argumentBegin, m_scanner.position()};
			boost::algorithm::trim(arguments.raw);

			auto bytesFormat = stringToBytes(arguments.raw);
			arguments.rawBytes = bytesFormat.first;
			arguments.format = bytesFormat.second;
		}

		if (!m_scanner.eol())
		{
			expectCharacter('#');
			skipWhitespaces();
			arguments.comment = string(m_scanner.position(), m_scanner.endPosition());
		}
	}
	return arguments;
}

TestFileParser::FunctionCallExpectations TestFileParser::parseFunctionCallExpectations()
{
	FunctionCallExpectations result;
	if (!m_scanner.eol() && m_scanner.current() == '-')
	{
		expectCharacter('-');
		expectCharacter('>');
		skipWhitespaces();

		auto expectedResultBegin = m_scanner.position();
		// TODO: allow # in quotes
		while (!m_scanner.eol() && m_scanner.current() != '#')
			m_scanner.advance();
		result.raw = string{expectedResultBegin, m_scanner.position()};
		boost::algorithm::trim(result.raw);

		auto bytesFormat = stringToBytes(result.raw);
		result.rawBytes = bytesFormat.first;
		result.format = bytesFormat.second;

		result.status = true;

		if (!m_scanner.eol())
		{
			expectCharacter('#');
			skipWhitespaces();
			result.comment = string(m_scanner.position(), m_scanner.endPosition());
		}
	}
	else
	{
		for (char c: string("REVERT"))
			expectCharacter(c);
		result.status = false;
	}
	return result;
}

u256 TestFileParser::parseFunctionCallValue()
{
	u256 cost;
	if (!m_scanner.eol() && m_scanner.current() == '[')
	{
		m_scanner.advance();
		auto etherBegin = m_scanner.position();
		while (!m_scanner.eol() && m_scanner.current() != ']')
			m_scanner.advance();
		string etherString(etherBegin, m_scanner.position());
		cost = u256(etherString);
		expectCharacter(']');
	}
	return cost;
}


bool TestFileParser::advanceLine()
{
	bool success = m_scanner.advanceLine();

	skipWhitespaces();

	skipSlashes(m_scanner.position(), m_scanner.endPosition());
	skipWhitespaces();

	return success;
}

void TestFileParser::expectCharacter(char const _char)
{
	expect(m_scanner.position(), m_scanner.endPosition(), _char);
}

void TestFileParser::skipWhitespaces()
{
	skipWhitespace(m_scanner.position(), m_scanner.endPosition());
}
