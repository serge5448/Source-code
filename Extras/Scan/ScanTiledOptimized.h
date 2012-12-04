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

		// ExclusiveScan is just an offset scan, so shift the results by one.
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
        
        ExclusiveScanAmpOptimized<TileSize>(array_view<T, 1>(in), array_view<T, 1>(out));
        copy(out.section(1, size - 1), outFirst);
		*(outFirst + size - 1) = *(last - 1) + *(outFirst + size - 2);
    }

    template <int TileSize, typename T>  
    void ExclusiveScanAmpOptimized(array_view<T, 1> input, array_view<T, 1> output)
    {
        assert(input.extent[0] > 0);
        assert(input.extent[0] == output.extent[0]);

        const int elementCount = input.extent[0];
        const int tileCount = (elementCount + (TileSize * 2) - 1) / (TileSize * 2);

        // Compute scan for each tile and store their total values in tileSums
        array<T> tileSums(tileCount);
        details::ComputeTilewiseScanOptimized<TileSize>(array_view<const T>(input), array_view<T>(output), array_view<T>(tileSums));
        if (tileCount > 1)
        {
            array<T> tmp(tileSums.extent);
            // TODO: This should really be exclusive scan, the copies fix this.
            InclusiveScanAmpTiled<TileSize>(array_view<T>(tileSums), array_view<T>(tmp));
            copy(tmp.section(index<1>(0), concurrency::extent<1>(tmp.extent - 1)), 
                tileSums.section(index<1>(1), concurrency::extent<1>(tileSums.extent - 1)));
            int zero = 0;
            copy(&zero, tileSums.section(0, 1));
            {
                std::vector<int> d(tileSums.extent[0]);
                copy(tileSums, d.begin());
                std::wcout << d << std::endl;
            }
            {
                std::vector<int> d(output.extent[0]);
                copy(output, d.begin());
                std::wcout << d << std::endl;
            }
            parallel_for_each(extent<1>(elementCount), [=, &tileSums] (concurrency::index<1> idx) restrict (amp) 
            {
                const int tileIdx = idx[0] / (TileSize * 2);
                output[idx] += tileSums[tileIdx];
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

        // For each tile calculate the exclusive scan.
        //
        // http://www.csce.uark.edu/~mqhuang/courses/5013/f2011/lab/Lab-5-scan.pdf 

        template <int TileSize, typename T>
        void ComputeTilewiseScanOptimized(array_view<const T, 1> input, array_view<T> tilewiseOutput, array_view<T, 1> tileSums)
        {
            assert(input.extent[0] == tilewiseOutput.extent[0]);
            assert(tileSums.extent[0] == input.extent[0] / (TileSize * 2));
            assert((input.extent[0] / (TileSize * 2)) >= 1);
            assert((input.extent[0] % (TileSize * 2)) == 0);

            const int elementCount = input.extent[0];
            const int tileCount = (elementCount + (TileSize * 2) - 1) / (TileSize * 2);
            const int threadCount = tileCount * TileSize;

            parallel_for_each(extent<1>(threadCount).tile<TileSize>(), [=](tiled_index<TileSize> tidx) restrict(amp) 
            {
                const int tid = tidx.local[0];
                const int gid = tidx.global[0];
                tile_static T tileData[TileSize * 2];

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

                //  Zero highest element in tile
                if (tid == (TileSize - 1)) 
                    tileData[TileSize * 2 - 1] = 0;

                // Down sweep phase.
                // Now: offset = TileSize * 2;
                for (int stride = 1; stride <= TileSize; stride *= 2)
                {
                    tidx.barrier.wait();

                    offset >>= 1;
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

                // Copy tile total out.
                if (tid == (TileSize - 1))
                    tileSums[tidx.tile[0]] = tileData[2 * tid + 1] + input[2 * gid + 1];
            });
        }
    }
}
