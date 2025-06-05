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
#define LOG_TAG "PgsParser"

#include "PgsParser.h"

#include "SubtitleLog.h"
#include "StreamUtils.h"

#include "ParserFactory.h"
#include "VideoInfo.h"


// Follow the latest solution of ffmpeg 7.0.1
// Refer to TV-121429/OTT-63501/SWPL-174424 for history.
// crop table
static const int MAX_NEG_CROP = 1024;
#define times4(x) x, x, x, x
#define times256(x) times4(times4(times4(times4(times4(x)))))
const uint8_t ff_crop_tab[256 + 2 * MAX_NEG_CROP] = {
times256(0x00),
0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,
0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,
0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,
0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x5B,0x5C,0x5D,0x5E,0x5F,
0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,
0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x7B,0x7C,0x7D,0x7E,0x7F,
0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,
0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F,
0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,
0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF,
0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,
0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF,
0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF,
0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF,
times256(0xFF)
};
#define SCALEBITS 10
#define ONE_HALF  (1 << (SCALEBITS - 1))
#define FIX(x)    ((int) ((x) * (1<<SCALEBITS) + 0.5))

#define YUV_TO_RGB1_CCIR(cb1, cr1)                        \
{                                                         \
    cb = (cb1) - 128;                                     \
    cr = (cr1) - 128;                                     \
    r_add = FIX(1.40200*255.0/224.0) * cr + ONE_HALF;     \
    g_add = - FIX(0.34414*255.0/224.0) * cb -             \
            FIX(0.71414*255.0/224.0) * cr + ONE_HALF;     \
    b_add = FIX(1.77200*255.0/224.0) * cb + ONE_HALF;     \
}

#define YUV_TO_RGB1_CCIR_BT709(cb1, cr1)                  \
{                                                         \
    cb    = (cb1) - 128;                                  \
    cr    = (cr1) - 128;                                  \
    r_add = ONE_HALF + FIX(1.5747 * 255.0 / 224.0) * cr;  \
    g_add = ONE_HALF - FIX(0.1873 * 255.0 / 224.0) * cb - \
                       FIX(0.4682 * 255.0 / 224.0) * cr;  \
    b_add = ONE_HALF + FIX(1.8556 * 255.0 / 224.0) * cb;  \
}

// To be used for the BT709 variant as well
#define YUV_TO_RGB2_CCIR(r, g, b, y1)       \
{                                           \
    y = ((y1) - 16) * FIX(255.0/219.0);     \
    r = cm[(y + r_add) >> SCALEBITS];       \
    g = cm[(y + g_add) >> SCALEBITS];       \
    b = cm[(y + b_add) >> SCALEBITS];       \
}

#define ARGB(r,g,b,a) (((unsigned)(a) << 24) | ((r) << 16) | ((g) << 8) | (b))

namespace {

static std::string int2StrWithLen(int num, int len) {
    std::string str = std::to_string(abs(num));
    while (str.length() < len) {
        str = "0" + str;
    }
    if (num < 0) {
        return "-" + str;
    }
    return str;
}

}

PgsParser::PgsParser(std::shared_ptr<DataSource> source)
{
    SUBTITLE_LOGI("enter %s", __func__);

    mDataSource = std::move(source);
    mParseType = TYPE_SUBTITLE_PGS;

    memset(&mPgsContext, 0, sizeof(PGSSubContext));
    mStopDecodeThread = false;
    mDecodeThread = std::thread(&PgsParser::_loopDecodePgsData, this);
    checkDebug();
}

PgsParser::~PgsParser()
{
    SUBTITLE_LOGI("enter %s", __func__);
    if (!mStopDecodeThread) {
        mStopDecodeThread = true;
        std::unique_lock<std::mutex> autolock(mDecodeMutex);
        mDecodeCv.notify_all();
    }
    if (mDecodeThread.joinable()) {
        mDecodeThread.join();
    }
    mPgsContextList.clear();
    for (auto i = 0; i < mPgsContext.presentation.object_count; ++i) {
        auto object = findObject(mPgsContext.presentation.objects[i].id,
                                 &mPgsContext.objects);
        if (!object) {
            continue;
        }

        // The malloc and free are done in decodeRleAndRenderBitmap actually
        // Add such housekeeping for safety.
        AVSubtitleRect* const rect = &mPgsContext.presentation.present_sub_rect[i];
        if (rect->decodedRle) {
            free(rect->decodedRle);
        }
        // rect->bitmap memory will be freed in de-constructor of AML_SPUVAR
    }
    SUBTITLE_LOGI("%s DONE", __func__);
}

// Parser interfaces
// =====================
bool PgsParser::stopParser()
{
    SUBTITLE_LOGI("enter %s", __func__);
    {
        mStopDecodeThread = true;
        std::unique_lock<std::mutex> autolock(mDecodeMutex);
        mDecodeCv.notify_all();
    }
    SUBTITLE_LOGI("%s: stop decoding thread", __func__);
    if (mDecodeThread.joinable()) {
        mDecodeThread.join();
    }
    mPgsContextList.clear();
    SUBTITLE_LOGI("%s: stop data reading thread", __func__);
    return Parser::stopParser();
}

int PgsParser::parse()
{
    while (!mThreadExitRequested) {
        readDataSource();
    }
    SUBTITLE_LOGI("%s: STOPPED", __func__);
    return 0;
}

