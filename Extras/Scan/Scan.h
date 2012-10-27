namespace Examples
{
    // CPU based implementation of scan

    template <typename InIt, typename OutIt>
    void Scan(InIt first, InIt last, OutIt outFirst)
    {
        InIt::value_type sum = InIt::value_type(0);
        transform(first, last, outFirst, [&sum](InIt::value_type i)->InIt::value_type { sum += i; return sum; });
    }

    // http://www.csce.uark.edu/~mqhuang/courses/5013/f2011/lab/Lab-5-scan.pdf 
    //template <int TileSize, typename T>
    //void ScanAmpTiled()
    //{
    //    int tileCount = 0;

    //}
}
