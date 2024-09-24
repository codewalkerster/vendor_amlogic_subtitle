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

#ifndef __SUBTITLE_PGS_PARSER_H__
#define __SUBTITLE_PGS_PARSER_H__

#include <cstdint>
#include <vector>

#include "Parser.h"
#include "DataSource.h"
#include "SubtitleTypes.h"

class PgsParser: public Parser {
public:
    explicit PgsParser(std::shared_ptr<DataSource> source);
    ~PgsParser();

    // Parser interfaces
    int parse() override;
    void dump(int fd, const char* prefix) override;

private:
    // Set enough delay time to wait for PGS End Segment.
    static const int DEFAULT_DURATION_SECOND = 30;
    static const int DEFAULT_DVB_TIME_MULTI  = 90;

    static const int MAX_EPOCH_PALETTES = 8;   // Max 8 allowed per PGS epoch
    static const int MAX_EPOCH_OBJECTS  = 64;  // Max 64 allowed per PGS epoch
    static const int MAX_OBJECT_REFS    = 2;   // Max objects per display set

    static const int OSD_HALF_SIZE = 1920*1280/8;

    // PGS segment handle sequence is: 1 PCS -> 2 WDS -> 3 PDS -> 4 ODS -> 5 END
    enum SegmentType {
        PALETTE_DEFINITION_SEGMENT       = 0x14,  // 3 PDS
        OBJECT_DEFINITION_SEGMENT        = 0x15,  // 4 ODS
        PRESENTATION_COMPOSITION_SEGMENT = 0x16,  // 1 PCS
        WINDOW_DEFINITION_SEGMENT        = 0x17,  // 2 WDS
        END_DISPLAY_SEGMENT              = 0x80,  // 5 END
    };

   // The subtitle rect to be presented
   // Tips: std::vector has a bad performace compare to malloc
    struct AVSubtitleRect {
        int x;
        int y;
        int w;
        int h;
        uint32_t palette[0x100];
        uint8_t* decodedRle = nullptr;
        int decodedRleSize = 0;
        uint8_t* bitmap = nullptr;
        int bitmapSize = 0;

        bool is_valid = false;
        bool is_forced_display = false;
        int object_segment_id;
    };

    struct PGSSubObjectRef {
        int     id;
        int     window_id;
        uint8_t composition_flag;
        int     x;
        int     y;
        int     crop_x;
        int     crop_y;
        int     crop_w;
        int     crop_h;
    };

    struct PGSSubPresentation {
        int videoWidth;
        int videoHeight;
        int id_number;
        int palette_id;
        int object_count;
        PGSSubObjectRef objects[MAX_OBJECT_REFS];
        int64_t pts;

        // The rect to be presented
        AVSubtitleRect present_sub_rect[MAX_OBJECT_REFS];
    };

    struct PGSSubObject {
        int          id;
        int          w;
        int          h;
        std::vector<uint8_t> rle;
        unsigned int rle_buffer_size, rle_data_len;
        unsigned int rle_remaining_len;
    };

    struct PGSSubObjects {
        int          count;
        PGSSubObject object[MAX_EPOCH_OBJECTS];
    };

    struct PGSSubPalette {
        int         id;
        uint32_t    argb[0x100];
    };

    struct PGSSubPalettes {
        int           count;
        PGSSubPalette palette[MAX_EPOCH_PALETTES];
    };

    struct PGSSubContext {
        PGSSubPresentation presentation;
        PGSSubPalettes     palettes;
        PGSSubObjects      objects;
    };

    PGSSubContext mPgsContext;
    int mPreviousObjectNum = 0;
    bool mDumpSub = false;
    std::string mDecodeSequenceTracker = "";
    int mFirstPtsMs = 0;
    uint64_t mCurrentTimeMs = 0;

    void checkDebug();
    int readDataSource();
    void softDemuxParser();
    void hwDemuxParser();
    void decode(const std::vector<uint8_t>& pgsPacket, int duration);

    // 1 PCS
    void parsePresentationSegment(const uint8_t* buf, int buf_size);
    // 2 WDS is ignored
    // 3 PDS
    void parsePaletteSegment(const uint8_t* buf, int buf_size);
    // 4 ODS
    int parseObjectSegment(const uint8_t* buf, int buf_size);
    // 5 END
    void handleDisplayEndSegment();

    bool decodeRle(AVSubtitleRect* rect, const std::vector<uint8_t>& buf);
    void renderRle2Bitmap(AVSubtitleRect* rect);
    void postDecodedItem(int duration);

    void flushPgsContext();
    PGSSubObject* findObject(int id, PGSSubObjects* objects);
    PGSSubPalette* findPalette(int id, PGSSubPalettes* palettes);
    void save2BitmapFile(uint8_t* bitmap, int size, int w, int h,
                         int64_t pts, int objectSegmentId, int objectId);
};

#endif // __SUBTITLE_PGS_PARSER_H__