void PgsParser::dump(int fd, const char *prefix)
{
    dprintf(fd, "%s PGS Parser\n", prefix);
    dumpCommon(fd, prefix);

    dprintf(fd, "%s     startTime   = %" PRId64 ", endTime = 0\n", prefix,
            mPgsContext.presentation.pts);
    dprintf(fd, "%s     objectCount = %d\n", prefix,
            mPgsContext.presentation.object_count);
    dprintf(fd, "%s     id_number   = %d\n", prefix,
            mPgsContext.presentation.id_number);
    dprintf(fd, "%s     palette_id  = %d\n", prefix,
            mPgsContext.presentation.palette_id);
    dprintf(fd, "%s     videoW = %d, videoH = %d\n", prefix,
            mPgsContext.presentation.videoWidth,
            mPgsContext.presentation.videoHeight);

    for (auto i = 0; i < mPgsContext.presentation.object_count; ++i) {
        auto object = findObject(mPgsContext.presentation.objects[i].id,
                                 &mPgsContext.objects);
        if (!object) {
            continue;
        }
        auto rect = &mPgsContext.presentation.present_sub_rect[i];
        dprintf(fd, "\n%s Presentation_Image_%d:\n", prefix, i);
        dprintf(fd, "%s     object_segment_id = %d\n", prefix, rect->object_segment_id);
        dprintf(fd, "%s     decodedRle_size   = %d\n", prefix, rect->decodedRleSize);
        dprintf(fd, "%s     bitmap_size       = %d\n", prefix, rect->bitmapSize);
        dprintf(fd, "%s     is_forced_display = %s\n", prefix,
                rect->is_forced_display ? "true" : "false");
        dprintf(fd, "%s     rect: x = %d y = %d w = %d h = %d\n", prefix,
                rect->x, rect->y, rect->w, rect->h);
    }
    dprintf(fd, "-------------------------------------------------------------\n");
}

void PgsParser::notifyRenderTimeChanged(int64_t renderTime)
{
    Parser::notifyRenderTimeChanged(renderTime);
    std::unique_lock<std::mutex> autolock(mDecodeMutex);
    mDecodeCv.notify_all();
}


// Private functions
// =====================
void PgsParser::checkDebug()
{
#ifdef NEED_DUMP_ANDROID
        char value[PROPERTY_VALUE_MAX] = {0};
        property_get("vendor.subtitle.dump", value, "false");
        mDumpSub = strcmp(value, "true") == 0;
#endif
}

void PgsParser::_loopDecodePgsData()
{
    while (!mThreadExitRequested) {
        std::unique_lock<std::mutex> autolock(mDecodeMutex);
        mDecodeCv.wait(autolock);
        if (mStopDecodeThread) {
            break;
        }
        handleDisplayEndSegment();
    }
    SUBTITLE_LOGI("%s: STOPPED", __func__);
}

int PgsParser::readDataSource()
{
    mState = SUB_PLAYING;

    uint8_t buf = 0;
    uint64_t packetHeader = 0;
    while (mDataSource->read(&buf, 1) == 1) {
        if (mState == SUB_STOP) {
            return 0;
        }

        packetHeader = ((packetHeader << 8) & 0x000000ffffffffff) | buf;

        // PES from demuxer
        if ((packetHeader & 0xffffffff) == 0x000001bd) {
            hwDemuxParser();
            return 0;
        }

        // From amnuplayer
        // Please read AmSubtitle::sendToSubtitleService() in amnuplayer for details:
        //     - The total header size is 24, the first 5 bytes is sync words;
        //     - SoftDemuxParse reads and parses the remain 24-5 bytes
        int syncWord = (packetHeader & 0xffffffffff) >> 8;
        uint8_t syncType = packetHeader & 0xff;
        if (syncWord == AML_PARSER_SYNC_WORD
            && (syncType == 0x77 || syncType == 0xaa)) {
            softDemuxParser();
            return 0;
        }
    }
    return -1;
}

void PgsParser::softDemuxParser()
{
    SUBTITLE_LOGI("enter %s", __func__);

    // 1. Read the header data, please see branch p-amlogic in
    // repo av-restricted/platform/vendor/amnuplayer
    static const int HEADER_LENGTH = 19;
    char header[HEADER_LENGTH] = {0};
    if (mDataSource->read(header, HEADER_LENGTH) != HEADER_LENGTH) {
        SUBTITLE_LOGE("%s: fail to read header", __func__);
        return;
    }

    // DEBUG_NEED: Leave the following log print here for checking amnuplayer's
    // data structure
    /*
    for (auto i = 0; i < 19; ++i) {
        SUBTITLE_LOGI("%d: 0x%0X\n", i, header[i]);
    }
    */

    // Skip 3 bytes type
    auto dataLen = subPeekAsUint32(header + 3);
    auto pts = subPeekAsUint64(header + 7);
    if (pts == 0) {
        SUBTITLE_LOGE("%s: get zero pts", __func__);
        return;
    }

    // The unit of duration is: duration = ms*90
    auto duration = subPeekAsUint32(header + 15);

    SUBTITLE_LOGI("%s: dataLen=%-6d, pts=%" PRId64 ", duration=%d",
                  __func__, dataLen, pts, duration);

    // 2. Read the packet data
    auto dataBuff = std::vector<uint8_t>(dataLen);
    if (mDataSource->read(dataBuff.data(), dataLen) != dataLen) {
        SUBTITLE_LOGE("%s: fail to read data", __func__);
        return;
    }

    auto buff = const_cast<const uint8_t*>(dataBuff.data());

    auto buff_end = buff + dataLen;
    while (buff < buff_end) {
        if (mState == SUB_STOP) {
            return;
        }

        auto packetType = bytestream_get_byte(&buff);
        auto packetLen  = bytestream_get_be16(&buff);
        if (packetLen == 0 && packetType != 0x80) {
            SUBTITLE_LOGI("%s: get zero packetLen, packetType=0x%02x",
                          __func__, packetType);
            return;
        }
        if (buff + packetLen > buff_end) {
            SUBTITLE_LOGE("%s: need more data, need %d(pgs packet len), but read %d",
                          __func__, packetLen, static_cast<int>(buff_end - buff));
            break;
        }

        // "PG" + pts + dts + segment type + segment size
        auto pgsHeaderSize = 2 + 8 + 1 + 2;
        auto pgsPacketSize = pgsHeaderSize + packetLen;
        // NOTE: this check comes from old PGS parser as some memory issue,
        // which is used for all subtitle parsers
        if (pgsPacketSize > OSD_HALF_SIZE*4) {
            SUBTITLE_LOGE("%s: PGS packet is too big: packetLen(%d) > max(%d)",
                          __func__, packetLen, OSD_HALF_SIZE*4);
            return;
        }

        std::vector<uint8_t> pgsPacketData;
        pgsPacketData.reserve(pgsPacketSize);
        pgsPacketData.push_back('P');
        pgsPacketData.push_back('G');
        // pts
        pgsPacketData.push_back(0xFF & (pts >> 24));
        pgsPacketData.push_back(0xFF & (pts >> 16));
        pgsPacketData.push_back(0xFF & (pts >> 8));
        pgsPacketData.push_back(0xFF & pts);
        // dts
        pgsPacketData.push_back(0);
        pgsPacketData.push_back(0);
        pgsPacketData.push_back(0);
        pgsPacketData.push_back(0);
        // segment type
        pgsPacketData.push_back(0xFF & packetType);
        // segment size
        pgsPacketData.push_back(0xFF & (packetLen >> 8));
        pgsPacketData.push_back(0xFF & packetLen);

        pgsPacketData.insert(pgsPacketData.end(), buff, buff + packetLen);
        buff += packetLen;

        decode(pgsPacketData);
    }
}

