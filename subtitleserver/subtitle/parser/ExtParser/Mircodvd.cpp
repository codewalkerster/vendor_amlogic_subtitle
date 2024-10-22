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

#define LOG_TAG "Extsubtitle_Mircodvd"

#include "Mircodvd.h"
#include "SubtitleLog.h"

Mircodvd::Mircodvd(std::shared_ptr<DataSource> source): TextSubtitle(source)
{
    mPtsRate = 24; // fps
}

std::shared_ptr<ExtSubItem> Mircodvd::decodedItem()
{
    char* line = (char *)MALLOC(LINE_LEN);
    if (!line) {
        SUBTITLE_LOGE("[%s::%d] line malloc error %m", __FUNCTION__, __LINE__);
        return nullptr;
    }
    char* subtitleText = (char *)MALLOC(LINE_LEN);
    if (!subtitleText) {
        SUBTITLE_LOGE("[%s::%d] line2 malloc error %m", __FUNCTION__, __LINE__);
        free(line);
        return nullptr;
    }

    memset(line, 0, LINE_LEN);
    memset(subtitleText, 0, LINE_LEN);
    while (mReader->getLine(line)) {
        int start =0, end = 0;
        if (sscanf(line, "{%d}{%d}%[^\r\n]", &start, &end, subtitleText) < 3) {
            if (sscanf(line, "{%d}{}%[^\r\n]", &start, subtitleText) < 2) {
                continue;
            }
        }

        if (start == 1) {
            if (atoi(subtitleText) > 0) {
                mPtsRate = atoi(subtitleText);
            }
            continue;
        }

        auto item = std::shared_ptr<ExtSubItem>(new ExtSubItem());
        item->start = start*100/mPtsRate;
        item->end = end*100/mPtsRate;
        std::string s(subtitleText);
        item->lines.push_back(s);
        free(line);
        free(subtitleText);
        return std::move(item);
    }

    free(line);
    free(subtitleText);
    return nullptr;
}
