#include "stdafx.h"

class ContainerWidth 
{
public:
	explicit ContainerWidth(size_t width) : m_width(width) { }

private:
	size_t m_width;

	template <class charT, class Traits>
	friend std::basic_ostream<charT, Traits>& operator << 
		(std::basic_ostream<charT, Traits>& os, const ContainerWidth& w)
	{ 
		os.iword(geti()) = w.m_width; 
		return os;
	}
};

inline int geti() 
{ 
	static int i = std::ios_base::xalloc();
	return i;
}

template <typename T>
std::wostream& operator << (std::wostream& os, const std::vector<T>& vec)
{
	size_t i = std::min<size_t>(os.iword(geti()), vec.size());
    std::copy(begin(vec), begin(vec) + i, std::ostream_iterator<int, wchar_t>(os, L"," ));
    return os;
}