// TODO: PES demux parser is not used by now, implement it for future extension.
void PgsParser::hwDemuxParser()
{
    SUBTITLE_LOGI("enter %s", __func__);

    uint8_t dataLen[2] = {0};
    if (mDataSource->read(dataLen, 2) != 2) {
        SUBTITLE_LOGE("%s: fail to read packet length", __func__);
        return;
    }
    const uint8_t* dataBuff = dataLen;
    auto packetLen = bytestream_get_be16(&dataBuff);
    if (packetLen < 3) {
        SUBTITLE_LOGE("%s: packet size is <3", __func__);
        return;
    }
    char header[3] = {0};
    if (mDataSource->read(header, 3) != 3) {
        SUBTITLE_LOGE("%s: fail to read header", __func__);
        return;
    }
    packetLen -= 3;
    auto pesHeaderLen = header[2];
    if (packetLen < pesHeaderLen) {
        SUBTITLE_LOGE("%s: packet size %d < pes len %d",
              __func__, packetLen, pesHeaderLen);
        return;
    }

    bool needSkipPkt = true;
    int64_t pts = 0, dts = 0;
    auto tag = header[1] & 0xc0;
    if (tag == 0x80 || tag == 0xc0) {
        needSkipPkt = false;
        auto pesHeader = std::vector<char>(pesHeaderLen);
        if (mDataSource->read(pesHeader.data(), pesHeaderLen) != pesHeaderLen) {
            SUBTITLE_LOGE("%s: fail to read pes header", __func__);
            return;
        }
        auto pesBuff = pesHeader.data();
        pts = (int64_t)(pesBuff[0] & 0xe) << 29;
        pts = pts | ((int64_t)(pesBuff[1] & 0xff) << 22);
        pts = pts | ((int64_t)(pesBuff[2] & 0xfe) << 14);
        pts = pts | ((int64_t)(pesBuff[3] & 0xff) << 7);
        pts = pts | ((int64_t)(pesBuff[4] & 0xfe) >> 1);
        packetLen -= pesHeaderLen;
        if (tag == 0xc0) {
            dts = (int64_t)(pesBuff[0] & 0xe) << 29;
            dts = dts | ((int64_t)(pesBuff[1] & 0xff) << 22);
            dts = dts | ((int64_t)(pesBuff[2] & 0xfe) << 14);
            dts = dts | ((int64_t)(pesBuff[3] & 0xff) << 7);
            dts = dts | ((int64_t)(pesBuff[4] & 0xfe) >> 1);
            packetLen -= 5;
        }
    }

    if (needSkipPkt) {
        char tmp;
        for (auto i = 0; i < packetLen; ++i) {
            if (mDataSource->read(&tmp, 1) == 0) break;
        }
        return;
    }

    if (pts == 0 || packetLen <= 0) {
        SUBTITLE_LOGE("%s: get zero pts or no data", __func__);
        return;
    }

    // "PG" + pts + dts
    // segment type + segment size are included in next reading.
    auto pgsHeaderSize = 2 + 8;
    auto pgsPacketSize = pgsHeaderSize + packetLen;
    if (mDataSource->availableDataSize() < packetLen || mState == SUB_STOP) {
        SUBTITLE_LOGI("%s: stopped or no enough data", __func__);
        return;
    }

    std::vector<uint8_t> pgsPacketData;
    pgsPacketData.reserve(pgsPacketSize);
    pgsPacketData.push_back('P');
    pgsPacketData.push_back('G');
    // pts
    pgsPacketData.push_back(0xFF & (pts >> 24));
    pgsPacketData.push_back(0xFF & (pts >> 16));
    pgsPacketData.push_back(0xFF & (pts >> 8));
    pgsPacketData.push_back(0xFF & pts);
    // dts
    pgsPacketData.push_back(0xFF & (dts >> 24));
    pgsPacketData.push_back(0xFF & (dts >> 16));
    pgsPacketData.push_back(0xFF & (dts >> 8));
    pgsPacketData.push_back(0xFF & dts);

    // segment type + segment size are included in this reading.
    if (mDataSource->read(pgsPacketData.data() + pgsHeaderSize, packetLen) != packetLen) {
        SUBTITLE_LOGE("%s: fail to read data", __func__);
        return;
    }

    decode(pgsPacketData);
}

