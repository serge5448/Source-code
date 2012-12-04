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
    // Exclusive scan (or prescan), output element at i contains the sum of elements [0]...[i-1].

    template <int TileSize, typename InIt, typename OutIt>
    inline void ExclusiveScanAmpTiled(InIt first, InIt last, OutIt outFirst)
    {
        typedef InIt::value_type T;

        const int size = int(distance(first, last));
        concurrency::array<T, 1> in(size);
        concurrency::array<T, 1> out(size);
        copy(first, last, in);
        
		InclusiveScanAmpTiled<TileSize>(array_view<T, 1>(in), array_view<T, 1>(out));

		// ExclusiveScan is just an offset scan, so shift the results by one.
        copy(out.section(0, size - 1), outFirst + 1);
		*outFirst = T(0);
    }

    // Inclusive scan, output element at i contains the sum of elements [0]...[i].

    template <int TileSize, typename InIt, typename OutIt>
    inline void InclusiveScanAmpTiled(InIt first, InIt last, OutIt outFirst)
    {
        typedef InIt::value_type T;

        const int size = int(distance(first, last));
        concurrency::array<T, 1> in(size);
        concurrency::array<T, 1> out(size);
        copy(first, last, in);
        
        InclusiveScanAmpTiled<TileSize>(array_view<T, 1>(in), array_view<T, 1>(out));
        copy(out, outFirst);
    }

    template <int TileSize, typename T>
    void InclusiveScanAmpTiled(array_view<T, 1> input, array_view<T, 1> output)
    {
        assert(input.extent[0] == output.extent[0]);
        assert(input.extent[0] > 0);

        const int elementCount = input.extent[0];
        const int tileCount = (elementCount + TileSize - 1) / TileSize;

        // Compute tile-wise scans and reductions.
        array<T> tileSums(tileCount);
        details::ComputeTilewiseScan<TileSize>(array_view<const T>(input), array_view<T>(output), array_view<T>(tileSums));

        if (tileCount > 1)
        {
            // Calculate the initial value of each tile based on the tileSums.
            array<T> tmp(tileSums.extent);
            // TODO: This should really be exclusive scan, the copies fix this.
            InclusiveScanAmpTiled<TileSize>(array_view<T>(tileSums), array_view<T>(tmp));
            copy(tmp.section(index<1>(0), concurrency::extent<1>(tmp.extent - 1)), 
                tileSums.section(index<1>(1), concurrency::extent<1>(tileSums.extent - 1)));
            int zero = 0;
            copy(&zero, tileSums.section(0, 1));

            parallel_for_each(extent<1>(elementCount), [=, &tileSums] (concurrency::index<1> idx) restrict (amp) 
            {
                int tileIdx = idx[0] / TileSize;
                output[idx] += tileSums[tileIdx];
            });
        }
    }

    namespace details
    {
        // For each tile calculate the inclusive scan.

        template <int TileSize, typename T>
        void ComputeTilewiseScan(array_view<const T> input, array_view<T> tilewiseOutput, array_view<T> tileSums)
        {
            const int elementCount = input.extent[0];
            const int tileCount = (elementCount + TileSize - 1) / TileSize;
            const int threadCount = tileCount * TileSize;

            // TODO: Change this to use twice the number of threads
            parallel_for_each(extent<1>(threadCount).tile<TileSize>(), [=](tiled_index<TileSize> tidx) restrict(amp) 
            {
                const int tid = tidx.local[0];
                const int gid = tidx.global[0];

                tile_static T tileData[2][TileSize];
                int inIdx = 0;
                int outIdx = 1;
                // Do the first pass (offset = 1) while loading elements into tile_static memory.
                if (gid < elementCount)
                {
                    if (tid >= 1)
                        tileData[outIdx][tid] = input[gid - 1] + input[gid];
                    else 
                        tileData[outIdx][tid] = input[gid];
                }
                tidx.barrier.wait();

                for (int offset = 2; offset < TileSize; offset *= 2)
                {
                    Switch(inIdx, outIdx);

                    if (gid < elementCount) 
                    {
                        if (tid >= offset)
                            tileData[outIdx][tid] = tileData[inIdx][tid - offset] + tileData[inIdx][tid];
                        else 
                            tileData[outIdx][tid] = tileData[inIdx][tid];
                    }
                    tidx.barrier.wait();
                }
                if (gid < elementCount)
                    tilewiseOutput[gid] = tileData[outIdx][tid];
                // Last thread in tileData updates the tileSums.
                if (tid == TileSize - 1)
                    tileSums[tidx.tile[0]] = tileData[outIdx][tid];
            });
        }

        void Switch(int& index1, int& index2) restrict(amp, cpu)
        {
            index1 = 1 - index1;
            index2 = 1 - index2;
        }
    }
}
