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

#include "ScanSimple.h"
#include "ScanSequential.h"
#include "ScanTiled.h"
#include "ScanTiledOptimized.h"

#include "Utilities.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Extras;

namespace ScanTests
{
    std::wstring Msg(std::vector<int>& expected, std::vector<int>& actual, size_t width = 8)
    {
        std::wostringstream msg;
        msg << ContainerWidth(width) << L"[" << expected << L"] != [" << actual << L"]" << std::endl;
        return msg.str();
    }

	TEST_CLASS(ScanTests)
	{
	public:
        TEST_METHOD(ExclusiveScanTests_Simple)
		{
            std::vector<int> input(8, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 0);

            ExclusiveScan(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result).c_str());
		}

        TEST_METHOD(ExclusiveScanTests_Complex)
		{
			std::array<int, 8> input =    { 1, 3, 6,  2,  7,  9,  0,  5 };
            std::vector<int> result(8);
			std::array<int, 8> expected = { 0, 1, 4, 10, 12, 19, 28, 28 };

            ExclusiveScan(begin(input), end(input), result.begin());
            
			std::vector<int> exp(begin(expected), end(expected));
            Assert::IsTrue(exp == result, Msg(exp, result).c_str());
		}

        TEST_METHOD(InclusiveScanTests_Simple)
		{
            std::vector<int> input(8, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 1);

            InclusiveScan(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result).c_str());
		}

		TEST_METHOD(InclusiveScanTests_Complex)
		{
			std::array<int, 8> input =    { 1, 3,  6,  2,  7,  9,  0,  5 };
            std::vector<int> result(8);
			std::array<int, 8> expected = { 1, 4, 10, 12, 19, 28, 28, 33 };

            InclusiveScan(begin(input), end(input), result.begin());
            
			std::vector<int> exp(begin(expected), end(expected));
            Assert::IsTrue(exp == result, Msg(exp, result).c_str());
		}
	};

	TEST_CLASS(ScanAmpTests)
	{
	public:

		TEST_METHOD(ExclusiveScanAmpSimpleTests_Simple)
		{
            std::vector<int> input(8, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 0);

            ExclusiveScanAmpSimple(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result).c_str());
		}

        TEST_METHOD(InclusiveScanAmpSimpleTests_Simple)
		{
            std::vector<int> input(8, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 1);

            InclusiveScanAmpSimple(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result).c_str());
		}

        TEST_METHOD(InclusiveScanAmpSimpleTests_Complex)
		{
			std::array<int, 8> input =    { 1, 3,  6,  2,  7,  9,  0,  5 };
            std::vector<int> result(8);
			std::array<int, 8> expected = { 1, 4, 10, 12, 19, 28, 28, 33 };

            InclusiveScanAmpSimple(begin(input), end(input), result.begin());
            
			std::vector<int> exp(begin(expected), end(expected));
            Assert::IsTrue(exp == result, Msg(exp, result).c_str());
		}

		TEST_METHOD(InclusiveScanAmpTests_Large)
		{
            std::vector<int> input(2048, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 1);

			InclusiveScanAmpSimple(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result).c_str());
		}
	};

    TEST_CLASS(ScanAmpTiledTests)
	{
	public:

		TEST_METHOD(ExclusiveScanAmpTiledTests_Simple)
		{
            std::vector<int> input(8, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 0);

            ExclusiveScanAmpTiled<4>(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result).c_str());
		}

        TEST_METHOD(InclusiveScanAmpTiledTests_Simple)
		{
            std::vector<int> input(8, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 1);

            InclusiveScanAmpTiled<4>(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result).c_str());
		}

        TEST_METHOD(InclusiveScanAmpTiledTests_Complex)
		{
			std::array<int, 8> input =    { 1, 3,  6,  2,  7,  9,  0,  5 };
            std::vector<int> result(8);
			std::array<int, 8> expected = { 1, 4, 10, 12, 19, 28, 28, 33 };

            InclusiveScanAmpTiled<4>(begin(input), end(input), result.begin());
            
			std::vector<int> exp(begin(expected), end(expected));
            Assert::IsTrue(exp == result, Msg(exp, result).c_str());
		}

        TEST_METHOD(ExclusiveScanAmpTiledTests_Complex)
		{
			std::array<int, 8> input =    { 1, 3, 6,  2,  7,  9,  0,  5 };
            std::vector<int> result(8);
			std::array<int, 8> expected = { 0, 1, 4, 10, 12, 19, 28, 28 };

            ExclusiveScanAmpTiled<4>(begin(input), end(input), result.begin());
            
			std::vector<int> exp(begin(expected), end(expected));
            Assert::IsTrue(exp == result, Msg(exp, result).c_str());
		}

		TEST_METHOD(InclusiveScanAmpTiledTests_Large)
		{
            std::vector<int> input(2048, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 1);

			InclusiveScanAmpTiled<4>(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result).c_str());
		}

		TEST_METHOD(ExclusiveScanAmpTiledTests_Large)
		{
            std::vector<int> input(2048, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 0);

			ExclusiveScanAmpTiled<4>(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result).c_str());
		}
    };

    TEST_CLASS(ScanAmpTiledOptimizedTests)
	{
	public:
		TEST_METHOD(ExclusiveScanAmpTiledOptimizedTests_SimpleOneTile)
		{
            std::vector<int> input(8, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 0);

            ExclusiveScanAmpTiledOptimized<4>(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result).c_str());
		}

		TEST_METHOD(ExclusiveScanAmpTiledOptimizedTests_Simple)
		{
            std::vector<int> input(16, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 0);

            ExclusiveScanAmpTiledOptimized<4>(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result, 12).c_str());
		}

        TEST_METHOD(ScanAmpTiledOptimizedTests_Simple)
		{
            std::vector<int> input(16, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 1);

            InclusiveScanAmpTiledOptimized<4>(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result, 16).c_str());
		}

        TEST_METHOD(ScanAmpTiledOptimizedTests_ComplexOneTile)
		{
			std::array<int, 8> input =    { 1, 3,  6,  2,  7,  9,  0,  5 };
            std::vector<int> result(8);
			std::array<int, 8> expected = { 1, 4, 10, 12, 19, 28, 28, 33 };

            InclusiveScanAmpTiledOptimized<8>(begin(input), end(input), result.begin());
            
			std::vector<int> exp(begin(expected), end(expected));
            Assert::IsTrue(exp == result, Msg(exp, result, 8).c_str());
		}

        TEST_METHOD(ScanAmpTiledOptimizedTests_Complex)
		{
			std::array<int, 8> input =    { 1, 3,  6,  2,  7,  9,  0,  5 };
            std::vector<int> result(8);
			std::array<int, 8> expected = { 1, 4, 10, 12, 19, 28, 28, 33 };

            InclusiveScanAmpTiledOptimized<2>(begin(input), end(input), result.begin());
            
			std::vector<int> exp(begin(expected), end(expected));
            Assert::IsTrue(exp == result, Msg(exp, result).c_str());
		}

		TEST_METHOD(ScanAmpTiledOptimizedTests_Large)
		{
            std::vector<int> input(2048, 1);
            std::vector<int> result(input.size());
            std::vector<int> expected(input.size());
            std::iota(begin(expected), end(expected), 1);

			InclusiveScanAmpTiledOptimized<4>(begin(input), end(input), result.begin());
            
            Assert::IsTrue(expected == result, Msg(expected, result, 12).c_str());
		}
	};

}