void PgsParser::decode(const std::vector<uint8_t>& pgsPacket)
{
    auto buf = const_cast<const uint8_t*>(pgsPacket.data());
    auto buf_size = pgsPacket.size();
    // The buffer can't be nullptr and the length must be more than 3 as the logic.
    assert(buf && buf_size > 3);

    auto buf_end = buf + buf_size;
    while (buf < buf_end) {
        if (mState == SUB_STOP) {
            return;
        }
        // Skip "PG"
        buf += 2;

        int pts = bytestream_get_be32(&buf);

        // Prepare log time
        int ptsMs = pts / DEFAULT_DVB_TIME_MULTI;
        // Set the first pts
        if (mFirstPtsMs == 0 || mFirstPtsMs > ptsMs) {
            mFirstPtsMs = ptsMs;
            SUBTITLE_LOGI("%s: mFirstPtsMs=%d ms mPresentationTimeMs=%" PRId64 " ms",
                          __func__, mFirstPtsMs,
                          mPresentationTime/DEFAULT_DVB_TIME_MULTI);
        }
        int msFromFirstPts = ptsMs - mFirstPtsMs;

        int ms = ptsMs % 1000;
        int second = ptsMs / 1000 % 60;
        int minute = ptsMs / 1000 / 60 % 60;
        int hour = ptsMs / 1000 / 60 / 60;
        // log PTS time
        std::string logPts = "[" + std::to_string(pts) + "]" + "["
            + int2StrWithLen(hour, 2) + ":" + int2StrWithLen(minute, 2)
            + ":" + int2StrWithLen(second, 2) + "." + int2StrWithLen(ms, 3) + "]";
        // log duration from the first PTS
        logPts += ("[" + int2StrWithLen(msFromFirstPts/1000, 4) + "."
            + int2StrWithLen(abs(msFromFirstPts)%1000, 3) + " s]");

        int dts = bytestream_get_be32(&buf);
        uint8_t segment_type = bytestream_get_byte(&buf);
        int segment_length = bytestream_get_be16(&buf);

        // softDemuxParse and hwDemuxParse already be sure the data is health.
        assert(buf + segment_length == buf_end);
        switch (segment_type) {
        case PRESENTATION_COMPOSITION_SEGMENT: // 1 PCS = 0x16
        {
            int prePtsTimeMs = ptsMs - mPresentationTime/DEFAULT_DVB_TIME_MULTI;
            if ( prePtsTimeMs < DECODE_PRE_TIME_MS) {
                SUBTITLE_LOGI("%s_sequence_start_jitter: pts=%d(%d ms) prePtsTimeMs= %d ms",
                              __func__, pts, ptsMs, prePtsTimeMs);
            }
            else {
                SUBTITLE_LOGI("%s_sequence_start: pts=%d(%d ms) prePtsTimeMs= %d ms",
                              __func__, pts, ptsMs, prePtsTimeMs);
            }
            mDecodeSequenceTracker = "[ 1_PCS --> ";
            mPgsContext.presentation.pts = pts;
            parsePresentationSegment(buf, segment_length);
            break;
        }
        case WINDOW_DEFINITION_SEGMENT: // 2 WDS = 0x17
        {
             mDecodeSequenceTracker += "2_WDS --> ";
             /*
             * Window Segment Structure (No new information provided):
             *     2 bytes: Unknown,
             *     2 bytes: X position of subtitle,
             *     2 bytes: Y position of subtitle,
             *     2 bytes: Width of subtitle,
             *     2 bytes: Height of subtitle.
             */
            break;
        }
        case PALETTE_DEFINITION_SEGMENT: // 3 PDS = 0x14
        {
            mDecodeSequenceTracker += "3_PDS --> ";
            parsePaletteSegment(buf, segment_length);
            break;
        }
        case OBJECT_DEFINITION_SEGMENT: // 4 ODS = 0x15
        {
            auto objectId = parseObjectSegment(buf, segment_length);
            mDecodeSequenceTracker += ("4_ODS(" + std::to_string(objectId) + ") --> ");
            break;
        }
        case END_DISPLAY_SEGMENT: // 5 END = 0x80
        {
            mDecodeSequenceTracker += "5_END ]";
            {
                std::unique_lock<std::mutex> autolock(mDecodeMutex);
                mPgsContextList.push_back(std::make_shared<PGSSubContext>(mPgsContext));
            }
            // onRenderTimeChanged will trigger the display.
            break;
        }
        default: {
            SUBTITLE_LOGE("%s_sequence: Unknown subtitle segment type 0x%x, length %d",
                          __func__, segment_type, segment_length);
            break;
        }
        }
        std::string log = logPts + mDecodeSequenceTracker;
        SUBTITLE_LOGI("%s_sequence: %s\n", __func__, log.c_str());
        buf += segment_length;
    }
}

