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
void AtomicExample();
void TdrExample();

void TransposeExample(int matrixSize);
void TransposeSimpleExample(int matrixSize);
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

#ifdef _DEBUG
static const int tileSize = 2;
#else
static const int tileSize = 32;
#endif

int main()
{
    std::wcout << std::fixed << std::setprecision(3);

    // Most of the samples do this check first but that would alter the FullTimingExample.
   
#ifndef _DEBUG
    accelerator defaultDevice;
    std::wcout << L"Using device : " << defaultDevice.get_description() << std::endl;
    if (defaultDevice == accelerator(accelerator::direct3d_ref))
        std::wcout << " WARNING!! No C++ AMP hardware accelerator detected, using the REF accelerator." << std::endl << 
            "To see better performance run on C++ AMP\ncapable hardware." << std::endl;
#endif

    FillExample();

    AtomicExample();

    //TdrExample();

#ifdef _DEBUG
    int size = tileSize * 3;
#else
    int size = tileSize * 300;
#endif
    TransposeSimpleExample(size);
    TransposeSimpleExample(size + tileSize);

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
//  Atomic example.
//--------------------------------------------------------------------------------------

void AtomicExample()
{
    std::random_device rd; 
    std::default_random_engine engine(rd()); 
    std::uniform_real_distribution<float> randDist(0.0f, 1.0f);
    std::vector<float> theData(100000);
    std::generate(theData.begin(), theData.end(), [=, &engine, &randDist]() { return randDist(engine); });
    array_view<float, 1> theDataView(int(theData.size()), theData);

    int exceptionalOccurrences = 0;
    array_view<int> count(1, &exceptionalOccurrences);
    parallel_for_each(theDataView.extent, [=] (index<1> idx) restrict(amp)
    {
        if (theDataView[idx] >= 0.9999f)  // Exceptional occurence.
        {
            atomic_fetch_inc(&count(0));
        }
        theDataView[idx] = fast_math::sqrt(theDataView[idx]);
    });
    count.synchronize();
    std::wcout << "Calculating values for " << theData.size() << " elements " << std::endl;
    std::wcout << "A total of " << exceptionalOccurrences << " exceptional occurrences were detected." 
        << std::endl;
}

//--------------------------------------------------------------------------------------
//  TDR example.
//--------------------------------------------------------------------------------------

void Compute(std::vector<float>& inData, std::vector<float>& outData, int start, 
             accelerator& device, queuing_mode mode = queuing_mode::queuing_mode_automatic)
{
    array_view<const float, 1> inDataView(int(inData.size()), inData);
    array_view<float, 1> outDataView(int(outData.size()), outData);

    accelerator_view view = device.create_view(mode);
    parallel_for_each(view, outDataView.extent, [=](index<1> idx) restrict(amp)
    {
        int i = start;
        while (i < 1024)
        {
            outDataView[idx] = inDataView[idx];
            i *= 2;
            i = i % 2048;
        }
    }); 
}

void TdrExample()
{
    std::vector<float> inData(10000);
    std::vector<float> outData(10000, 0.0f);

    try
    {
        Compute(inData, outData, -1, accelerator());
    }
    catch (accelerator_view_removed& ex)
    {
        std::wcout << "TDR exception: " << ex.what(); 
        std::wcout << "  Error code:" << std::hex << ex.get_error_code(); 
        std::wcout << "  Reason:" << std::hex << ex.get_view_removed_reason();

        std::wcout << "Retrying..." << std::endl;
        try
        {
            Compute(inData, outData, -1, accelerator(), queuing_mode::queuing_mode_immediate);
        }
        catch (accelerator_view_removed& ex)
        {
            std::wcout << "TDR exception: " << ex.what(); 
            std::wcout << "  Error code:" << std::hex << ex.get_error_code(); 
            std::wcout << "  Reason:" << std::hex << ex.get_view_removed_reason();
            std::wcout << "FAILED." << std::endl;
        }
    }
}

//--------------------------------------------------------------------------------------
//  Simple matrix transpose example. Included here for camparison timing.
//--------------------------------------------------------------------------------------

void TransposeSimpleExample(int matrixSize)
{ 
    if (matrixSize % tileSize != 0)
        throw std::exception("matrix is not a multiple of tile size.");

    std::vector<unsigned int> inData(matrixSize * matrixSize);
    std::vector<unsigned int> outData(matrixSize * matrixSize, 0u);
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
            outDataView[tidx.global] = inDataView[tidx.global[1]][tidx.global[0]];
        });
    });
    outDataView.synchronize();
    std::wcout << "Transpose simple exact size" << std::endl;
    std::wcout << "  Matrix size " << matrixSize << " x " << matrixSize << std::endl;
    std::wcout << "  Elapsed time " << elapsedTime << " ms" << std::endl;
    CheckMatrix(static_cast<const unsigned int* const>(outData.data()), matrixSize);
    PrintMatrix(static_cast<const unsigned int* const>(outData.data()), matrixSize);
    std::wcout << std::endl;
}

