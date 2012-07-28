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

#include <amp.h>
#include <assert.h>
#include <amp_math.h>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <random>
#include <numeric>
#include <amp_graphics.h>

#include "Timer.h"

using namespace concurrency;

void FillExample();

void TransposeExample(int matrixSize);
void TransposePaddedExample(int matrixSize);
void TransposeTruncatedExample(int matrixSize);
void TransposeTruncatedExample2(int matrixSize);

//--------------------------------------------------------------------------------------
//  Print a subsection of a matrix. 
//  The top left 10 x 10 region or the whole matrix, whichever is smaller.
//--------------------------------------------------------------------------------------

template<typename T>
void PrintMatrix(const T* const data, int size)
{
#ifndef _DEBUG
    return;
#endif
    for (int i = 0; i < min(10, size); ++i)
    {
        for (int j = 0; j < min(10, size); ++j)
            std::wcout << data[i * size + j] << " ";
        std::wcout << std::endl;
    }
}

template<typename T>
void CheckMatrix(const T* const data, int size)
{
    for (int i = 0; i < size; ++i)
        for (int j = 0; j < size; ++j)
        {
            T expected = j * size + i;
            if (data[i * size + j] != expected)
            {
                std::wcout << "Error! [" << i << ", " << j << "] expected " 
                    << expected << " but found " << data[i * size + j] << std::endl;
                return;
            }
        }
}

static const int tileSize = 32;

int main()
{
    std::wcout << std::fixed << std::setprecision(3);

    // Most of the samples do this check first but that would alter the FullTimingExample.
   
#ifndef _DEBUG
    accelerator defaultDevice;
    std::wcout << L" Using device : " << defaultDevice.get_description() << std::endl;
    if (defaultDevice == accelerator(accelerator::direct3d_ref))
        std::wcout << " WARNING!! No C++ AMP hardware accelerator detected, using the REF accelerator." << std::endl << 
            "To see better performance run on C++ AMP\ncapable hardware." << std::endl;
#endif

    FillExample();

    int size = tileSize * 4;
    TransposeExample(size);
    TransposeExample(size + tileSize);

    TransposePaddedExample(size + 1);
    TransposePaddedExample(size + tileSize / 2);
    TransposePaddedExample(size + tileSize - 1);

    TransposeTruncatedExample(size + 1);
    TransposeTruncatedExample(size + tileSize / 2);
    TransposeTruncatedExample(size + tileSize - 1);

    TransposeTruncatedExample2(size + 1);
    TransposeTruncatedExample2(size + tileSize / 2);
    TransposeTruncatedExample2(size + tileSize - 1);

    std::wcout << std::endl << std::endl;
}

//--------------------------------------------------------------------------------------
//  Fill example.
//--------------------------------------------------------------------------------------

template<typename T, int Rank>
void Fill(array<T, Rank>& arr, T value) 
{ 
    parallel_for_each(arr.extent, [&arr, value](index<Rank> idx) restrict(amp)
    {
        arr[idx] = value; 
    }); 
}

void FillExample()
{
    array<float, 2> theData(100, 100);
    Fill(theData, 1.5f);
}

//--------------------------------------------------------------------------------------
//  Tiled matrix transpose example.
//--------------------------------------------------------------------------------------

void TransposeExample(int matrixSize)
{ 
    if (matrixSize % tileSize != 0)
        throw std::exception("matrix is not a multiple of tile size.");

    std::vector<unsigned int> inData(matrixSize * matrixSize);
    std::vector<unsigned int> outData(matrixSize * matrixSize, 0);
    unsigned int v = 0;
    std::generate(inData.begin(), inData.end(), [&v]() { return v++; });

    array_view<const unsigned int, 2> inDataView(matrixSize, matrixSize, inData);
    array_view<unsigned int, 2> outDataView(matrixSize, matrixSize, outData);
    outDataView.discard_data();

    tiled_extent<tileSize, tileSize> computeDomain = inDataView.extent.tile<tileSize, tileSize>();
    accelerator_view view = accelerator(accelerator::default_accelerator).default_view;
    double elapsedTime = TimeFunc(view, [&]() 
    {
        parallel_for_each(computeDomain, [=](tiled_index<tileSize, tileSize> tidx) restrict(amp)
        {     
            tile_static unsigned int localData[tileSize][tileSize];
            localData[tidx.local[1]][tidx.local[0]] = inDataView[tidx.global];

            tidx.barrier.wait();

            index<2> outIdx(index<2>(tidx.tile_origin[1], tidx.tile_origin[0]) + tidx.local);
            outDataView[outIdx] = localData[tidx.local[0]][tidx.local[1]];
        });
    });
    outDataView.synchronize();
    std::wcout << "Transpose exact size" << std::endl;
    std::wcout << "Matrix size " << matrixSize << " x " << matrixSize << std::endl;
    std::wcout << "Elapsed time " << elapsedTime << " ms" << std::endl;
    CheckMatrix(static_cast<const unsigned int* const>(outData.data()), matrixSize);
    PrintMatrix(static_cast<const unsigned int* const>(outData.data()), matrixSize);
    std::wcout << std::endl;
}