// 1 PCS
void PgsParser::parsePresentationSegment(const uint8_t* buf, int buf_size)
{
    assert(buf);
    auto buf_end = buf + buf_size;

    // Video descriptor
    mPgsContext.presentation.videoWidth  = bytestream_get_be16(&buf);
    mPgsContext.presentation.videoHeight = bytestream_get_be16(&buf);

    // Skip Frame rate, which is always 0x10, can be ignored.
    buf += 1;
    mPgsContext.presentation.id_number = bytestream_get_be16(&buf);

    /*
     * state is a 2 bit field that defines pgs epoch boundaries
     * 00 - Normal, previously defined objects and palettes are still valid
     * 01 - Acquisition point, previous objects and palettes can be released
     * 10 - Epoch start, previous objects and palettes can be released
     * 11 - Epoch continue, previous objects and palettes can be released
     *
     * reserved 6 bits discarded
     */
    auto state = bytestream_get_byte(&buf) >> 6;
    if (state != 0) {
        flushPgsContext();
    }

    // skip palette_update_flag (0x80)
    buf += 1;
    mPgsContext.presentation.palette_id   = bytestream_get_byte(&buf);
    mPgsContext.presentation.object_count = bytestream_get_byte(&buf);
    // Sometimes there are no objects in PGS packet
    SUBTITLE_LOGI("%s: videoW%d, videoH%d, id_number=%d, state=%d, palette_id=%d,"
                  " object_count=%d\n",
                  __func__,
                  mPgsContext.presentation.videoWidth,
                  mPgsContext.presentation.videoHeight,
                  mPgsContext.presentation.id_number,
                  state,
                  mPgsContext.presentation.palette_id,
                  mPgsContext.presentation.object_count);

    if (mPgsContext.presentation.object_count > MAX_OBJECT_REFS) {
        SUBTITLE_LOGE("%s: Invalid number of presentation objects %d", __func__,
                      mPgsContext.presentation.object_count);
        mPgsContext.presentation.object_count = 2;
        return;
    }

    for (auto i = 0; i < mPgsContext.presentation.object_count; ++i) {
        PGSSubObjectRef *const object = &mPgsContext.presentation.objects[i];

        if (buf_end - buf < 8) {
            SUBTITLE_LOGE("%s: insufficient space for object", __func__);
            mPgsContext.presentation.object_count = i;
            return;
        }

        object->id               = bytestream_get_be16(&buf);
        object->window_id        = bytestream_get_byte(&buf);
        object->composition_flag = bytestream_get_byte(&buf);

        object->x = bytestream_get_be16(&buf);
        object->y = bytestream_get_be16(&buf);

        // If cropping
        if (object->composition_flag & 0x80) {
            object->crop_x = bytestream_get_be16(&buf);
            object->crop_y = bytestream_get_be16(&buf);
            object->crop_w = bytestream_get_be16(&buf);
            object->crop_h = bytestream_get_be16(&buf);
        }
        SUBTITLE_LOGI("%s_%d: objectId=%d, composition_flag=0x%02x, (x%d, y%d)",
                      __func__, i, object->id,
                      object->composition_flag, object->x, object->y);
        if (object->x > mPgsContext.presentation.videoWidth
            || object->y > mPgsContext.presentation.videoHeight) {
            SUBTITLE_LOGE("%s: out of video bounds: subtitle(x%d, y%d), video(w%d, h%d)",
                          __func__, object->x, object->y,
                          mPgsContext.presentation.videoWidth,
                          mPgsContext.presentation.videoHeight);
            object->y = object->x = 0;
            return;
        }
    }
}

// 3 PDS
void PgsParser::parsePaletteSegment(const uint8_t* buf, int buf_size)
{
    assert(buf);
    PGSSubPalette* palette;

    auto buf_end = buf + buf_size;
    const uint8_t* cm = ff_crop_tab + MAX_NEG_CROP;
    int color_id;
    int y, cb, cr, alpha;
    int r, g, b, r_add, g_add, b_add;
    int id;

    id  = bytestream_get_byte(&buf);
    palette = findPalette(id, &mPgsContext.palettes);
    if (!palette) {
        if (mPgsContext.palettes.count >= MAX_EPOCH_PALETTES) {
            SUBTITLE_LOGE("%s: Too many palettes in epoch", __func__);
            return;
        }
        palette = &mPgsContext.palettes.palette[mPgsContext.palettes.count++];
        palette->id = id;
    }

    // Skip palette version
    buf += 1;

    while (buf < buf_end) {
        color_id  = bytestream_get_byte(&buf);
        y         = bytestream_get_byte(&buf);
        cr        = bytestream_get_byte(&buf);
        cb        = bytestream_get_byte(&buf);
        alpha     = bytestream_get_byte(&buf);

        // Default to BT.709 colorspace. In case of <= 576 height use BT.601
        if (mPgsContext.presentation.videoHeight <= 0
            || mPgsContext.presentation.videoHeight > 576) {
            YUV_TO_RGB1_CCIR_BT709(cb, cr);
        } else {
            YUV_TO_RGB1_CCIR(cb, cr);
        }
        YUV_TO_RGB2_CCIR(r, g, b, y);

        palette->argb[color_id] = ARGB(r, g, b, alpha);
    }
}

// 4 ODS
int PgsParser::parseObjectSegment(const uint8_t* buf, int buf_size)
{
    assert(buf);
    if (buf_size <= 4) {
        return -1;
    }
    buf_size -= 4;

    int id = bytestream_get_be16(&buf);
    auto object = findObject(id, &mPgsContext.objects);
    if (!object) {
        if (mPgsContext.objects.count >= MAX_EPOCH_OBJECTS) {
            SUBTITLE_LOGE("%s: Too many objects in epoch", __func__);
            return -1;
        }
        object = &mPgsContext.objects.object[mPgsContext.objects.count++];
        object->id = id;
    }

    // skip object version number
    buf += 1;

    // Read the Sequence Description to determine if start of RLE data or
    // appended to previous RLE
    uint8_t sequence_desc = bytestream_get_byte(&buf);

    if (!(sequence_desc & 0x80)) {
        // Additional RLE data
        if (buf_size > object->rle_remaining_len) {
            SUBTITLE_LOGE("%s: buf_size %d > rle_remaining_len %d",
                          __func__, buf_size, object->rle_remaining_len);
            return -1;
        }

        object->rle.insert(object->rle.end(), buf, buf + buf_size);
        object->rle_data_len += buf_size;
        assert(object->rle.size() == object->rle_data_len);
        object->rle_remaining_len -= buf_size;
        return id;
    }

    if (buf_size <= 7) {
        SUBTITLE_LOGE("%s: buf_size %d <= 7", __func__, buf_size);
        return -1;
    }
    buf_size -= 7;

    // Decode rle bitmap length, stored size includes width/height data
    unsigned int rle_bitmap_len = bytestream_get_be24(&buf) - 2*2;

    if (buf_size > rle_bitmap_len) {
        SUBTITLE_LOGE("%s: Buffer dimension %d larger than the expected RLE data %d",
                      __func__, buf_size, rle_bitmap_len);
        return -1;
    }

    // Get bitmap dimensions from data
    unsigned int width  = bytestream_get_be16(&buf);
    unsigned int height = bytestream_get_be16(&buf);

    // Make sure the bitmap is not too large
    if (mPgsContext.presentation.videoWidth < width
        || mPgsContext.presentation.videoHeight < height
        || !width || !height) {
        SUBTITLE_LOGE("%s: Bitmap dimensions (%dx%d) invalid",
                      __func__, width, height);
        return -1;
    }

    object->w = width;
    object->h = height;

    object->rle.clear();
    object->rle.insert(object->rle.begin(), buf, buf + buf_size);
    object->rle_data_len = buf_size;
    object->rle_remaining_len = rle_bitmap_len - buf_size;
    return object->id;
}

