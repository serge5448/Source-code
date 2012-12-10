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

#pragma once

#include <amp.h>
#include <assert.h>

#include "Utilities.h"

using namespace concurrency;

namespace Extras
{
    // Exclusive scan, output element at i contains the sum of elements [0]...[i-1].
    static const int kExclusive = 0;
    static const int kInclusive = 1;

    template <int TileSize, typename InIt, typename OutIt>
    inline void ExclusiveScanAmpOptimized(InIt first, InIt last, OutIt outFirst)
    {
        typedef InIt::value_type T;

        const int size = int(distance(first, last));
        concurrency::array<T, 1> in(size);
        concurrency::array<T, 1> out(size);
        copy(first, last, in);   
		details::ScanAmpOptimized<TileSize, kExclusive>(array_view<T, 1>(in), array_view<T, 1>(out));
        copy(out, outFirst);
    }

    template <int TileSize, typename T>  
    inline void ExclusiveScanAmpOptimized(array_view<T, 1> input, array_view<T, 1> output)
    {
        details::ScanAmpOptimized<TileSize, kExclusive, T>(input, output);
    }

    // Inclusive scan, output element at i contains the sum of elements [0]...[i].

    template <int TileSize, typename InIt, typename OutIt>
    inline void InclusiveScanAmpOptimized(InIt first, InIt last, OutIt outFirst)
    {
        typedef InIt::value_type T;

        const int size = int(distance(first, last));
        concurrency::array<T, 1> in(size);
        concurrency::array<T, 1> out(size);
        copy(first, last, in);      
        details::ScanAmpOptimized<TileSize, kInclusive>(array_view<T, 1>(in), array_view<T, 1>(out));
        copy(out, outFirst);
    }

    template <int TileSize, typename T>  
    inline void InclusiveScanAmpOptimized(array_view<T, 1> input, array_view<T, 1> output)
    {
        details::ScanAmpOptimized<TileSize, kInclusive, T>(input, output);
    }

    namespace details
    {
        template <int B, int LogB>
        inline int ConflictFreeOffset(int offset) restrict(amp)
        {
            return n >> B + n >> (2 * LogB);
        }

        // For each tile calculate the exclusive scan.
        //
        // http.developer.nvidia.com/GPUGems3/gpugems3_ch39.html

        template <int TileSize, int Mode, typename T>  
        void ScanAmpOptimized(array_view<T, 1> input, array_view<T, 1> output)
        {
            const int domainSize = TileSize * 2;
            const int elementCount = input.extent[0];
            const int tileCount = (elementCount + domainSize - 1) / domainSize;

            static_assert((Mode == kExclusive || Mode == kInclusive), "Mode must be either inclusive or exclusive.");
            static_assert(IsPowerOfTwoStatic<TileSize>::result, "TileSize must be a power of 2.");
            assert(elementCount > 0);
            assert(elementCount == output.extent[0]);
            assert((elementCount / domainSize) >= 1);

            // Compute scan for each tile and store their total values in tileSums
            array<T> tileSums(tileCount);
            details::ComputeTilewiseExclusiveScanOptimized<TileSize, Mode>(array_view<const T>(input), output, array_view<T>(tileSums));
        
            if (tileCount > 1)
            {
                // Calculate the initial value of each tile based on the tileSums.
                array<T> tileSumScan(tileSums.extent);
                ScanAmpTiled<TileSize, kExclusive>(array_view<T>(tileSums), array_view<T>(tileSumScan));
                std::wcout << tileSumScan << std::endl;
                // Add the tileSums all the elements in each tile except the first tile.
                output.discard_data();
                parallel_for_each(extent<1>(elementCount - domainSize), [=, &tileSumScan] (concurrency::index<1> idx) restrict (amp) 
                {
                    const int tileIdx = (idx[0] + domainSize) / domainSize;
                    output[idx + domainSize] += tileSumScan[tileIdx];
                });
            }
        }

        template <int TileSize, int Mode, typename T>
        void ComputeTilewiseExclusiveScanOptimized(array_view<const T, 1> input, array_view<T> tilewiseOutput, array_view<T, 1> tileSums)
        {
            static const int domainSize = TileSize * 2;
            const int elementCount = input.extent[0];
            const int tileCount = (elementCount + TileSize - 1) / TileSize;
            const int threadCount = tileCount * TileSize;

            tilewiseOutput.discard_data();
            parallel_for_each(extent<1>(threadCount).tile<TileSize>(), [=](tiled_index<TileSize> tidx) restrict(amp) 
            {
                const int tid = tidx.local[0];
                const int tidx2 = tidx.local[0] * 2;
                const int gidx2 = tidx.global[0] * 2;
                tile_static T tileData[domainSize];

                // Load data into tileData, load 2x elements per tile.

                if (gidx2 + 1 < elementCount)
                {
                    tileData[tidx2] = input[gidx2];
                    tileData[tidx2 + 1] = input[gidx2 + 1];
                }
                // Up sweep (reduce) phase.

                int offset = 1;
                for (int stride = TileSize; stride > 0; stride >>= 1)
                {
                    tidx.barrier.wait();
                    if (tid < stride)
                    {
                        const int ai = offset * (tidx2 + 1) - 1; 
                        const int bi = offset * (tidx2 + 2) - 1; 
                        tileData[bi] += tileData[ai];
                    }
                    offset *= 2;
                }
                
                //  Zero highest element in tile
                if (tid == 0) 
                    tileData[domainSize - 1] = 0;
                
                // Down sweep phase.
                // Now: offset = domainSize
                for (int stride = 1; stride <= TileSize; stride *= 2)
                {
                    offset >>= 1;
                    tidx.barrier.wait();
                
                    if (tid < stride)
                    {
                        const int ai = offset * (tidx2 + 1) - 1; 
                        const int bi = offset * (tidx2 + 2) - 1; 
                        T t = tileData[ai]; 
                        tileData[ai] = tileData[bi]; 
                        tileData[bi] += t;
                    }
                }
                tidx.barrier.wait();

                // Copy tile results out. For inclusive scan shift all elements left.

                if (gidx2 + 1 - (2 * Mode) < elementCount)
                {
                    tilewiseOutput[gidx2] = tileData[tidx2 + Mode]; 
                    tilewiseOutput[gidx2 + 1] = tileData[tidx2 + 1 + Mode];
                }

                // Copy tile total out, this is the inclusive total.

                if (tid == (TileSize - 1))
                {
                    // For inclusive scan calculate the last value
                    if (Mode == kInclusive)
                        tilewiseOutput[gidx2 + 1] = tileData[domainSize - 1] + input[gidx2 + 1];
                    
                    tileSums[tidx.tile[0]] = tileData[domainSize - 1] + input[gidx2 + 1];
                }
            });
        }
    }
}
