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

#include <iostream>
#include <amp.h>
#include <math.h>

#include "Timer.h"

using namespace Concurrency;
using std::vector;

void InitializeArray(vector<float> &vM, int size)
{
    for(int i=0; i<size; ++i)
    {
        vM[i] = (float)rand() / (float)(RAND_MAX + 1);
    }
}


int main()
{
    srand(2010);

    const int M = 64;
    const int N = 64;
    const int W = 64;

    accelerator defaultacc (accelerator::default_accelerator);
    accelerator_view defaultView = defaultacc.default_view;

#ifndef _DEBUG
    std::vector<accelerator> allAccelerators = accelerator::get_all();
    allAccelerators.erase(std::remove_if(allAccelerators.begin(), allAccelerators.end(), 
        [](const accelerator& acc){ return (acc.is_emulated) || (acc.device_path == accelerator::cpu_accelerator);} ),allAccelerators.end());

    if (allAccelerators.size() > 0)
        defaultView = allAccelerators[0].default_view;
#endif

    vector<float> vA(M * W);
    vector<float> vB(W * N);
    vector<float> vC(M * N);
    vector<float> vRef(M * N);

    InitializeArray(vA, M * W);
    InitializeArray(vB, W * N);

    double elapsedTime = TimeFunc([&]()
    {
        // Compute mxm on CPU
        for(int row=0; row<M; ++row)
        {
            for(int col=0; col<N; ++col)
            {
                float result = 0.0f;

                for(int i=0; i<W; ++i)
                {
                    int idxA = row * W + i;
                    int idxB = i * N + col;

                    result += vA[idxA] * vB[idxB];
                }

                vRef[row * N + col] = result;
            }
        }
    });

    std::wcout << "CPU exec time: " << elapsedTime << " (ms)" << std::endl << std::endl;

    elapsedTime = TimeFunc([&]()
    {
        extent<2> eA(M, W), eB(W, N), eC(M, N);
        array_view<float, 2> mA(eA, vA); 
        array_view<float, 2> mB(eB, vB); 
        array_view<float, 2> mC(eC, vC);

        mC.discard_data();
        parallel_for_each(defaultView,extent<2>(eC), [=](index<2> idx) restrict(amp)
        {
            float result = 0.0f;

            for(int i = 0; i < mA.extent[1]; ++i)
            {
                index<2> idxA(idx[0], i);
                index<2> idxB(i, idx[1]);

                result += mA[idxA] * mB[idxB];
            }

            mC[idx] = result;
        });

        mC.synchronize();
    });

    std::wcout << "GPU exec time (non tiled) including copy-in/out: " << elapsedTime << " (ms)" << std::endl << std::endl;

    // Compare GPU non tiled and CPU results
    bool passed = true;

    for (int i=0; i<M * N; ++i)
    {
        if (fabs(vC[i] - vRef[i]) > 0.01)
        {
            std::wcout << "vC[" << i << "] = " << vC[i] << ", vRef[" << i << "] = " << vRef[i] << std::endl;
            passed = false;
            break;
        }
    }

    std::wcout << " non tiled " << (passed ? "PASSED" : "FAILED") << std::endl;

    static const int TS = 16;

    elapsedTime = TimeFunc([&]()
    {
        array_view<const float,2> a(M, W, vA);
        array_view<const float,2> b(W, N, vB);
        array_view<float,2> c(M, N, vC);

        c.discard_data();
        parallel_for_each(c.extent.tile< TS, TS >(),
            [=] (tiled_index< TS, TS> tidx) restrict(amp) 
        {
            int row = tidx.local[0]; 
            int col = tidx.local[1];
            float sum = 0.0f;
            for (int i = 0; i < W; i += TS) 
            {
                tile_static float sA[TS][TS], sB[TS][TS];
                sA[row][col] = a(tidx.global[0], col + i);
                sB[row][col] = b(row + i, tidx.global[1]);
                tidx.barrier.wait();
                for (int k = 0; k < TS; k++)
                    sum += sA[row][k] * sB[k][col];
                tidx.barrier.wait();
            }
            c[tidx.global] = sum;
        });

        c.synchronize();
    });

    std::wcout << "GPU exec time (tiled - tile size is " << TS << ") including copy-in/out: " <<
        elapsedTime << " (ms)" << std::endl << std::endl;

    // Compare tiled GPU and CPU results
    passed = true;

    for (int i=0; i<M * N; ++i)
    {
        if (fabs(vC[i] - vRef[i]) > 0.01)
        {
            std::wcout << "vC[" << i << "] = " << vC[i] << ", vRef[" << i << "] = " << vRef[i] << std::endl;
            passed = false;
            break;
        }
    }

    std::wcout << " non tiled " << (passed ? "PASSED" : "FAILED") << std::endl;

    return passed ? 0 : 1;
}