// 5 END
void PgsParser::handleDisplayEndSegment()
{
    // 1. Drop the PGS contexts suppose which are received during seek
    auto presentationPtsMs = mPresentationTime/DEFAULT_DVB_TIME_MULTI + DECODE_PRE_TIME_MS;
    mPgsContextList.erase(std::remove_if(mPgsContextList.begin(), mPgsContextList.end(),
            [=] (const std::shared_ptr<PGSSubContext>& it) {
                int ptsMs = it->presentation.pts / DEFAULT_DVB_TIME_MULTI;
                int diffTimeMs = ptsMs - presentationPtsMs;
                // Skip the playback start time
                bool isWrongPgs = diffTimeMs >= WRONG_PGS_PTS_TIME_MS
                    && abs(ptsMs - mFirstPtsMs) > 10000;
                if (isWrongPgs) {
                    SUBTITLE_LOGI("handleDisplayEndSegment_decode_drop: pts=%" PRId64
                        ", ptsDiff= %d ms", it->presentation.pts, diffTimeMs);
                }
                return isWrongPgs;
            }),
            mPgsContextList.end());

    // 2. Find PGS contexts need to be decoded
    std::vector<std::shared_ptr<PGSSubContext>> presentPgsContexts;
    for (auto it : mPgsContextList) {
        auto presentationPts =
            mPresentationTime + DECODE_PRE_TIME_MS*DEFAULT_DVB_TIME_MULTI;
        auto ptsDiffMs =
            (it->presentation.pts - presentationPts) / DEFAULT_DVB_TIME_MULTI;
        if (ptsDiffMs > 0) {
            break;
        }
        presentPgsContexts.push_back(it);
    }

    // 3. Decode PGS contexts
    bool isDecodeBusy = false;
    for (auto i = 0; i < presentPgsContexts.size(); ++i) {
        if (mState == SUB_STOP) {
            return;
        }

        int ptsMsDiff2Next = 0;
        auto it = presentPgsContexts[i];
        // Stop post display cleanup item if the next pts is same as this null spu
        if (i + 1 < presentPgsContexts.size()) {
            auto next = presentPgsContexts[i+1];
            ptsMsDiff2Next =
                (next->presentation.pts - it->presentation.pts) / DEFAULT_DVB_TIME_MULTI;
            if (it->presentation.object_count == 0
                && it->presentation.pts == next->presentation.pts) {
                mPgsContextList.erase(mPgsContextList.begin());
                continue;
            }
        }

        mCurrentTimeMs = getCurrentTimeMs();
        decodeRleAndRenderBitmap(*it);
        postDecodedItem(*it, true);
        mPgsContextList.erase(mPgsContextList.begin());

        auto spendTimeMs = getCurrentTimeMs() - mCurrentTimeMs;
        if (spendTimeMs >= DECODE_PRE_TIME_MS && spendTimeMs >= ptsMsDiff2Next) {
           SUBTITLE_LOGI("%s_decode_jitter:[pts=%" PRId64 "] spend %" PRIu64
                         " ms, total = %zu/%zu",
                         __func__, it->presentation.pts,
                         spendTimeMs, presentPgsContexts.size(),
                         mPgsContextList.size());
           isDecodeBusy = true;
           break;
        }
    }
    presentPgsContexts.clear();

    // 4. Drop remaining if decode is busy
    if (isDecodeBusy && mPgsContextList.size() > 0) {
        int dropNum = 0;
        auto queuedNum = mPgsContextList.size();
        // Be sure the screen cleanup items are not dropped.
        while (!mPgsContextList.empty()) {
            auto it = mPgsContextList.front();
            if (it->presentation.object_count == 0) {
                break;
            }
            mPgsContextList.erase(mPgsContextList.begin());
            ++dropNum;
        }
        SUBTITLE_LOGI("%s_decode_jitter_drop: drop %d/%zu",
                      __func__, dropNum, queuedNum);
    }
}

