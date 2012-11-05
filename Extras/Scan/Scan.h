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

    template <typename InIt, typename OutIt>
    void Prescan(InIt first, InIt last, OutIt outFirst)
    {
        typedef OutIt::value_type T;

		*outFirst = T(0);
		for (int i = 1; i < std::distance(first, last); ++i)
			outFirst[i] = first[i - 1] + outFirst[i - 1];
    }

	// Inclusive scan, output element at i contains the sum of elements [0]...[i].

	template <typename InIt, typename OutIt>
    void Scan(InIt first, InIt last, OutIt outFirst)
    {
        typedef OutIt::value_type T;

		*outFirst = T(*first);
		for (int i = 1; i < std::distance(first, last); ++i)
			outFirst[i] = first[i] + outFirst[i - 1];
    }

    // http://www.csce.uark.edu/~mqhuang/courses/5013/f2011/lab/Lab-5-scan.pdf 

	// Exclusive scan, output element at i contains the sum of elements [0]...[i-1].

    template <typename InIt, typename OutIt>
    inline void PrescanAmpSimple(InIt first, InIt last, OutIt outFirst)
    {
        PrescanAmp(first, last, outFirst, ComputeScanAmpSimple<InIt::value_type>);
    }

    template <typename InIt, typename OutIt>
    inline void PrescanAmpTiled(InIt first, InIt last, OutIt outFirst)
    {
        PrescanAmp(first, last, outFirst, ComputeScanAmpTiled<16, InIt::value_type>);
    }

    template <typename InIt, typename OutIt, typename Func>
    void PrescanAmp(InIt first, InIt last, OutIt outFirst, Func f)
    {
        typedef InIt::value_type T;

        int size = int(distance(first, last));
        concurrency::array<T, 1> in(size);
        concurrency::array<T, 1> out(size);
        copy(first, last, in);
        
		f(array_view<T, 1>(in), array_view<T, 1>(out));

		// Prescan is just an offset scan, so shift the results by one.
        copy(out.section(0, size - 1), outFirst + 1);
		*outFirst = T(0);
    }

	// Inclusive scan, output element at i contains the sum of elements [0]...[i].

    template <typename InIt, typename OutIt>
    inline void ScanAmpSimple(InIt first, InIt last, OutIt outFirst)
    {
        ScanAmp(first, last, outFirst, ComputeScanAmpSimple<InIt::value_type>);
    }

    template <typename InIt, typename OutIt>
    inline void ScanAmpTiled(InIt first, InIt last, OutIt outFirst)
    {
        ScanAmp(first, last, outFirst, ComputeScanAmpTiled<16, InIt::value_type>);
    }

	template <typename InIt, typename OutIt, typename Func>
    void ScanAmp(InIt first, InIt last, OutIt outFirst, Func f)
    {
        typedef InIt::value_type T;

        int size = int(distance(first, last));
        concurrency::array<T, 1> in(size);
        concurrency::array<T, 1> out(size);
        copy(first, last, in);
        
		f(array_view<T, 1>(in), array_view<T, 1>(out));
        copy(out, outFirst);
    }

	template <typename T>
	void ComputeScanAmpSimple(array_view<T, 1> input, array_view<T, 1> output)
	{
		assert(input.extent[0] == output.extent[0]);

        for (int offset = 1; offset < input.extent[0]; offset *= 2)
        {
			assert(input.extent[0] == output.extent[0]);
			assert(input.extent[0] % 2 == 0);
			output.discard_data();
			parallel_for_each(input.extent, [=](index<1> idx) restrict (amp)
			{
				if (idx[0] >= offset)
					output[idx] = input[idx] + input[idx - offset];
				else
					output[idx] = input[idx];
			});
			std::swap(input, output);
        }
	}

    template <int TileSize, typename T>
    void ComputeScanAmpTiled(array_view<const T, 1> input, array_view<T, 1> output)
    {
        assert(input.extent[0] == output.extent[0]);

        const int elementCount = input.extent[0];
        const int tileCount = (elementCount + TileSize - 1) / TileSize;

        // Compute tile-wise scans and reductions
        array<T> tileSums(tileCount);
        ComputeTilewiseScan<TileSize>(array_view<const T>(input), array_view<T>(output), 
            array_view<T>(tileSums));

        // recurse if necessary
        if (tileCount >  1)
        {
            array<T> tmp(tileSums.extent);
            ComputeScanAmpTiled<TileSize>(array_view<const T>(tileSums), array_view<T>(tmp));
            copy(tmp, tileSums);

            if (elementCount > 0) 
            {
                parallel_for_each(extent<1>(elementCount), [=, &tileSums] (concurrency::index<1> idx) restrict (amp) 
                {
                    int tileIdx = idx[0] / TileSize;
                    if (tileIdx == 0)
                        output[idx] = output[idx];
                    else
                        output[idx] = tileSums[tileIdx - 1] + output[idx];
                });
            }
        }
    }

    // Calculate prefix sum for a tile

    template <int TileSize, typename T>
    void ComputeTilewiseScan(array_view<const T> input, array_view<T> tilewiseOutput, array_view<T> tileSums)
    {
        int elementCount = input.extent[0];
        int tileCount = (elementCount + TileSize - 1) / TileSize;
        int threadCount = tileCount * TileSize;
    
        parallel_for_each(extent<1>(threadCount).tile<TileSize>(), [=](tiled_index<TileSize> tidx) restrict(amp) 
        {
            const int tid = tidx.local[0];
            const int globid = tidx.global[0];

            tile_static T tile[2][TileSize];
            int inIdx = 0;
            int outIdx = 1;
            if (globid < elementCount)
                tile[outIdx][tid] = input[globid];
            tidx.barrier.wait();

            for (int offset = 1; offset < TileSize; offset *= 2)
            {
                // For each iteration, the Y dimension index
                // specifies which index acts as input or output.
                // For each iteration these elements toggle
                inIdx = 1 - inIdx;
                outIdx = 1 - outIdx;

                if (globid < elementCount) 
                {
                    if (tid >= offset)
                        tile[outIdx][tid]  = tile[inIdx][tid] + tile[inIdx][tid - offset];
                    else 
                        tile[outIdx][tid]  = tile[inIdx][tid];
                }
                tidx.barrier.wait();
            }
            if (globid < elementCount)
                tilewiseOutput[globid] = tile[outIdx][tid];
            // Last thread in tile updates the tileSums.
            if (tid == TileSize - 1)
                tileSums[tidx.tile[0]] = tile[outIdx][tid];
        });
    }
}
