/*
 * Copyright (C) 2014-2024 Amlogic, Inc. All rights reserved.
 *
 * All information contained herein is Amlogic confidential.
 *
 * This software is provided to you pursuant to Software License Agreement
 * (SLA) with Amlogic Inc ("Amlogic"). This software may be used
 * only in accordance with the terms of this agreement.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification is strictly prohibited without prior written permission from
 * Amlogic.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define LOG_TAG "Extsubtitle_Vplayer"

#include "Vplayer.h"

Vplayer::Vplayer(std::shared_ptr<DataSource> source): TextSubtitle(source)
{
    mBuffer = new char[BUFFER_SIZE]();
    if (!mBuffer) {
        SUBTITLE_LOGE("%s: fail to new mBuffer", __func__);
    }
}

Vplayer::~Vplayer()
{
    delete[] mBuffer;
}

// protected
// ==================
std::shared_ptr<ExtSubItem> Vplayer::decodedItem()
{
    if (!mBuffer) {
        SUBTITLE_LOGE("%s: null buffer", __func__);
        return nullptr;
    }

    int a1 = 0;
    int a2 = 0;
    int a3 = 0;
    char text[BUFFER_SIZE] = {0};
    int pattenLen = 0;

    while (true) {
        // Read new line if there is no pending item to wait end item.
        if (!mHasPendingItemForEndTime) {
            memset(mBuffer, 0, sizeof(mBuffer));
            if (mReader->getLine(mBuffer) == nullptr) {
                return nullptr;
            }
        }

        // Parse pending line or new line.
        if (sscanf(mBuffer, "%d:%d:%d:%[^\n\r]", &a1, &a2, &a3, text) < 4) {
            // The begin line must be a correct subtitle line, skip the line if not.
            mHasPendingItemForEndTime = false;
            continue;
        }

        // Make the spu item.
        auto item = std::make_shared<ExtSubItem>();
        item->start =  a1 * 360000 + a2 * 6000 + a3 * 100;
        item->end = item->start + 200;
        item->lines.push_back(std::string(text));

        // Read one more line to check the end time.
        memset(mBuffer, 0, sizeof(mBuffer));
        if (mReader->getLine(mBuffer) == nullptr) {
            // Run one more time to stop it.
            mHasPendingItemForEndTime = false;
            return std::move(item);
        }

        // Check the next line if which is an end time line
        pattenLen = sscanf(mBuffer, "%d:%d:%d:%[^\n\r]", &a1, &a2, &a3, text);
        if (pattenLen == 4) {
            // Which is not a end time line
            mHasPendingItemForEndTime = true;
            return std::move(item);
        } else if (pattenLen == 3) {
            // Found the end time line
            mHasPendingItemForEndTime = false;
            item->end = a1 * 360000 + a2 * 6000 + a3 * 100;
            return std::move(item);
        }

        // Found an unexpected line, continue to read the next line.
        mHasPendingItemForEndTime = false;
        SUBTITLE_LOGE("%s: unexpected line: %s", __func__, mBuffer);
    }
    return nullptr;
}