void PgsParser::decodeRleAndRenderBitmap(PGSSubContext& subContext)
{
    auto palette = findPalette(subContext.presentation.palette_id,
                               &subContext.palettes);
    if (!palette) {
        SUBTITLE_LOGE("%s: Invalid palette id %d", __func__,
                      subContext.presentation.palette_id);
        return;
    }

    for (auto i = 0; i < subContext.presentation.object_count; ++i) {
        auto object = findObject(subContext.presentation.objects[i].id,
                                 &subContext.objects);
        if (!object) {
            SUBTITLE_LOGE("%s: Invalid object, objectId=%d", __func__,
                          subContext.presentation.objects[i].id);
            continue;
        }

        AVSubtitleRect* const rect = &subContext.presentation.present_sub_rect[i];
        rect->x = subContext.presentation.objects[i].x;
        rect->y = subContext.presentation.objects[i].y;
        rect->is_valid = true;
        if (object->rle.size() > 0) {
            rect->w = object->w;
            rect->h = object->h;

            if (object->rle_remaining_len) {
                SUBTITLE_LOGE("%s: RLE data length %u is %u bytes shorter than expected",
                              __func__, object->rle_data_len, object->rle_remaining_len);
                rect->is_valid = false;
                return;
            }

            rect->decodedRleSize = rect->w * rect->h;
            // The malloc should be freed after renderRle2Bitmap
            rect->decodedRle = static_cast<uint8_t*>(malloc(rect->decodedRleSize));
            if (!rect->decodedRle) {
                SUBTITLE_LOGE("%s: malloc error %m", __func__);
                return;
            }
            auto ret = decodeRle(rect, object->rle);
            if (!ret) {
                rect->w = 0;
                rect->h = 0;
                rect->is_valid = false;
                free(rect->decodedRle);
                rect->decodedRle = nullptr;
                rect->decodedRleSize = 0;
                continue;
            }
        }

        memcpy(&rect->palette, &palette->argb, 0x100 * sizeof(uint32_t));

        // Must be called after palette is filled and rle is decoded
        renderRle2Bitmap(rect);
        free(rect->decodedRle);
        rect->decodedRle = nullptr;
        rect->decodedRleSize = 0;

        rect->is_forced_display = \
            (subContext.presentation.objects[i].composition_flag & 0x40);
        // NOTE: spu's object_segment_id is mapped to reference object id.
        rect->object_segment_id = i;
    }
}

bool PgsParser::decodeRle(AVSubtitleRect* rect, const std::vector<uint8_t>& rleData)
{
    assert(rect && !rleData.empty());

    auto rle_buf = const_cast<const uint8_t*>(rleData.data());
    auto rle_bitmap_end = rle_buf + rleData.size();

    int pixel_count = 0;
    int line_count  = 0;
    while (rle_buf < rle_bitmap_end && line_count < rect->h) {
        auto color = bytestream_get_byte(&rle_buf);
        auto run   = 1;
        if (color == 0x00) {
            auto flags = bytestream_get_byte(&rle_buf);
            run = flags & 0x3f;
            if (flags & 0x40) {
                run = (run << 8) + bytestream_get_byte(&rle_buf);
            }
            color = flags & 0x80 ? bytestream_get_byte(&rle_buf) : 0;
        }

        if (run > 0 && pixel_count + run <= rect->w * rect->h) {
            memset(rect->decodedRle + pixel_count, color, run);
            pixel_count += run;
        }
        else if (!run) {
            // New Line. Check if correct pixels decoded, if not display warning
            // and adjust bitmap pointer to correct new line position.
            if (pixel_count % rect->w > 0) {
                SUBTITLE_LOGE("%s: Decoded %d pixels, when line should be %d pixels",
                              __func__, pixel_count % rect->w, rect->w);
                return false;
            }
            line_count++;
        }
    }

    if (pixel_count < rect->w * rect->h) {
        SUBTITLE_LOGE("%s: Insufficient RLE data for subtitle", __func__);
        return false;
    }
    return true;
}

void PgsParser::renderRle2Bitmap(AVSubtitleRect* rect)
{
    assert(rect && rect->decodedRle);

    rect->bitmapSize = 4 * rect->decodedRleSize;
    // NOTE: The malloc will be freed in AML_SPUVAR's de-constructor after postDecodedItem
    rect->bitmap = static_cast<uint8_t*>(malloc(rect->bitmapSize));
    if (!rect->bitmap) {
        ALOGE("%s: malloc error %m", __func__);
        return;
    }

    auto j = 0;
    for (auto i = 0; i < rect->decodedRleSize; ++i) {
        // palette is uint32_t ARGB
        auto argb = rect->palette[rect->decodedRle[i]];
        rect->bitmap[j]     = (uint8_t)(argb & 0xFF);        // B
        rect->bitmap[j + 1] = (uint8_t)(argb >> 8 & 0xFF);   // G
        rect->bitmap[j + 2] = (uint8_t)(argb >> 16 & 0xFF);  // R
        rect->bitmap[j + 3] = (uint8_t)(argb >> 24 & 0xFF);  // A
        j += 4;
    }
}

