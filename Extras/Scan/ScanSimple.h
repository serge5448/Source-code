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

namespace Extras
{
    //===============================================================================
    // Exclusive scan, output element at i contains the sum of elements [0]...[i-1].
    //===============================================================================

    template <typename InIt, typename OutIt>
    inline void ExclusiveScanSimple(InIt first, InIt last, OutIt outFirst)
    {
        typedef InIt::value_type T;

        const int size = int(distance(first, last));
        concurrency::array<T, 1> in(size);
        concurrency::array<T, 1> out(size);
        copy(first, last, in);
        details::ScanSimple<details::kExclusive>(concurrency::array_view<T, 1>(in), concurrency::array_view<T, 1>(out));
        copy(in, outFirst);
    }

    template <typename T>
    void ExclusiveScanSimple(concurrency::array_view<T, 1> input, concurrency::array_view<T, 1> output)
    {
		details::ScanSimple<details::kExclusive>(input, output);
    }

    //===============================================================================
    // Inclusive scan, output element at i contains the sum of elements [0]...[i].
    //===============================================================================

    template <typename InIt, typename OutIt>
    inline void InclusiveScanSimple(InIt first, InIt last, OutIt outFirst)
    {
        typedef InIt::value_type T;

        const int size = int(distance(first, last));
        concurrency::array<T, 1> in(size);
        concurrency::array<T, 1> out(size);
        copy(first, last, in);
        details::ScanSimple<details::kInclusive>(concurrency::array_view<T, 1>(in), concurrency::array_view<T, 1>(out));
        copy(out, outFirst);
    }

    template <typename T>
    void InclusiveScanSimple(concurrency::array_view<T, 1> input, concurrency::array_view<T, 1> output)
    {
		details::ScanSimple<details::kInclusive>(input, output);
    }

    //===============================================================================
    //  Implementation. Not supposed to be called directly.
    //===============================================================================

    namespace details
    {
        // Inclusive
        template <int Mode, typename T>
        void ScanSimple(concurrency::array_view<T, 1> input, concurrency::array_view<T, 1> output)
        {
            assert(input.extent[0] == output.extent[0]);
            for (int offset = 1; offset < input.extent[0]; offset *= 2)
            {
                assert(input.extent[0] == output.extent[0]);
                assert(input.extent[0] % 2 == 0);
                output.discard_data();
                parallel_for_each(input.extent, [=](concurrency::index<1> idx) restrict (amp)
                {
                    if (idx[0] >= offset)
                        output[idx] = input[idx] + input[idx - offset];
                    else
                        output[idx] = input[idx];
                });
                std::swap(input, output);
            }
            // TODO: Is this a better way to do this.
            if (Mode == details::kInclusive)
                return;

            parallel_for_each(output.extent, [=] (concurrency::index<1> idx) restrict (amp) 
            {
                if (idx[0] == 0)
                    output[idx] = T(0);
                else
                    output[idx] = input[idx - 1];
            });
            std::swap(input, output);
        }
    }
}
