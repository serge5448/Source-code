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

#include "..\Scan\ScanSimple.h"
#include "..\Scan\ScanTiled.h"

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
        ScanAmpSimple(array_view<int, 1>(in), array_view<int, 1>(out));
    }
};

class TiledScan : public IScan
{
public:
    void Scan(array_view<int, 1>(in), array_view<int, 1>(out)) const
    {
        ScanAmpTiled<256>(array_view<int, 1>(in), array_view<int, 1>(out));
    }
};

typedef std::pair<std::shared_ptr<IScan>, std::wstring> ScanDescription;

int _tmain(int argc, _TCHAR* argv[])
{
    std::array<ScanDescription, 3> scans = {
        ScanDescription(std::make_shared<DummyScan>(),    L"Overhead"),
        ScanDescription(std::make_shared<SimpleScan>(),   L"Simple"),
        ScanDescription(std::make_shared<TiledScan>(),    L"Tiled") };

    accelerator_view view = accelerator(accelerator::default_accelerator).default_view;

    const size_t elementCount = 4 * 1024 * 1024;

    std::vector<int> input(elementCount, 1);
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