//--------------------------------------------------------------------------------------
//  Tiled and padded matrix transpose example.
//--------------------------------------------------------------------------------------

template <typename T, unsigned int Rank>
T PaddedRead(const array_view<const T, Rank>& A, const index<Rank>& idx) restrict(cpu, amp)
{
    return A.extent.contains(idx) ? A[idx] : T();
}

template <typename T, unsigned int Rank>
void PaddedWrite(const array_view<T, Rank>& A, const index<Rank>& idx, const T& val) restrict(cpu, amp)
{
    if (A.extent.contains(idx))
        A[idx] = val;
}

void TransposePaddedExample(int matrixSize)
{
    std::vector<unsigned int> inData(matrixSize * matrixSize);
    std::vector<unsigned int> outData(matrixSize * matrixSize, 0);
    unsigned int v = 0;
    std::generate(inData.begin(), inData.end(), [&v]() { return v++; });

    array_view<const unsigned int, 2> inDataView(matrixSize, matrixSize, inData);
    array_view<unsigned int, 2> outDataView(matrixSize, matrixSize, outData);
    outDataView.discard_data();

    tiled_extent<tileSize, tileSize> computeDomain = inDataView.extent.tile<tileSize, tileSize>();
    computeDomain = computeDomain.pad();
    accelerator_view view = accelerator(accelerator::default_accelerator).default_view;
    double elapsedTime = TimeFunc(view, [&]() 
    {
        parallel_for_each(view, computeDomain, 
            [=](tiled_index<tileSize, tileSize> tidx) restrict(amp)
        {     
            tile_static unsigned int localData[tileSize][tileSize];
            localData[tidx.local[1]][tidx.local[0]] = PaddedRead(inDataView, tidx.global);

            tidx.barrier.wait();

            index<2> outIdx(index<2>(tidx.tile_origin[1], tidx.tile_origin[0]) + tidx.local);
            PaddedWrite(outDataView, outIdx, localData[tidx.local[0]][tidx.local[1]]);
        });
    });
    
    outDataView.synchronize();
    std::wcout << "Transpose padded" << std::endl;
    std::wcout << "Matrix size " << matrixSize << " x " << matrixSize << ", padded size " 
        << computeDomain[0] << " x " << computeDomain[0] << std::endl;
    std::wcout << "Elapsed time " << elapsedTime << " ms" << std::endl;
    CheckMatrix(static_cast<const unsigned int* const>(outData.data()), matrixSize);
    PrintMatrix(static_cast<const unsigned int* const>(outData.data()), matrixSize);
    std::wcout << std::endl;
}

//--------------------------------------------------------------------------------------
//  Tiled and truncated matrix transpose example.
//--------------------------------------------------------------------------------------

