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
#include <array>

#include "FrameProcessorAmp.h"
#include "IFrameProcessor.h"

//--------------------------------------------------------------------------------------
//  Frame processor for a single C++ AMP accelerator.
//--------------------------------------------------------------------------------------
//
//  This runs on a single accelerator. If no hardware accelerator is available it will use the 
//  reference accelerator, this may be very slow.

class FrameProcessorAmpBase : public IFrameProcessor
{
private:
    accelerator m_accelerator;
    std::array<std::shared_ptr<array<ArgbPackedPixel, 2>>, 3> m_frames;
    UINT m_height;
    UINT m_width;

public:
    FrameProcessorAmpBase(const accelerator& accel) :
        m_accelerator(accel),
        m_height(0),
        m_width(0)
    {
    }

    virtual inline void ApplyEdgeDetection(accelerator& acc, const array<ArgbPackedPixel, 2>& srcFrame, array<ArgbPackedPixel, 2>& destFrame, 
                        const array<ArgbPackedPixel, 2>& orgFrame, UINT simplifierNeighborWindow) = 0;
    virtual inline void ApplyColorSimplifier(accelerator& acc, const array<ArgbPackedPixel, 2>& srcFrame, array<ArgbPackedPixel, 2>& destFrame, UINT neighborWindow) = 0;

    void ProcessImage(const Gdiplus::BitmapData& srcFrame, 
        Gdiplus::BitmapData& destFrame, 
        UINT phases, UINT simplifierNeighborWindow)
    {
        assert(simplifierNeighborWindow % 2 == 0);
        assert(phases > 0);

        ConfigureFrameBuffers(srcFrame);

        int current = kCurrent;
        int next = kNext;
        CopyIn(srcFrame, *m_frames[current].get());
        m_frames[current]->copy_to(*m_frames[kOriginal].get());
        for (UINT i = 0; i < phases; ++i)
        {
            ApplyColorSimplifier(m_accelerator, 
                *m_frames[current].get(), *m_frames[next].get(), simplifierNeighborWindow);
            std::swap(current, next);
        }

        ApplyEdgeDetection(m_accelerator, 
            *m_frames[current].get(), *m_frames[next].get(), *m_frames[kOriginal].get(), 
            simplifierNeighborWindow);
        std::swap(current, next);
        CopyOut(*m_frames[current].get(), destFrame);
    }

private:
    void ConfigureFrameBuffers(const Gdiplus::BitmapData& srcFrame)
    {
        // Only recalculate frames if the heights have changed as this may be expensive.
        if ((m_height == srcFrame.Height) && (m_width == srcFrame.Width))
            return;
        m_height = srcFrame.Height;
        m_width = srcFrame.Width;
        ATLTRACE("Configure frame buffers: New image size %d x %d\n", m_height, m_width);

        std::generate(m_frames.begin(), m_frames.end(), [=]()->std::shared_ptr<array<ArgbPackedPixel, 2>> 
        { 
            return std::make_shared<array<ArgbPackedPixel, 2>>(int(m_height), int(m_width), m_accelerator.default_view); 
        });
    }
};

class FrameProcessorAmpSingle : public FrameProcessorAmpBase
{
public:
    FrameProcessorAmpSingle(const accelerator& accel) : FrameProcessorAmpBase(accel) { }

    virtual inline void ApplyColorSimplifier(accelerator& acc, const array<ArgbPackedPixel, 2>& srcFrame, array<ArgbPackedPixel, 2>& destFrame, UINT neighborWindow)
    {
        ::ApplyColorSimplifierHelper(acc, srcFrame, destFrame, neighborWindow);
    }

    virtual inline void ApplyEdgeDetection(accelerator& acc, const array<ArgbPackedPixel, 2>& srcFrame, array<ArgbPackedPixel, 2>& destFrame, 
                        const array<ArgbPackedPixel, 2>& orgFrame, UINT simplifierNeighborWindow)
    {
        ::ApplyEdgeDetectionHelper(acc, srcFrame, destFrame, orgFrame, simplifierNeighborWindow);
    }
};

class FrameProcessorAmpSingleTiled : public FrameProcessorAmpBase
{
public:
    FrameProcessorAmpSingleTiled(const accelerator& accel) : FrameProcessorAmpBase(accel) { }

    virtual inline void ApplyColorSimplifier(accelerator& acc, const array<ArgbPackedPixel, 2>& srcFrame, array<ArgbPackedPixel, 2>& destFrame, UINT neighborWindow)
    {
        ::ApplyColorSimplifierTiledHelper(acc, srcFrame, destFrame, neighborWindow);
    }

    virtual inline void ApplyEdgeDetection(accelerator& acc, const array<ArgbPackedPixel, 2>& srcFrame, array<ArgbPackedPixel, 2>& destFrame, 
                        const array<ArgbPackedPixel, 2>& orgFrame, UINT simplifierNeighborWindow)
    {
        ::ApplyEdgeDetectionTiledHelper(acc, srcFrame, destFrame, orgFrame, simplifierNeighborWindow);
    }
};