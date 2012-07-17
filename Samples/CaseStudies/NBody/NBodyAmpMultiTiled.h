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

#include "NBodyAmpTiled.h"

//--------------------------------------------------------------------------------------
//  Tiled, multi-accelerator integration implementation.
//--------------------------------------------------------------------------------------
//
//  This implementation loads all particles onto each available GPU but each GPU only 
//  updates a range of particles. After updates are complete each updated range is copied 
//  into the CPU memory (the m_hostPos and m_hostVel vectors). Once all the data is available 
//  in CPU memory it is copied back to each GPU, which is now ready for the next integration.
//
//  The tile size is passed in as a template parameter allowing the calling code to easily create new 
//  instances with different tile sizes. See NBodyFactory() in NBodyGravityApp.cpp for examples.

template <int TSize>
class NBodyAmpMultiTiled : public INBodyAmp
{
    // These are considered mutable because they are cache arrays for accelerator/host copies.
    // They are member variables so they can be allocated once outside of the Integrate method.
    mutable std::vector<float_3> m_hostPos;
    mutable std::vector<float_3> m_hostVel;

    NBodyAmpTiled<TSize> m_engine;

public:
    NBodyAmpMultiTiled(float softeningSquared, float dampingFactor, float deltaTime, float particleMass, int maxParticles) :
        m_hostPos(maxParticles),
        m_hostVel(maxParticles),
        m_engine(softeningSquared, dampingFactor, deltaTime, particleMass)
    {
    }

    inline int TileSize() const { return m_engine.TileSize(); }

    void Integrate(const std::vector<std::shared_ptr<TaskData>>& particleData, int numParticles) const
    {
        assert(particleData.size() > 1);

        const int tileSize = m_engine.TileSize();
        const size_t numAccs = particleData.size();
        const int rangeSize = ((numParticles / tileSize) / int(numAccs)) * tileSize;
        std::vector<completion_future> copyResult(2 * numAccs);

        // Update range of particles on each accelerator

        for (size_t i = 0; i < numAccs; ++i)
            m_engine.TiledBodyBodyInteraction((*particleData[i]->DataOld), (*particleData[i]->DataNew), int(i * rangeSize), rangeSize, numParticles);

        // Copy updated particles from each accelerator back into host memory.
        // Use asynchronous copies to reduce the latency by queueing all copies in parallel.

        for (size_t i = 0; i < numAccs; ++i)
        {
            const int rangeStart = static_cast<int>(i) * rangeSize;
            index<1> begin(rangeStart);
            extent<1> end(rangeSize);
            array_view<float_3, 1> posSrc = particleData[i]->DataNew->pos.section(begin, end);
            copyResult[i] = copy_async(posSrc, m_hostPos.begin() + rangeStart); 
            array_view<float_3, 1> velSrc = particleData[i]->DataNew->vel.section(begin, end);
            copyResult[i + numAccs] = copy_async(velSrc, m_hostVel.begin() + rangeStart); 
        }

        std::for_each(copyResult.begin(), copyResult.end(), [](completion_future& f) { f.wait(); });

        // Sync updated particles back onto all accelerators. Even for N=58368 simple copy is faster than
        // only copying updated data to individual accelerator.

        for (size_t i = 0; i < numAccs; ++i)
        {
            copyResult[i] = copy_async(m_hostPos.begin(), particleData[i]->DataNew->pos);
            copyResult[i + numAccs] = copy_async(m_hostVel.begin(), particleData[i]->DataNew->vel);
        }
        std::for_each(copyResult.begin(), copyResult.end(), [](completion_future& f) { f.wait(); });
    }
};
