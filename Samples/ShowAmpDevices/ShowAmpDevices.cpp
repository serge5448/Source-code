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

#include <tchar.h>
#include <SDKDDKVer.h>
#include <iostream>
#include <vector>
#include <amp.h>

using namespace concurrency;

int _tmain(int argc, _TCHAR* argv[])
{
    std::vector<accelerator> accls = accelerator::get_all();
    accls.erase(std::remove_if(accls.begin(), accls.end(), [](accelerator& a) { 
        return (a.device_path == accelerator::cpu_accelerator) || (a.device_path == accelerator::direct3d_ref); 
    }), accls.end());

    if (accls.empty())
    {
        std::wcout << "No accelerators found that are compatible with C++ AMP" << std::endl << std::endl;
        return 0;
    }
    std::wcout << std::endl << "Found " << accls.size() 
        << " accelerator device(s) that are compatible with C++ AMP:" << std::endl;
    int n = 0;
    std::for_each(accls.cbegin(), accls.cend(), [=, &n](const accelerator& a)
    {
        std::wcout << "  " << ++n << ": " << a.description 
            << ", has_display=" << (a.has_display ? "true" : "false") 
            << ", is_emulated=" << (a.is_emulated ? "true" : "false") 
            << std::endl;
    });
    std::wcout << std::endl;
	return 1;
}
