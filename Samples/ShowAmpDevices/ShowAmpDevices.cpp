// ShowAmpDevices.cpp : Defines the entry point for the console application.
//

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
        return a.device_path == accelerator::cpu_accelerator; 
    }), accls.end());
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
	return 0;
}
