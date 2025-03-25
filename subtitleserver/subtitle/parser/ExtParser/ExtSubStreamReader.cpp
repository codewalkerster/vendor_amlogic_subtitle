/*
 * Copyright (C) 2014-2025 Amlogic, Inc. All rights reserved.
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

#define LOG_TAG "ExtSubStreamReader"

#include "ExtSubStreamReader.h"

namespace {

static inline void dump(const char* buf, int size) {
    char str[64] = {0};

    for (int i = 0; i < size; ++i) {
        char chars[6] = {0};
        sprintf(chars, "%02x ", buf[i]);
        strcat(str, chars);
        if (i % 8 == 7) {
            SUBTITLE_LOGI("%s", str);
            str[0] = str[1] = 0;
        }
    }
    SUBTITLE_LOGI("%s", str);
}

}; // namespace


// Public Functions
// ==========================
ExtSubStreamReader::ExtSubStreamReader(int charset, std::shared_ptr<DataSource> source) {
    mDataSource = source;
    mEncoding = charset;

    mDataSource->lseek(0, SEEK_SET);

    // Try detecting the file encode type if mEncoding is not assigned.
    if (mEncoding == AML_ENCODING_NONE) {
        detectEncoding();
    }
}

ExtSubStreamReader::~ExtSubStreamReader() {
    freeBuffer();
}

// TODO: it is dangerous to malloc here but free outside.
char* ExtSubStreamReader::strdup(char* src) {
    char *ret;
    int len;
    len = strlen(src);
    ret = (char *)MALLOC(len + 1);
    if (ret) {
        strcpy(ret, src);
    }
    return ret;
}

char* ExtSubStreamReader::strIStr(const char* haystack, const char* needle) {
    int len = 0;
    const char* p = haystack;
    if (!(haystack && needle)) {
        return NULL;
    }
    len = strlen(needle);
    while (*p != '\0') {
        if (strncasecmp(p, needle, len) == 0)
            return (char *)p;
        p++;
    }
    return NULL;
}

void ExtSubStreamReader::trimSpace(char* s) {
    int i = 0;
    int len = strlen(s) + 1;
    char* r = (char *)malloc(len);
    memset(r, 0, len);

    while (isspace(s[i])) {
        ++i;
    }

    strcpy(r, s + i);

    int k = strlen(r) - 1;
    while (k > 0 && isspace(r[k])) {
        r[k--] = '\0';
    }

    memcpy(s, r, len);  // Avoid strcpy memory overlap warning
    free(r);
}

void ExtSubStreamReader::backtoLastLine() {
    mBufferReadOffset =
        mBufferReadOffset > mLastLineLen ? mBufferReadOffset - mLastLineLen : 0;
}

// TODO: fix it, Not Thread safe
bool ExtSubStreamReader::rewindStream() {
    if (mDataSource != nullptr && mDataSource->lseek(0, SEEK_SET) > 0) {
        mBufferSize = 0;
        mBufferReadOffset = 0;
        mLastLineLen = 0;
        free(mBuffer);
        mBuffer = nullptr;
        return true;
    }
    return false;
}

bool ExtSubStreamReader::isEolCharacter(char c) {
    return (c == '\r' || c == '\n' || c == '\0');
}

char* ExtSubStreamReader::getLine(char* s) {
    if (mEncoding < AML_ENCODING_NONE || mEncoding > AML_ENCODING_UTF16BE) {
        SUBTITLE_LOGE("%s: unsupported mEncoding %d", __func__, mEncoding);
        return nullptr;
    }

    if (!mBuffer) {
        // Reserve two lines buffer because need combine the previous line's remain
        // size with the new line
        mBuffer = (char*)MALLOC(LINE_LEN * 2);
        if (mBuffer == nullptr) {
            SUBTITLE_LOGE("%s: %m", __func__);
            return nullptr;
        }
        mBufferReadOffset = 0;
        mLastLineLen = 0;
        mBufferSize = mDataSource->read(mBuffer, LINE_LEN);
    }

    if (mBufferSize <= 0) {
        SUBTITLE_LOGI("%s: no more data", __func__);
        return nullptr;
    }

    int offset = mBufferReadOffset;
    while (offset <= mBufferSize) {
        bool foundEol = false;

        // '\n' is the default LF(Line Feed)
        int lineBreakTagLen = 1;

        // step 1: try finding the End Of Line position
        if (mEncoding == AML_ENCODING_NONE || mEncoding == AML_ENCODING_UTF8) {
            if (offset < mBufferSize) {
                if (mBuffer[offset] == '\n' || mBuffer[offset] == '\0') {
                    foundEol = true;
                }
            }
        } else if (mEncoding == AML_ENCODING_UTF16BE) {
            // AML_ENCODING_UTF16BE is available from TV-35678
            lineBreakTagLen = 4; // '00 0d 00 0a'
            if (offset + lineBreakTagLen < mBufferSize) {
                if (mBuffer[offset] == 0x0
                    && mBuffer[offset+1] == 0xd
                    && mBuffer[offset+2] == 0x0
                    && mBuffer[offset+3] == 0xa) {
                    foundEol = true;
                }
            }
        } else if (mEncoding == AML_ENCODING_UTF16) {
            lineBreakTagLen = 4; // '0d 00 0a 0d'
            if (offset + lineBreakTagLen < mBufferSize) {
                 if (mBuffer[offset] == 0xd
                    && mBuffer[offset+1] == 0x0
                    && mBuffer[offset+2] == 0xa
                    && mBuffer[offset+3] == 0x0) {
                    foundEol = true;
                }
            }
        }

        // step 2: move to the next character if didn't find EOL
        if (!foundEol) {
            ++offset;
            if (offset < mBufferSize) {
                continue;
            }

            // step 2.1: Try reading more buffer if didn't find the EOL and buffer
            // is not enough to check
            if (mBufferSize - mBufferReadOffset >= LINE_LEN) {
                SUBTITLE_LOGE("%s: unsupport large line, the line size > %d",
                              __func__, LINE_LEN);
                freeBuffer();
                return nullptr;
            }

            auto remainBufferSize = mBufferSize - mBufferReadOffset;
            memmove(mBuffer, mBuffer + mBufferReadOffset, remainBufferSize);
            auto readSize = mDataSource->read(mBuffer + remainBufferSize, LINE_LEN);
            if (readSize <= 0) {
                SUBTITLE_LOGI("%s: reach end of file", __func__);
                if (readSize < 0) {
                    SUBTITLE_LOGE("%s: fail to read more data: %m", __func__);
                    return nullptr;
                }
                if (remainBufferSize > 0) {
                    MEMCPY(s, mBuffer, remainBufferSize);
                    freeBuffer();
                    return s;
                }
                return nullptr;
            }

            mBufferSize = remainBufferSize + readSize;
            mBufferReadOffset = 0;
            mLastLineLen = 0;
            offset = 0;
            continue;
        }

        // step 3: return line data if found
        auto dataLen = offset - mBufferReadOffset;
        MEMCPY(s, mBuffer + mBufferReadOffset, dataLen + 1);
        if (dataLen > 0) {
            convertToUtf8(mEncoding, s, dataLen);
        }

        s[dataLen] = '\0'; // Replace tag to end of string
        if (s[dataLen - 1] == '\r') {
            // Windows file formats EOL with CRLF(\r\n)
            s[dataLen - 1] = '\0';
        }
        mLastLineLen = dataLen + lineBreakTagLen;
        mBufferReadOffset = offset + lineBreakTagLen;
        return s;
    }

    SUBTITLE_LOGE("%s: something is wrong, offset=%d, mBufferSize=%d, mBufferReadOffset=%d",
                  __func__, offset, mBufferSize, mBufferReadOffset);
    return nullptr;
}

// Private Functions
// ==========================
void ExtSubStreamReader::detectEncoding() {
    if (mDataSource == nullptr) return;

    char header[3] = {0};
    mDataSource->lseek(0, SEEK_SET);
    auto ret = mDataSource->read(header, 3);
    if (ret < 3) {
        mDataSource->lseek(0, SEEK_SET);
        return;
    }

    // TODO: to check if AML_ENCODING_UTF32/AML_ENCODING_UTF32BE is really not used.

    if (header[0] == 0xFF && header[1] == 0xFE) {
        mEncoding = AML_ENCODING_UTF16;
        mDataSource->lseek(2, SEEK_SET);
    } else if (header[0] == 0xFE && header[1] == 0xFF) {
        // UTF16 Big Endian
        mEncoding = AML_ENCODING_UTF16BE;
        mDataSource->lseek(2, SEEK_SET);
    } else if (header[0] == 0xEF && header[1] == 0xBB && header[2] == 0xBF) {
        mEncoding = AML_ENCODING_UTF8;
        mDataSource->lseek(3, SEEK_SET);
    } else {
        // NO BOM, used default AML_ENCODING_NONE
        mDataSource->lseek(0, SEEK_SET);
    }
}

void ExtSubStreamReader::convertToUtf8(int charset, char* s, int inLen) {
    UTF8 *utf8Str = nullptr;
    int outLen = 0;

    if (charset == AML_ENCODING_UTF8 || charset == AML_ENCODING_NONE) {
        return;
    }

    utf8Str = (unsigned char *)malloc(inLen * 2 + 1);
    if (utf8Str == nullptr) {
        return;
    }
    memset(utf8Str, 0x0, inLen * 2 + 1);
    outLen = doConvertToUtf8(charset, (const UTF16 *)s, inLen / 2, utf8Str, inLen * 2);
    if (outLen > 0) {
        memcpy(s, utf8Str, outLen);
        s[outLen] = '\0';
    }

    free(utf8Str);
}

int ExtSubStreamReader::doConvertToUtf8(int charset, const UTF16* in,
                                        int inLen, UTF8* out, int outMax) {
    int outLen = 0;
    if (out) {
        // Output buffer passed in; actually encode data.
        while (inLen > 0) {
            UTF16 ch = *in;
            if (charset == AML_ENCODING_UTF16BE) {
                ch = ((ch << 8) & 0xFF00) | ch >> 8;
            }
            inLen--;
            if (ch < 0x80) {
                if (--outMax < 0) {
                    return -1;
                }
                *out++ = (UTF8) ch;
                outLen++;
            } else if (ch < 0x800) {
                if ((outMax -= 2) < 0) {
                    return -1;
                }
                *out++ = (UTF8)(0xC0 | ((ch >> 6) & 0x1F)); // 1
                *out++ = (UTF8)(0x80 | (ch & 0x3F));
                outLen += 2;
            } else if (ch >= 0xD800 && ch <= 0xDBFF) {
                UTF16 ch2;
                UTF32 ucs4;
                if (--inLen < 0) {
                    return -1;
                }
                ch2 = *++in;
                if (ch2 < 0xDC00 || ch2 > 0xDFFF) {
                    // This is an invalid UTF-16 surrogate pair sequence
                    // Encode the replacement character instead
                    ch = 0xFFFD;
                    goto Encode3;
                }
                ucs4 =
                    ((ch - 0xD800) << 10) + (ch2 - 0xDC00) +
                    0x10000;
                if ((outMax -= 4) < 0) {
                    return -1;
                }
                *out++ = (UTF8)(0xF0 | ((ucs4 >> 18) & 0x07)); // 2
                *out++ = (UTF8)(0x80 | ((ucs4 >> 12) & 0x3F));
                *out++ = (UTF8)(0x80 | ((ucs4 >> 6) & 0x3F));
                *out++ = (UTF8)(0x80 | (ucs4 & 0x3F));
                outLen += 4;
            } else {
                if (ch >= 0xDC00 && ch <= 0xDFFF) {
                    // This is an invalid UTF-16 surrogate pair sequence
                    // Encode the replacement character instead
                    ch = 0xFFFD;
                }
Encode3:
                if ((outMax -= 3) < 0) {
                    return -1;
                }
                *out++ = (UTF8)(0xE0 | ((ch >> 12) & 0x0F));
                *out++ = (UTF8)(0x80 | ((ch >> 6) & 0x3F));
                *out++ = (UTF8)(0x80 | (ch & 0x3F));
                outLen += 3;
            }
            in++;
        }
    } else {
        // Count output characters without actually encoding.
        while (inLen > 0) {
            UTF16 ch = *in, ch2;
            inLen--;
            if (ch < 0x80) {
                outLen++;
            } else if (ch < 0x800) {
                outLen += 2;
            } else if (ch >= 0xD800 && ch <= 0xDBFF) {
                if (--inLen < 0) {
                    return -1;
                }
                ch2 = *++in;
                if (ch2 < 0xDC00 || ch2 > 0xDFFF) {
                    // Invalid...
                    // We'll encode 0xFFFD for this
                    outLen += 3;
                } else {
                    outLen += 4;
                }
            } else {
                outLen += 3;
            }
            in++;
        }
    }
    return outLen;
}

void ExtSubStreamReader::freeBuffer() {
    if (mBuffer) {
        free(mBuffer);
        mBuffer = nullptr;
    }
    mBufferSize = 0;
    mBufferReadOffset = 0;
    mLastLineLen = 0;
}
