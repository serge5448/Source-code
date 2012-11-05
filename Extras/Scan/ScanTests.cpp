//===============================================================================
//
// Microsoft Press
// C++ AMP: Accelerated Massive Parallelism with Microsoft Visual C++
//
//===============================================================================
// Copyright (c) 2012 Ade Miller & Kate Gregory.  All rights reserved.
// This code released under the terms of the 
// Microsoft Public License (Ms-PL), http://ampbook.codeplex.com/license.
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//===============================================================================

#include "stdafx.h"

#include "Scan.h"
#include "Utilities.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Extras;

namespace ScanTests
{
    std::wstring Msg(std::vector<int>& expected, std::vector<int>& actual)
    {
        std::wostringstream msg;
        msg << ContainerWidth(8) << L"[" << expected << L"] != [" << actual << L"]" << std::endl;
        return msg.str();
    }

	TEST_CLASS(ScanTests)
	{
	public:
        TEST_METHOD(PrescanTests_Simple)
		{
            std::vector<int> input(8, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 0);

            Prescan(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result).c_str());
		}

        TEST_METHOD(PrescanTests_Complex)
		{
			std::array<int, 8> input =    { 1, 3, 6,  2,  7,  9,  0,  5 };
            std::vector<int> result(8);
			std::array<int, 8> expected = { 0, 1, 4, 10, 12, 19, 28, 28 };

            Prescan(begin(input), end(input), result.begin());
            
			std::vector<int> exp(begin(expected), end(expected));
            Assert::IsTrue(exp == result, Msg(exp, result).c_str());
		}

        TEST_METHOD(ScanTests_Simple)
		{
            std::vector<int> input(8, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 1);

            Scan(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result).c_str());
		}

		TEST_METHOD(ScanTests_Complex)
		{
			std::array<int, 8> input =    { 1, 3,  6,  2,  7,  9,  0,  5 };
            std::vector<int> result(8);
			std::array<int, 8> expected = { 1, 4, 10, 12, 19, 28, 28, 33 };

            Scan(begin(input), end(input), result.begin());
            
			std::vector<int> exp(begin(expected), end(expected));
            Assert::IsTrue(exp == result, Msg(exp, result).c_str());
		}
	};

	TEST_CLASS(ScanAmpTests)
	{
	public:

		TEST_METHOD(PrescanAmpSimpleTests_Simple)
		{
            std::vector<int> input(8, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 0);

            PrescanAmpSimple(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result).c_str());
		}

        TEST_METHOD(ScanAmpSimpleTests_Simple)
		{
            std::vector<int> input(8, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 1);

            ScanAmpSimple(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result).c_str());
		}

        TEST_METHOD(ScanAmpSimpleTests_Complex)
		{
			std::array<int, 8> input =    { 1, 3,  6,  2,  7,  9,  0,  5 };
            std::vector<int> result(8);
			std::array<int, 8> expected = { 1, 4, 10, 12, 19, 28, 28, 33 };

            ScanAmpSimple(begin(input), end(input), result.begin());
            
			std::vector<int> exp(begin(expected), end(expected));
            Assert::IsTrue(exp == result, Msg(exp, result).c_str());
		}

		TEST_METHOD(ScanAmpTests_Large)
		{
            std::vector<int> input(2048, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 1);

			ScanAmpSimple(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result).c_str());
		}
	};

    TEST_CLASS(ScanAmpTiledTests)
	{
	public:

		TEST_METHOD(PrescanAmpTiledTests_Simple)
		{
            std::vector<int> input(8, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 0);

            PrescanAmpTiled(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result).c_str());
		}

        TEST_METHOD(ScanAmpTiledTests_Simple)
		{
            std::vector<int> input(8, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 1);

            ScanAmpTiled(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result).c_str());
		}

        TEST_METHOD(ScanAmpTiledTests_Complex)
		{
			std::array<int, 8> input =    { 1, 3,  6,  2,  7,  9,  0,  5 };
            std::vector<int> result(8);
			std::array<int, 8> expected = { 1, 4, 10, 12, 19, 28, 28, 33 };

            ScanAmpTiled(begin(input), end(input), result.begin());
            
			std::vector<int> exp(begin(expected), end(expected));
            Assert::IsTrue(exp == result, Msg(exp, result).c_str());
		}

		TEST_METHOD(ScanAmpTiledTests_Large)
		{
            std::vector<int> input(2048, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 1);

			ScanAmpTiled(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result).c_str());
		}
	};
}