//--------------------------------------------------------------------------------------
//  Tiled matrix transpose example.
//--------------------------------------------------------------------------------------

void TransposeExample(int matrixSize)
{ 
    if (matrixSize % tileSize != 0)
        throw std::exception("matrix is not a multiple of tile size.");

    std::vector<unsigned int> inData(matrixSize * matrixSize);
    std::vector<unsigned int> outData(matrixSize * matrixSize, 0u);
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
    std::wcout << "  Matrix size " << matrixSize << " x " << matrixSize << std::endl;
    std::wcout << "  Elapsed time " << elapsedTime << " ms" << std::endl;
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
    std::vector<unsigned int> outData(matrixSize * matrixSize, 0u);
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
    std::wcout << "  Matrix size " << matrixSize << " x " << matrixSize << ", padded size " 
        << computeDomain[0] << " x " << computeDomain[1] << std::endl;
    std::wcout << "  Elapsed time " << elapsedTime << " ms" << std::endl;
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
    std::vector<unsigned int> outData(matrixSize * matrixSize, 0u);
    unsigned int v = 0;
    std::generate(inData.begin(), inData.end(), [&v]() { return v++; });

    array_view<const unsigned int, 2> inDataView(matrixSize, matrixSize, inData);
    array_view<unsigned int, 2> outDataView(matrixSize, matrixSize, outData);
    outDataView.discard_data();

    tiled_extent<tileSize, tileSize> computeDomain = inDataView.extent.tile<tileSize, tileSize>();
    computeDomain = computeDomain.truncate();

    // TODO_AMP: TDRs with /02 on RC build.
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
                    for (idx0 = computeDomain[0] + 1; idx0 < inDataView.extent[0]; idx0++)
                        for (idx1 = computeDomain[1] + 1; idx1 < inDataView.extent[1]; idx1++) 
                            outDataView(idx1, idx0) = inDataView(idx0, idx1); 
                }
            }
        });
    });

    outDataView.synchronize();
    std::wcout << "Transpose truncated, edge thread handles truncated elements" << std::endl;
    std::wcout << "  Matrix size " << matrixSize << " x " << matrixSize << ", truncated size " 
        << computeDomain[0] << " x " << computeDomain[1] << std::endl;
    std::wcout << "  Elapsed time " << elapsedTime << " ms" << std::endl;
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

    const int rightMargin = inDataView.extent[1] - computeDomain[1];
    const int bottomMargin = inDataView.extent[0] - computeDomain[0];
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
    std::wcout << "Transpose truncated, margin threads handle truncated elements" << std::endl;
    std::wcout << "  Matrix size " << matrixSize << " x " << matrixSize << ", truncated size " 
        << computeDomain[0] << " x " << computeDomain[1] << std::endl;
    std::wcout << "  Elapsed time " << elapsedTime << " ms" << std::endl;
    CheckMatrix(static_cast<const unsigned int* const>(outData.data()), matrixSize);
    PrintMatrix(static_cast<const unsigned int* const>(outData.data()), matrixSize);
    std::wcout << std::endl;
}