void TransposeTruncatedExample(int matrixSize)
{
    std::vector<unsigned int> inData(matrixSize * matrixSize);
    std::vector<unsigned int> outData(matrixSize * matrixSize, 0);
    unsigned int v = 0;
    std::generate(inData.begin(), inData.end(), [&v]() { return v++; });

    array_view<const unsigned int, 2> inDataView(matrixSize, matrixSize, inData);
    array_view<unsigned int, 2> outDataView(matrixSize, matrixSize, outData);
    outDataView.discard_data();

    tiled_extent<tileSize, tileSize> computeDomain = inDataView.extent.tile<tileSize, tileSize>();
    computeDomain = computeDomain.truncate();

    // TODO_AMP: TDRs in release mode on RC build.
    accelerator_view view = accelerator(accelerator::default_accelerator).default_view;
    double elapsedTime = TimeFunc(view, [&]() 
    {
        parallel_for_each(computeDomain, [=](tiled_index<tileSize, tileSize> tidx) restrict(amp)
        {     
            tile_static unsigned int localData[tileSize][tileSize];
            localData[tidx.local[1]][tidx.local[0]] = inDataView[tidx.global];
            tidx.barrier.wait();
            index<2> outIdx(index<2>(tidx.tile_origin[1], tidx.tile_origin[0]) + tidx.local);
            outDataView[outIdx] = localData[tidx.local[0]][tidx.local[1]];

            // Handle tuncated elements using edge thread.

            bool isRightMost = tidx.global[1] == computeDomain[1] - 1; 
            bool isBottomMost = tidx.global[0] == computeDomain[0] - 1; 

            // Exit branching as quickly as possible as the majority of threads will not meet the boundary criteria
            if (isRightMost | isBottomMost)
            {
                int idx0, idx1;
                if (isRightMost)
                {
                    idx0 = tidx.global[0]; 
                    for (idx1 = computeDomain[1]; idx1 < inDataView.extent[1]; idx1++) 
                        outDataView(idx1, idx0) = inDataView(idx0, idx1); 
                }
                if (isBottomMost)
                {
                    idx1 = tidx.global[1];
                    for (idx0 = computeDomain[0]; idx0 < inDataView.extent[0]; idx0++) 
                        outDataView(idx1, idx0) = inDataView(idx0, idx1); 
                }
                if (isRightMost && isBottomMost) 
                { 
                    for (idx0 = computeDomain[0]; idx0 < inDataView.extent[0]; idx0++)
                        for (idx1 = computeDomain[1]; idx1 < inDataView.extent[1]; idx1++) 
                            outDataView(idx1, idx0) = inDataView(idx0, idx1); 
                }
            }
        });
    });

    outDataView.synchronize();
    std::wcout << "Transpose truncated" << std::endl;
    std::wcout << "Matrix size " << matrixSize << " x " << matrixSize << std::endl;
    std::wcout << "Elapsed time " << elapsedTime << " ms" << std::endl;
    CheckMatrix(static_cast<const unsigned int* const>(outData.data()), matrixSize);
    PrintMatrix(static_cast<const unsigned int* const>(outData.data()), matrixSize);
    std::wcout << std::endl;
}

void TransposeTruncatedExample2(int matrixSize)
{
    std::vector<unsigned int> inData(matrixSize * matrixSize);
    std::vector<unsigned int> outData(matrixSize * matrixSize, 0);
    unsigned int v = 0;
    std::generate(inData.begin(), inData.end(), [&v]() { return v++; });

    array_view<const unsigned int, 2> inDataView(matrixSize, matrixSize, inData);
    array_view<unsigned int, 2> outDataView(matrixSize, matrixSize, outData);
    outDataView.discard_data();

    tiled_extent<tileSize, tileSize> computeDomain = inDataView.extent.tile<tileSize, tileSize>();
    computeDomain = computeDomain.truncate();

    const int rightMargin = computeDomain[1] - computeDomain[1];
    const int bottomMargin = computeDomain[0] - computeDomain[0];
    accelerator_view view = accelerator(accelerator::default_accelerator).default_view;
    double elapsedTime = TimeFunc(view, [&]() 
    {
        parallel_for_each(view, computeDomain, [=](tiled_index<tileSize, tileSize> tidx) restrict(amp)
        {     
            tile_static unsigned int localData[tileSize][tileSize];
            localData[tidx.local[1]][tidx.local[0]] = inDataView[tidx.global];
            tidx.barrier.wait();
            index<2> outIdx(index<2>(tidx.tile_origin[1], tidx.tile_origin[0]) + tidx.local);
            outDataView[outIdx] = localData[tidx.local[0]][tidx.local[1]];

            // Handle truncated elements using threads in the margins.

            bool isRightMost = tidx.global[1] >= computeDomain[1] - rightMargin; 
            bool isBottomMost = tidx.global[0] >= computeDomain[0] - bottomMargin; 

            // Exit branching as quickly as possible as the majority of threads will not meet the boundary criteria
            if (isRightMost | isBottomMost)
            {
                int idx0, idx1;
                if (isRightMost)
                {
                    idx0 = tidx.global[0]; 
                    idx1 = tidx.global[1] + rightMargin;
                    outDataView(idx1, idx0) = inDataView(idx0, idx1);    
                }
                if (isBottomMost)
                {
                    idx1 = tidx.global[1];
                    idx0 = tidx.global[0] + bottomMargin;
                    outDataView(idx1, idx0) = inDataView(idx0, idx1);
                }
                if (isRightMost && isBottomMost) 
                { 
                    idx0 = tidx.global[0] + bottomMargin;
                    idx1 = tidx.global[1] + rightMargin;
                    outDataView(idx1, idx0) = inDataView(idx0, idx1);
                }
            }
        });
    });

    outDataView.synchronize();
    std::wcout << "Transpose truncated" << std::endl;
    std::wcout << "Matrix size " << matrixSize << " x " << matrixSize << std::endl;
    std::wcout << "Elapsed time " << elapsedTime << " ms" << std::endl;
    CheckMatrix(static_cast<const unsigned int* const>(outData.data()), matrixSize);
    PrintMatrix(static_cast<const unsigned int* const>(outData.data()), matrixSize);
    std::wcout << std::endl;
}
