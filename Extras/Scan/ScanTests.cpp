#include "stdafx.h"

#include <CppUnitTest.h>
#include "Scan.h"

using namespace std;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Examples;

namespace ScanTests
{		
	TEST_CLASS(ScanTests)
	{

	public:
        TEST_METHOD(ScanTests_Sequential)
		{
            vector<int> input(5, 1);
            vector<int> result(5);
            vector<int> expected(5);

            std::iota(expected.begin(), expected.end(), 1);

            Scan(input.begin(), input.end(), result.begin());
            
            Assert::IsTrue(expected == result);
		}
	};
}