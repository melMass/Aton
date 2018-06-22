/*
Copyright (c) 2016,
Dan Bethell, Johannes Saam, Vahan Sosoyan, Brian Scherbinski.
All rights reserved. See COPYING.txt for more details.
*/

#ifndef FBUpdater_h
#define FBUpdater_h

#include "aton_node.h"

// Our RenderBuffer updater thread
static void FBUpdater(unsigned index, unsigned nthreads, void* data)
{
    Aton* node = reinterpret_cast<Aton*>(data);
    double uiFrame, opFrame, prevFrame = 0;
    const int ms = 20;

    while (node->m_legit)
    {
        uiFrame = node->uiContext().frame();
        opFrame = node->outputContext().frame();
        const size_t fbSize = node->m_framebuffers.size();

        if (node->m_multiframes && fbSize > 1 && uiFrame != prevFrame &&
                                                 uiFrame != opFrame)
        {
            const int f_index = node->getFrameIndex(node->m_frames, uiFrame);
            RenderBuffer& fB = node->m_framebuffers[f_index];
            if (node->m_live_camera)
            {
                node->setCameraKnobs(fB.getCameraFov(),
                                     fB.getCameraMatrix());
            }

            node->flagForUpdate();
            prevFrame = uiFrame;
        }
        else
            SleepMS(ms);
    }
}

#endif /* FBUpdater */
