// ScanPerf.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "Timer.h"
#include "..\Scan\Scan.h"

using namespace Extras;

class IScan
{
public:
    virtual void Scan(array_view<int, 1>(in), array_view<int, 1>(out)) const = 0;
};

class DummyScan : public IScan
{
public:
    void Scan(array_view<int, 1>(in), array_view<int, 1>(out)) const { }
};

class SimpleScan : public IScan
{
public:
    void Scan(array_view<int, 1>(in), array_view<int, 1>(out)) const
    {
		ComputeScanAmp(array_view<int, 1>(in), array_view<int, 1>(out));
    }
};

typedef std::pair<std::shared_ptr<IScan>, std::wstring> ScanDescription;

int _tmain(int argc, _TCHAR* argv[])
{
	std::array<ScanDescription, 2> scans = {
        ScanDescription(std::make_shared<DummyScan>(),    L"Overhead"),
        ScanDescription(std::make_shared<SimpleScan>(),   L"Simple") };

    accelerator_view view = accelerator(accelerator::default_accelerator).default_view;

	std::vector<int> input(2048, 1);
	std::vector<int> result(input.size());
	std::vector<int> expected(input.size());
	std::iota(begin(expected), end(expected), 1);

	for (ScanDescription s : scans)
	{
		IScan* scanImpl = s.first.get();
        std::wstring scanName = s.second;

		std::fill(begin(input), end(input), 1);
		std::fill(begin(result), end(result), 0);

		double computeTime = 0.0, totalTime = 0.0;
        totalTime = JitAndTimeFunc(view, [&]()
        {
			concurrency::array<int, 1> in(input.size());
			concurrency::array<int, 1> out(input.size());
			copy(begin(input), end(input), in);

			computeTime = TimeFunc(view, [&]()
			{
				scanImpl->Scan(array_view<int, 1>(in), array_view<int, 1>(out));
			});
			copy(out, begin(result));
		});

        std::wcout << "SUCCESS: " << scanName;
        std::wcout.width(std::max(0U, 55 - scanName.length()));
        std::wcout << std::right << std::fixed << std::setprecision(2) << totalTime << " : " << computeTime << " (ms)" << std::endl;        
	}
	return 0;
}
