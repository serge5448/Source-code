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
    void PrescanAmp(InIt first, InIt last, OutIt outFirst)
    {
        typedef InIt::value_type T;

        int size = int(distance(first, last));
        concurrency::array<T, 1> in(size);
        concurrency::array<T, 1> out(size);
        copy(first, last, in);
        
		ComputeScanAmp(array_view<T, 1>(in), array_view<T, 1>(out));

		// Prescan is just an offset scan, so shift the results by one.
        copy(out.section(0, size - 1), outFirst + 1);
		*outFirst = T(0);
    }

	// Inclusive scan, output element at i contains the sum of elements [0]...[i].

	template <typename InIt, typename OutIt>
    void ScanAmp(InIt first, InIt last, OutIt outFirst)
    {
        typedef InIt::value_type T;

        int size = int(distance(first, last));
        concurrency::array<T, 1> in(size);
        concurrency::array<T, 1> out(size);
        copy(first, last, in);
        
		ComputeScanAmp(array_view<T, 1>(in), array_view<T, 1>(out));
        copy(out, outFirst);
    }

	template <typename T>
	void ComputeScanAmp(array_view<T, 1> input, array_view<T, 1> output)
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
}
