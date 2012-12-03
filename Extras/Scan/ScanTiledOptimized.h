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
    inline void ExclusiveScanAmpTiledOptimized(InIt first, InIt last, OutIt outFirst)
    {
        typedef InIt::value_type T;

        const int size = int(distance(first, last));
        concurrency::array<T, 1> in(size);
        concurrency::array<T, 1> out(size);
        copy(first, last, in);
        
		ExclusiveScanAmpTiledOptimized<TileSize>(array_view<T, 1>(in), array_view<T, 1>(out));

		// ExclusiveScan is just an offset scan, so shift the results by one.
        copy(out.section(0, size - 1), outFirst + 1);
		*outFirst = T(0);
    }

    // Inclusive scan, output element at i contains the sum of elements [0]...[i].

    template <int TileSize, typename InIt, typename OutIt>
    inline void InclusiveScanAmpTiledOptimized(InIt first, InIt last, OutIt outFirst)
    {
        typedef InIt::value_type T;

        const int size = int(distance(first, last));
        concurrency::array<T, 1> in(size);
        concurrency::array<T, 1> out(size);
        copy(first, last, in);
        
        ExclusiveScanAmpTiledOptimized<TileSize>(array_view<T, 1>(in), array_view<T, 1>(out));
        copy(out, outFirst);
    }

    template <int TileSize, typename T>
    void ExclusiveScanAmpTiledOptimized(array_view<T, 1> input, array_view<T, 1> output)
    {
        assert(input.extent[0] == output.extent[0]);

        const int elementCount = input.extent[0];
        const int tileCount = (elementCount + (TileSize * 2) - 1) / (TileSize * 2);

        // Compute tile-wise scans and reductions
        array<T> tileSums(tileCount);
        details::ComputeExclusiveTiledScanOptimized<TileSize>(array_view<const T>(input), array_view<T>(output), array_view<T>(tileSums));

        // recurse if necessary
        if (tileCount >  1)
        {
            array<T> tmp(tileSums.extent);
            InclusiveScanAmpTiled<TileSize>(array_view<T>(tileSums), array_view<T>(tmp));
            array_view<T, 1> dest = tileSums.section(index<1>(1), concurrency::extent<1>(tileSums.extent - 1));
            copy(tmp.section(index<1>(0), concurrency::extent<1>(tmp.extent - 1)), dest);

            if (elementCount > 0) 
            {
                parallel_for_each(extent<1>(elementCount), [=, &tileSums] (concurrency::index<1> idx) restrict (amp) 
                {
                    const int tileIdx = idx[0] / (TileSize);
                    if (tileIdx == 0)
                        output[idx] = output[idx];
                    else
                        output[idx] = tileSums[tileIdx - 1] + output[idx];
                });
            }
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
        void ComputeExclusiveTiledScanOptimized(array_view<const T, 1> input, array_view<T> tilewiseOutput, array_view<T, 1> tileSums)
        {
            assert(input.extent[0] == (TileSize * 2));
            assert(tilewiseOutput.extent[0] == TileSize);

            const int elementCount = input.extent[0];
            const int tileCount = (elementCount + (TileSize * 2) - 1) / (TileSize * 2);
            const int threadCount = tileCount * TileSize;

            parallel_for_each(extent<1>(threadCount).tile<TileSize>(), [=](tiled_index<TileSize> tidx) restrict(amp) 
            {
                const int tid = tidx.local[0];
                const int globid = tidx.global[0];
                int offset = 1;
                tile_static T tile[TileSize * 2];

                tile[2 * tid] = input[2 * globid];
                tile[2 * tid + 1] = input[2 * globid + 1];

                const int n = TileSize * 2;
                for (int d = n >> 1; d > 0; d >>= 1)
                {
                    tidx.barrier.wait();
                    if (tid < d)
                    {
                        const int ai = offset * (2 * tid + 1) - 1; 
                        const int bi = offset * (2 * tid + 2) - 1; 
                        tile[bi] += tile[ai];
                    }
                    offset *= 2;
                }

                if (tid == 0) 
                    tile[n - 1] = 0;

                for (int d = 1; d < n; d*= 2)
                {
                    offset >>= 1;

                    tidx.barrier.wait();
                    if (tid < d)
                    {
                        const int ai = offset * (2 * tid + 1) - 1; 
                        const int bi = offset * (2 * tid + 2) - 1; 
                        T t = tile[ai]; 
                        tile[ai] = tile[bi]; 
                        tile[bi] += t;
                    }
                }
                tidx.barrier.wait();

                tilewiseOutput[2 * tid] = tile[2 * tid]; 
                tilewiseOutput[2 * tid + 1] = tile[2 * tid + 1]; 
            });
        }
    }
}
