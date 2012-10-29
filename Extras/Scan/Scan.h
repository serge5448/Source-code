using namespace concurrency;

namespace Examples
{
    // CPU based implementation of scan

    template <typename InIt, typename OutIt>
    void Scan(InIt first, InIt last, OutIt outFirst)
    {
        typedef InIt::value_type T;
        T sum = InIt::value_type(0);
        transform(first, last, outFirst, [&sum](T i)->T { sum += i; return sum; });
    }

    // http://www.csce.uark.edu/~mqhuang/courses/5013/f2011/lab/Lab-5-scan.pdf 

    template <typename InIt, typename OutIt>
    void ScanAmp(InIt first, InIt last, OutIt outFirst)
    {
        typedef InIt::value_type T;

        int size = int(distance(first, last));
        concurrency::array<T, 1> in(size);
        concurrency::array<T, 1> out(size);
        copy(first, last, in);
        
        for (int offset = 1; offset < size; offset *= 2)
        {
            ComputeScan(array_view<const T, 1>(in), array_view<T, 1>(out), offset);
            std::swap(in, out);
        }
        copy(in, outFirst);
    }

    template <typename T>
    void ComputeScan(array_view<const T> input, array_view<T> output, int offset)
    {
        assert(input.extent[0] == output.extent[0]);
        assert(input.extent[0] % 2 == 0);

        parallel_for_each(input.extent, [=](index<1> idx) restrict (amp)
        {
			output[idx] = input[idx];
			if (idx[0] >= offset)
				output[idx] += input[idx - offset];
        });
    }
}
