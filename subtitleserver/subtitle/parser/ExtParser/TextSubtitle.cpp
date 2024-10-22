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

#define LOG_TAG "Extsubtitle_TextSubtitle"

#include "TextSubtitle.h"
#include "ExtSubStreamReader.h"
#include "SubtitleLog.h"

TextSubtitle::TextSubtitle(std::shared_ptr<DataSource> source)
{
    mSource = std::move(source);
    mReader = std::make_shared<ExtSubStreamReader>(AML_ENCODING_NONE, mSource);
}

bool TextSubtitle::decodeSubtitles(int idxSubTrackId)
{
    mSource->lseek(0, SEEK_SET);
    mIdxSubTrackId = idxSubTrackId;

    SUBTITLE_LOGI("%s ....", __func__);
    while (true) {
        auto item = this->decodedItem();
        if (item == nullptr) {
            break; // No more data, EOF found.
        }

        for (auto& it : item->lines) {
            std::regex brTag("<br[ ]*/>");
            it = std::regex_replace(it, brTag, "\n");
            std::replace(it.begin(), it.end(), '|', '\n');

            SUBTITLE_LOGI("%s: decoded_item: start=%" PRId64
                          " ms, end=%" PRId64 " ms, text = %s",
                          __func__, item->start, item->end, it.c_str());
        }

        item->start = sub_ms2pts(item->start);
        item->end = sub_ms2pts(item->end);
        mSubData.subtitles.push_back(item);
    }

    // dump(0, nullptr);
    return true;
}

// consume subtitle
std::shared_ptr<AML_SPUVAR> TextSubtitle::popDecodedItem()
{
    if (totalItems() <= 0) {
        return nullptr;
    }

    std::shared_ptr<ExtSubItem> item = mSubData.subtitles.front();
    mSubData.subtitles.pop_front();
    std::shared_ptr<AML_SPUVAR> spu(new AML_SPUVAR());

    spu->pts = item->start;
    spu->m_delay = item->end;

    std::string str;
    std::for_each(item->lines.begin(), item->lines.end(), [&](std::string &s) {
        str.append(s);
        str.append("\n");
    });

    spu->spu_data = (unsigned char *)malloc(str.length()+1);
    memcpy(spu->spu_data, str.c_str(), str.length());
    spu->spu_data[str.length()] = 0;
    spu->buffer_size = str.length();
    spu->isExtSub = true;

    return spu;
}

// return total decoded, not consumed subtitles
int TextSubtitle::totalItems()
{
    return mSubData.subtitles.size();
}

void TextSubtitle::dump(int fd, const char *prefix)
{
    if (fd <= 0) {
        SUBTITLE_LOGI("Total: %zu", mSubData.subtitles.size());
        for (auto i : mSubData.subtitles) {
            SUBTITLE_LOGI("[%" PRId64 ":%" PRId64 "]",
                          sub_pts2ms(i->start), sub_pts2ms(i->end));
            for (auto s :i->lines) {
                SUBTITLE_LOGI("    %s", s.c_str());
            }
        }
    }
    else {
        dprintf(fd, "%s Total: %zu\n", prefix, mSubData.subtitles.size());
        for (auto i : mSubData.subtitles) {
            dprintf(fd, "%s [%" PRId64 ":%" PRId64 "]\n", prefix,
                    sub_pts2ms(i->start), sub_pts2ms(i->end));
            for (auto s :i->lines) {
                dprintf(fd, "%s    %s\n", prefix, s.c_str());
            }
        }
    }
}
