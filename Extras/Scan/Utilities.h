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

#include <iostream>
#include <xutility>

//===============================================================================
//  Stream output overloads
//===============================================================================

class ContainerWidth 
{
public:
    explicit ContainerWidth(size_t width) : m_width(width) { }

private:
    size_t m_width;

    template <class T, class Traits>
    inline friend std::basic_ostream<T, Traits>& operator << 
        (std::basic_ostream<T, Traits>& os, const ContainerWidth& container)
    { 
        os.iword(::details::geti()) = container.m_width; 
        return os;
    }
};

template<typename StrmType, typename Traits, typename VecT>
std::basic_ostream<StrmType, Traits>& operator<< (std::basic_ostream<StrmType, Traits>& os, const std::vector<VecT>& vec)
{
    size_t i = std::min<size_t>(::details::GetWidth(os), vec.size());
    std::copy(std::begin(vec), std::begin(vec) + i, std::ostream_iterator<VecT, Traits::char_type>(os, ::details::GetDelimiter<Traits::char_type>()));
    return os;
}

template<typename StrmType, typename Traits, typename VecT>
std::basic_ostream<StrmType, Traits>& operator<< (std::basic_ostream<StrmType, Traits>& os, concurrency::array<VecT, 1>& vec)
{
    size_t i = std::min<size_t>(::details::GetWidth(os), vec.extent[0]);
    std::vector<const VecT> buffer(i);
    copy(vec.section(0, i), std::begin(buffer));
    std::copy(std::begin(buffer), std::begin(buffer) + i, std::ostream_iterator<VecT, Traits::char_type>(os, ::details::GetDelimiter<Traits::char_type>()));
    return os;
}

template<typename StrmType, typename Traits, typename VecT>
std::basic_ostream<StrmType, Traits>& operator<< (std::basic_ostream<StrmType, Traits>& os, const concurrency::array_view<VecT, 1>& vec)
{
    size_t i = std::min<size_t>(::details::GetWidth(os), vec.extent[0]);
    std::vector<VecT> buffer(i);
    copy(vec.section(0, i), std::begin(buffer));  
    std::copy(std::begin(buffer), std::begin(buffer) + i, std::ostream_iterator<VecT, Traits::char_type>(os, ::details::GetDelimiter<Traits::char_type>()));
    return os;
}

namespace details
{
    inline int geti() 
    { 
        static int i = std::ios_base::xalloc();
        return i; 
    }

    template <typename STREAM>
    size_t GetWidth(STREAM& os)
    {
        const size_t kDefaultWidth = 10;
        size_t width = os.iword(geti());
        if (width == 0)
            width = kDefaultWidth;
        return width;
    }

    template <typename T>
    T* GetDelimiter()
    {
        assert(false);
        return nullptr;
    }

    template <>
    char* GetDelimiter()
    {
        static char delim(',');
        return &delim;
    }

    template <>
    wchar_t* GetDelimiter()
    {
        static wchar_t delim(L',');
        return &delim;
    }
}
