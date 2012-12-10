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
		details::ScanAmpTiled<TileSize, kExclusive>(array_view<T, 1>(in), array_view<T, 1>(out));
        copy(out, outFirst);
    }

    template <int TileSize, typename T>
    void ExclusiveScanAmpTiled(array_view<T, 1> input, array_view<T, 1> output)
    {
		details::ScanAmpTiled<TileSize, kExclusive>(input, output);
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
        details::ScanAmpTiled<TileSize, kInclusive>(array_view<T, 1>(in), array_view<T, 1>(out));
        copy(out, outFirst);
    }

    template <int TileSize, typename T>
    inline void InclusiveScanAmpTiled(array_view<T, 1> input, array_view<T, 1> output)
    {
		details::ScanAmpTiled<TileSize, kInclusive>(input, output);
    }

    namespace details
    {
        template <int TileSize, int Mode, typename T>
        void ScanAmpTiled(array_view<T, 1> input, array_view<T, 1> output)
        {
            static_assert((Mode == kExclusive || Mode == kInclusive), "Mode must be either inclusive or exclusive.");
            static_assert(IsPowerOfTwoStatic<TileSize>::result, "TileSize must be a power of 2.");
            assert(input.extent[0] == output.extent[0]);
            assert(input.extent[0] > 0);

            const int elementCount = input.extent[0];
            const int tileCount = (elementCount + TileSize - 1) / TileSize;

            // Compute tile-wise scans and reductions.
            array<T> tileSums(tileCount);
            details::ComputeTilewiseExclusiveScanTiled<TileSize, Mode>(array_view<const T>(input), array_view<T>(output), array_view<T>(tileSums));

            if (tileCount > 1)
            {
                // Calculate the initial value of each tile based on the tileSums.
                array<T> tmp(tileSums.extent);
                ScanAmpTiled<TileSize, kExclusive>(array_view<T>(tileSums), array_view<T>(tmp));
                output.discard_data();
                parallel_for_each(extent<1>(elementCount), [=, &tileSums, &tmp] (concurrency::index<1> idx) restrict (amp) 
                {
                    int tileIdx = idx[0] / TileSize;
                    output[idx] += tmp[tileIdx];
                });
            }
        }

        // For each tile calculate the inclusive scan.

        template <int TileSize, int Mode, typename T>
        void ComputeTilewiseExclusiveScanTiled(array_view<const T> input, array_view<T> tilewiseOutput, array_view<T> tileSums)
        {
            const int elementCount = input.extent[0];
            const int tileCount = (elementCount + TileSize - 1) / TileSize;
            const int threadCount = tileCount * TileSize;

            tilewiseOutput.discard_data();
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

                // Copy tile results out. For exclusive scan shift all elements right.

                if (gid < elementCount)
                {
                    // For exclusive scan calculate the last value
                    if (Mode == kInclusive)
                        tilewiseOutput[gid] = tileData[outIdx][tid] ;
                    else
                        if (tid == 0)
                            tilewiseOutput[gid] = T(0);
                        else
                            tilewiseOutput[gid] = tileData[outIdx][tid - 1] ;
                }
                // Last thread in tile updates the tileSums.
                if (tid == TileSize - 1)
                    tileSums[tidx.tile[0]] = tileData[outIdx][tid - 1] + + input[gid];
            });
        }

        void Switch(int& index1, int& index2) restrict(amp, cpu)
        {
            index1 = 1 - index1;
            index2 = 1 - index2;
        }
    }
}
