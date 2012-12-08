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

    template <int TileSize, typename InIt, typename OutIt>
    inline void ExclusiveScanAmpOptimized(InIt first, InIt last, OutIt outFirst)
    {
        typedef InIt::value_type T;

        const int size = int(distance(first, last));
        concurrency::array<T, 1> in(size);
        concurrency::array<T, 1> out(size);
        copy(first, last, in);   
		ExclusiveScanAmpOptimized<TileSize>(array_view<T, 1>(in), array_view<T, 1>(out));
        copy(out, outFirst);
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
        // ExclusiveScan is just an offset scan, so shift the results by one.
        ExclusiveScanAmpOptimized<TileSize>(array_view<T, 1>(in), array_view<T, 1>(out));
        copy(out.section(1, size - 1), outFirst);
		*(outFirst + size - 1) = *(last - 1) + *(outFirst + size - 2);
    }

    // TODO: Should be inclusive.
    template <int TileSize, typename T>  
    void ExclusiveScanAmpOptimized(array_view<T, 1> input, array_view<T, 1> output)
    {
        const int domainSize = TileSize * 2;
        const int elementCount = input.extent[0];
        const int tileCount = (elementCount + domainSize - 1) / domainSize;

        assert(elementCount > 0);
        assert(elementCount == output.extent[0]);
        assert(tileSums.extent[0] == elementCount / domainSize);
        assert((elementCount / domainSize) >= 1);
        assert((elementCount % domainSize) == 0);
        static_assert(IsPowerOfTwoStatic<TileSize>::result, "TileSize must be a power of 2.");
        // TODO: Need to fix scan to support arrays that are not a whole number of tiles.
        //assert(elementCount % TileSize == 0);


        // Compute scan for each tile and store their total values in tileSums
        array<T> tileSums(tileCount);
        details::ComputeTilewiseExclusiveScanOptimized<TileSize>(array_view<const T>(input), output, array_view<T>(tileSums));
        
#ifdef _DEBUG
        std::wcout << ContainerWidth(10) << "tileSums[" << tileSums.extent[0] << "] = " << tileSums << std::endl;
        std::wcout << ContainerWidth(10) << "output = [" << output.extent[0] << "] = " << output << std::endl;
#endif
        if (tileCount > 1)
        {
            // Calculate the initial value of each tile based on the tileSums.
            array<T> tmp(tileSums.extent);
            InclusiveScanAmpTiled<TileSize>(array_view<T>(tileSums), array_view<T>(tmp));
            std::swap(tmp, tileSums);
#ifdef _DEBUG
            std::cout << ContainerWidth(4) << "tileSums[" << tileSums.extent[0] << "] = " << tileSums << std::endl;
            std::wcout << "output = [" << output.extent[0] << "] = " << output << std::endl;
#endif
            // Add the tileSums all the elements in each tile except the first tile.
            parallel_for_each(extent<1>(elementCount - TileSize), [=, &tileSums] (concurrency::index<1> idx) restrict (amp) 
            {
                const int tileIdx = (idx[0] + TileSize) / domainSize;
                output[idx + TileSize] += tileSums[tileIdx - 1];
            });
        }
    }

    namespace details
    {
        template <int B, int LogB>
        inline int ConflictFreeOffset(int offset) restrict(amp)
        {
            return n >> B + n >> (2 * LogB);
        }
        
        template <typename T>
        void InclusiveToExclusive(array_view<T, 1> inclusiveScan, array_view<T, 1> exclusiveScan)
        {
            parallel_for_each(inclusiveScan.extent, [=] (concurrency::index<1> idx) restrict (amp) 
            {
                if (idx[0] == 0)
                    exclusiveScan[idx] = T(0);
                else
                    exclusiveScan[idx] = inclusiveScan[idx - 1];
            });
        }

        // For each tile calculate the exclusive scan.
        //
        // http.developer.nvidia.com/GPUGems3/gpugems3_ch39.html

        template <int TileSize, typename T>
        void ComputeTilewiseExclusiveScanOptimized(array_view<const T, 1> input, array_view<T> tilewiseOutput, array_view<T, 1> tileSums)
        {
            static const int domainSize = TileSize * 2;
            const int elementCount = input.extent[0];

            parallel_for_each(extent<1>(elementCount / 2).tile<TileSize>(), [=](tiled_index<TileSize> tidx) restrict(amp) 
            {
                const int tid = tidx.local[0];
                const int gid = tidx.global[0];
                tile_static T tileData[domainSize];

                // Load data into tileData, load 2x elements per tile.

                tileData[2 * tid] = input[2 * gid];
                tileData[2 * tid + 1] = input[2 * gid + 1];

                // Up sweep (reduce) phase.

                int offset = 1;
                for (int stride = TileSize; stride > 0; stride >>= 1)
                {
                    tidx.barrier.wait();
                    if (tid < stride)
                    {
                        const int ai = offset * (2 * tid + 1) - 1; 
                        const int bi = offset * (2 * tid + 2) - 1; 
                        tileData[bi] += tileData[ai];
                    }
                    offset *= 2;
                }

                // TODO: Is another barrier required here?
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
                        const int ai = offset * (2 * tid + 1) - 1; 
                        const int bi = offset * (2 * tid + 2) - 1; 
                        T t = tileData[ai]; 
                        tileData[ai] = tileData[bi]; 
                        tileData[bi] += t;
                    }
                }
                tidx.barrier.wait();

                // Copy tile results out.

                tilewiseOutput[2 * gid] = tileData[2 * tid]; 
                tilewiseOutput[2 * gid + 1] = tileData[2 * tid + 1];

                // Copy tile total out, this is the inclusive total.

                if (tid == (TileSize - 1))
                    tileSums[tidx.tile[0]] = tileData[2 * TileSize - 1] + input[2 * gid + 1];
            });
        }
    }
}