void PgsParser::postDecodedItem(PGSSubContext& subContext, bool isImmediatePresent)
{
    if (mState == SUB_STOP) {
        return;
    }

    auto pts = subContext.presentation.pts;

    // End Display Segment has 0 object and Composition State is 0x00, it means
    // all objects should be cleared on the screen
    if (subContext.presentation.object_count < mPreviousObjectNum
        || subContext.presentation.object_count == 0) {
        for (auto i = 0; i < MAX_OBJECT_REFS; ++i) {
            int ptsMs = pts / DEFAULT_DVB_TIME_MULTI;
            int presentPtsMs = mPresentationTime / DEFAULT_DVB_TIME_MULTI;
            SUBTITLE_LOGI("%s_end_display: objectId=%d, pts=%" PRId64 "(%d ms)"
                          " diffWithPresentTime=%d ms"
                          " videoW%d, videoH%d, count %d(pre=%d) mPgsContextList=%zu",
                          __func__, i, pts, ptsMs, ptsMs - presentPtsMs,
                          subContext.presentation.videoWidth,
                          subContext.presentation.videoHeight,
                          subContext.presentation.object_count,
                          mPreviousObjectNum,
                          mPgsContextList.size());

            // 1. Make spu structure
            std::shared_ptr<AML_SPUVAR> spu = std::make_shared<AML_SPUVAR>();
            spu->subtitle_type = TYPE_SUBTITLE_PGS;
            spu->isImmediatePresent = isImmediatePresent;
            spu->spu_origin_display_w = subContext.presentation.videoWidth;
            spu->spu_origin_display_h = subContext.presentation.videoHeight;
            spu->pts = pts;
            spu->m_delay = pts;
            spu->objectSegmentId = i;
            spu->buffer_size = 0;

            if (mDumpSub) {
                // Dump a nullptr flag
                save2BitmapFile(nullptr, 0, 0, 0, spu->pts, spu->objectSegmentId, 0);
            }

            // 2. Post spu
            Parser::addDecodedItem(std::move(spu));
        }
        mPreviousObjectNum = subContext.presentation.object_count;
        if (subContext.presentation.object_count == 0) {
            return;
        }
    }

    mPreviousObjectNum = subContext.presentation.object_count;
    for (auto i = 0; i < subContext.presentation.object_count; ++i) {
        auto object = findObject(subContext.presentation.objects[i].id,
                                 &subContext.objects);
        if (!object) {
            SUBTITLE_LOGE("%s: Invalid objectId %d", __func__,
                          subContext.presentation.objects[i].id);
            continue;
        }

        auto rect = &subContext.presentation.present_sub_rect[i];
        if (!rect->is_valid) {
            // The error log was printed during parsing
            continue;
        }

        int ptsMs = pts / DEFAULT_DVB_TIME_MULTI;
        int presentPtsMs = mPresentationTime / DEFAULT_DVB_TIME_MULTI;
        SUBTITLE_LOGI("%s: index_%d/%d. objectId=%d, pts=%" PRId64 "(%d ms)"
                      " diffWithPresentTime=%d ms"
                      " bitmap_bytes=%d(w%4d*h%-3d*4), (x%-3d, y%-3d), videoW%d,"
                      " videoH%d, isValid=%d, mPgsContextList=%zu",
                      __func__, i+1, subContext.presentation.object_count,
                      rect->object_segment_id, pts, ptsMs, ptsMs - presentPtsMs,
                      rect->bitmapSize, rect->w, rect->h, rect->x, rect->y,
                      subContext.presentation.videoWidth,
                      subContext.presentation.videoHeight,
                      rect->is_valid, mPgsContextList.size());

        // 1. Make spu structure
        std::shared_ptr<AML_SPUVAR> spu = std::make_shared<AML_SPUVAR>();
        spu->subtitle_type = TYPE_SUBTITLE_PGS;
        spu->isImmediatePresent = isImmediatePresent;
        spu->spu_start_x = rect->x;
        spu->spu_start_y = rect->y;
        spu->spu_width = rect->w;
        spu->spu_height = rect->h;
        spu->spu_origin_display_w = subContext.presentation.videoWidth;
        spu->spu_origin_display_h = subContext.presentation.videoHeight;
        spu->pts = pts;
        spu->m_delay = pts + DEFAULT_DURATION_SECOND * 1000 * DEFAULT_DVB_TIME_MULTI;
        spu->objectSegmentId = rect->object_segment_id;
        spu->buffer_size = rect->bitmapSize;

        // Use malloc as architecture design, which will be freed in AML_SPUVAR.
        spu->useMalloc = true;
        spu->spu_data = rect->bitmap;

        if (mDumpSub && rect->bitmap) {
            save2BitmapFile(rect->bitmap, rect->bitmapSize, rect->w, rect->h,
                            spu->pts, spu->objectSegmentId,
                            subContext.presentation.objects[i].id);
        }

        // 2. Post spu
        Parser::addDecodedItem(spu);
        rect->bitmap = nullptr;
    }
}

void PgsParser::flushPgsContext()
{
    for (auto i = 0; i < mPgsContext.objects.count; ++i) {
        mPgsContext.objects.object[i].rle.clear();
        mPgsContext.objects.object[i].rle_buffer_size = 0;
        mPgsContext.objects.object[i].rle_remaining_len = 0;
    }
    mPgsContext.objects.count = 0;
    mPgsContext.palettes.count = 0;
}

PgsParser::PGSSubObject* PgsParser::findObject(int id, PGSSubObjects* objects)
{
    for (auto i = 0; i < objects->count; ++i) {
        if (objects->object[i].id == id) {
            return &objects->object[i];
        }
    }
    return nullptr;
}

PgsParser::PGSSubPalette* PgsParser::findPalette(int id, PGSSubPalettes* palettes)
{
    for (auto i = 0; i < palettes->count; ++i) {
        if (palettes->palette[i].id == id) {
            return &palettes->palette[i];
        }
    }
    return nullptr;
}

void PgsParser::save2BitmapFile(uint8_t* bitmap, int size, int w, int h,
                                int64_t pts, int objectSegmentId, int objectId)
{
    char filename[128];
    if (bitmap) {
        snprintf(filename, sizeof(filename),
                 "./data/subtitleDump/pgs_%" PRId64 "_%d_%d.ppm",
                 pts, objectSegmentId, objectId);
    }
    else {
        assert(size == 0);
        snprintf(filename, sizeof(filename),
                 "./data/subtitleDump/pgs_%" PRId64 "_%d_nullptr.ppm",
                 pts, objectSegmentId);
    }

    FILE* f = fopen(filename, "w");
    if (!f) {
        perror(filename);
        return;
    }

    // ppm is formatted as RGB
    fprintf(f, "P6\n" "%d %d\n" "%d\n", w, h, 255);
    for (auto i = 0; i < size;) {
        // bitmap is ARGB
        putc(bitmap[i+2], f); // R
        putc(bitmap[i+1], f); // G
        putc(bitmap[i+0], f); // B
        i += 4;
    }
    fclose(f);
}
