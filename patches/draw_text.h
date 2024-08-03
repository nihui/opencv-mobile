//
// Copyright (C) 2023 nihui
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <ctype.h>
#include <limits.h>
#include <math.h>

#include <algorithm>

#if __ARM_NEON
#include <arm_neon.h>
#endif // __ARM_NEON

#if __SSE2__
#include <emmintrin.h>
#endif

#include "mono_font_data.h"

namespace cv
{

static int utf8_to_unicode(const char* utf8, std::vector<unsigned int>& unicode)
{
    const int n = strlen(utf8);

    int i = 0;
    while (i < n)
    {
        unsigned char ch = utf8[i++];

        unsigned int uni = 0;
        int todo = 0;
        if (ch <= 0x7F)
        {
            uni = ch;
        }
        else if (ch <= 0xBF)
        {
            return -1;
        }
        else if (ch <= 0xDF)
        {
            uni = ch & 0x1F;
            todo = 1;
        }
        else if (ch <= 0xEF)
        {
            uni = ch & 0x0F;
            todo = 2;
        }
        else if (ch <= 0xF7)
        {
            uni = ch & 0x07;
            todo = 3;
        }

        for (int j = 0; j < todo; j++)
        {
            if (i == n)
                return -1;

            unsigned char ch2 = utf8[i++];

            if (ch2 < 0x80 || ch2 > 0xBF)
                return -1;

            uni <<= 6;
            uni += ch2 & 0x3F;
        }
        if (uni >= 0xD800 && uni <= 0xDFFF)
            return -1;
        if (uni > 0x10FFFF)
            return -1;

        unicode.push_back(uni);
    }

    return 0;
}

static void get_text_drawing_size(const char* text, int fontpixelsize, int* w, int* h, const FontFace& ff = FontFace())
{
    *w = 0;
    *h = 0;

    std::vector<unsigned int> unicode;
    utf8_to_unicode(text, unicode);

    const int n = (int)unicode.size();

    int line_w = 0;
    for (int i = 0; i < n; i++)
    {
        unsigned int ch = unicode[i];

        if (ch == '\n')
        {
            // newline
            *w = std::max(*w, line_w);
            *h += fontpixelsize * 2;
            line_w = 0;
        }

        if (ch < 128 && isprint(ch) != 0)
        {
            line_w += fontpixelsize;
        }
        else if (ff.d->get_glyph_bitmap(ch))
        {
            line_w += fontpixelsize * 2;
        }
    }

    *w = std::max(*w, line_w);
    *h += fontpixelsize * 2;
}

static void resize_bilinear_font(const unsigned char* font_bitmap, unsigned char* resized_font_bitmap, int fontpixelsize, int fullwidth = 0)
{
    const int INTER_RESIZE_COEF_BITS = 11;
    const int INTER_RESIZE_COEF_SCALE = 1 << INTER_RESIZE_COEF_BITS;

    const int srcw = fullwidth ? 40 : 20;
    const int srch = 40;
    const int w = fullwidth ? fontpixelsize * 2 : fontpixelsize;
    const int h = fontpixelsize * 2;

    double scale = (double)srcw / w;

    int* buf = new int[w + h + w + h];

    int* xofs = buf;     //new int[w];
    int* yofs = buf + w; //new int[h];

    short* ialpha = (short*)(buf + w + h);    //new short[w * 2];
    short* ibeta = (short*)(buf + w + h + w); //new short[h * 2];

    float fx;
    float fy;
    int sx;
    int sy;

#define SATURATE_CAST_SHORT(X) (short)::std::min(::std::max((int)(X + (X >= 0.f ? 0.5f : -0.5f)), SHRT_MIN), SHRT_MAX);

    for (int dx = 0; dx < w; dx++)
    {
        fx = (float)((dx + 0.5) * scale - 0.5);
        sx = static_cast<int>(floor(fx));
        fx -= sx;

        xofs[dx] = sx;

        float a0 = (1.f - fx) * INTER_RESIZE_COEF_SCALE;
        float a1 = fx * INTER_RESIZE_COEF_SCALE;

        ialpha[dx * 2] = SATURATE_CAST_SHORT(a0);
        ialpha[dx * 2 + 1] = SATURATE_CAST_SHORT(a1);
    }

    for (int dy = 0; dy < h; dy++)
    {
        fy = (float)((dy + 0.5) * scale - 0.5);
        sy = static_cast<int>(floor(fy));
        fy -= sy;

        yofs[dy] = sy;

        float b0 = (1.f - fy) * INTER_RESIZE_COEF_SCALE;
        float b1 = fy * INTER_RESIZE_COEF_SCALE;

        ibeta[dy * 2] = SATURATE_CAST_SHORT(b0);
        ibeta[dy * 2 + 1] = SATURATE_CAST_SHORT(b1);
    }

#undef SATURATE_CAST_SHORT

    // loop body
    std::vector<short> rowsbuf0(w);
    std::vector<short> rowsbuf1(w);
    short* rows0 = (short*)rowsbuf0.data();
    short* rows1 = (short*)rowsbuf1.data();

    {
        short* rows1p = rows1;
        for (int dx = 0; dx < w; dx++)
        {
            rows1p[dx] = 0;
        }
    }

    int prev_sy1 = -2;

    for (int dy = 0; dy < h; dy++)
    {
        sy = yofs[dy];

        if (sy == prev_sy1)
        {
            // reuse all rows
        }
        else if (sy == prev_sy1 + 1)
        {
            // hresize one row
            short* rows0_old = rows0;
            rows0 = rows1;
            rows1 = rows0_old;
            const unsigned char* S1 = font_bitmap + (fullwidth ? 20 : 10) * (sy + 1);

            if (sy >= srch - 1)
            {
                short* rows1p = rows1;
                for (int dx = 0; dx < w; dx++)
                {
                    rows1p[dx] = 0;
                }
            }
            else
            {
                const short* ialphap = ialpha;
                short* rows1p = rows1;
                for (int dx = 0; dx < w; dx++)
                {
                    sx = xofs[dx];
                    short a0 = ialphap[0];
                    short a1 = ialphap[1];

                    unsigned char S1p0;
                    unsigned char S1p1;

                    if (sx < 0)
                    {
                        S1p0 = 0;
                        S1p1 = S1[0] & 0x0f;
                    }
                    else if (sx >= srcw - 1)
                    {
                        S1p0 = (S1[fullwidth ? 19 : 9] & 0xf0) >> 4;
                        S1p1 = 0;
                    }
                    else
                    {
                        S1p0 = sx % 2 == 0 ? S1[sx / 2] & 0x0f : (S1[sx / 2] & 0xf0) >> 4;
                        S1p1 = sx % 2 == 0 ? (S1[sx / 2] & 0xf0) >> 4 : S1[sx / 2 + 1] & 0x0f;
                    }
                    rows1p[dx] = (S1p0 * a0 + S1p1 * a1) * 17 >> 4;

                    ialphap += 2;
                }
            }
        }
        else
        {
            // hresize two rows
            const unsigned char* S0 = font_bitmap + (fullwidth ? 20 : 10) * (sy);
            const unsigned char* S1 = font_bitmap + (fullwidth ? 20 : 10) * (sy + 1);

            if (sy >= srch - 1)
            {
                const short* ialphap = ialpha;
                short* rows0p = rows0;
                short* rows1p = rows1;
                for (int dx = 0; dx < w; dx++)
                {
                    sx = xofs[dx];
                    short a0 = ialphap[0];
                    short a1 = ialphap[1];

                    unsigned char S0p0;
                    unsigned char S0p1;

                    if (sx < 0)
                    {
                        S0p0 = 0;
                        S0p1 = S0[0] & 0x0f;
                    }
                    else if (sx >= srcw - 1)
                    {
                        S0p0 = (S0[fullwidth ? 19 : 9] & 0xf0) >> 4;
                        S0p1 = 0;
                    }
                    else
                    {
                        S0p0 = sx % 2 == 0 ? S0[sx / 2] & 0x0f : (S0[sx / 2] & 0xf0) >> 4;
                        S0p1 = sx % 2 == 0 ? (S0[sx / 2] & 0xf0) >> 4 : S0[sx / 2 + 1] & 0x0f;
                    }
                    rows0p[dx] = (S0p0 * a0 + S0p1 * a1) * 17 >> 4;
                    rows1p[dx] = 0;

                    ialphap += 2;
                }
            }
            else
            {
                const short* ialphap = ialpha;
                short* rows0p = rows0;
                short* rows1p = rows1;
                for (int dx = 0; dx < w; dx++)
                {
                    sx = xofs[dx];
                    short a0 = ialphap[0];
                    short a1 = ialphap[1];

                    unsigned char S0p0;
                    unsigned char S0p1;
                    unsigned char S1p0;
                    unsigned char S1p1;

                    if (sx < 0)
                    {
                        S0p0 = 0;
                        S0p1 = S0[0] & 0x0f;
                        S1p0 = 0;
                        S1p1 = S1[0] & 0x0f;
                    }
                    else if (sx >= srcw - 1)
                    {
                        S0p0 = (S0[fullwidth ? 19 : 9] & 0xf0) >> 4;
                        S0p1 = 0;
                        S1p0 = (S1[fullwidth ? 19 : 9] & 0xf0) >> 4;
                        S1p1 = 0;
                    }
                    else
                    {
                        S0p0 = sx % 2 == 0 ? S0[sx / 2] & 0x0f : (S0[sx / 2] & 0xf0) >> 4;
                        S0p1 = sx % 2 == 0 ? (S0[sx / 2] & 0xf0) >> 4 : S0[sx / 2 + 1] & 0x0f;
                        S1p0 = sx % 2 == 0 ? S1[sx / 2] & 0x0f : (S1[sx / 2] & 0xf0) >> 4;
                        S1p1 = sx % 2 == 0 ? (S1[sx / 2] & 0xf0) >> 4 : S1[sx / 2 + 1] & 0x0f;
                    }
                    rows0p[dx] = (S0p0 * a0 + S0p1 * a1) * 17 >> 4;
                    rows1p[dx] = (S1p0 * a0 + S1p1 * a1) * 17 >> 4;

                    ialphap += 2;
                }
            }
        }

        prev_sy1 = sy;

        if (dy + 1 < h && yofs[dy + 1] == sy)
        {
            // vresize for two rows
            short b0 = ibeta[0];
            short b1 = ibeta[1];
            short b2 = ibeta[2];
            short b3 = ibeta[3];

            short* rows0p = rows0;
            short* rows1p = rows1;
            unsigned char* Dp0 = resized_font_bitmap + w * (dy);
            unsigned char* Dp1 = resized_font_bitmap + w * (dy + 1);

            int dx = 0;
#if __ARM_NEON
            int16x8_t _b0 = vdupq_n_s16(b0);
            int16x8_t _b1 = vdupq_n_s16(b1);
            int16x8_t _b2 = vdupq_n_s16(b2);
            int16x8_t _b3 = vdupq_n_s16(b3);
            for (; dx + 15 < w; dx += 16)
            {
                int16x8_t _r00 = vld1q_s16(rows0p);
                int16x8_t _r01 = vld1q_s16(rows0p + 8);
                int16x8_t _r10 = vld1q_s16(rows1p);
                int16x8_t _r11 = vld1q_s16(rows1p + 8);
                int16x8_t _acc00 = vaddq_s16(vqdmulhq_s16(_r00, _b0), vqdmulhq_s16(_r10, _b1));
                int16x8_t _acc01 = vaddq_s16(vqdmulhq_s16(_r01, _b0), vqdmulhq_s16(_r11, _b1));
                int16x8_t _acc10 = vaddq_s16(vqdmulhq_s16(_r00, _b2), vqdmulhq_s16(_r10, _b3));
                int16x8_t _acc11 = vaddq_s16(vqdmulhq_s16(_r01, _b2), vqdmulhq_s16(_r11, _b3));
                uint8x16_t _Dp0 = vcombine_u8(vqrshrun_n_s16(_acc00, 3), vqrshrun_n_s16(_acc01, 3));
                uint8x16_t _Dp1 = vcombine_u8(vqrshrun_n_s16(_acc10, 3), vqrshrun_n_s16(_acc11, 3));
                vst1q_u8(Dp0, _Dp0);
                vst1q_u8(Dp1, _Dp1);
                Dp0 += 16;
                Dp1 += 16;
                rows0p += 16;
                rows1p += 16;
            }
            for (; dx + 7 < w; dx += 8)
            {
                int16x8_t _r0 = vld1q_s16(rows0p);
                int16x8_t _r1 = vld1q_s16(rows1p);
                int16x8_t _acc0 = vaddq_s16(vqdmulhq_s16(_r0, _b0), vqdmulhq_s16(_r1, _b1));
                int16x8_t _acc1 = vaddq_s16(vqdmulhq_s16(_r0, _b2), vqdmulhq_s16(_r1, _b3));
                uint8x8_t _Dp0 = vqrshrun_n_s16(_acc0, 3);
                uint8x8_t _Dp1 = vqrshrun_n_s16(_acc1, 3);
                vst1_u8(Dp0, _Dp0);
                vst1_u8(Dp1, _Dp1);
                Dp0 += 8;
                Dp1 += 8;
                rows0p += 8;
                rows1p += 8;
            }
#endif // __ARM_NEON
#if __SSE2__
            __m128i _b0 = _mm_set1_epi16(b0);
            __m128i _b1 = _mm_set1_epi16(b1);
            __m128i _b2 = _mm_set1_epi16(b2);
            __m128i _b3 = _mm_set1_epi16(b3);
            __m128i _v2 = _mm_set1_epi16(2);
            for (; dx + 15 < w; dx += 16)
            {
                __m128i _r00 = _mm_loadu_si128((const __m128i*)rows0p);
                __m128i _r01 = _mm_loadu_si128((const __m128i*)(rows0p + 8));
                __m128i _r10 = _mm_loadu_si128((const __m128i*)rows1p);
                __m128i _r11 = _mm_loadu_si128((const __m128i*)(rows1p + 8));
                __m128i _acc00 = _mm_add_epi16(_mm_mulhi_epi16(_r00, _b0), _mm_mulhi_epi16(_r10, _b1));
                __m128i _acc01 = _mm_add_epi16(_mm_mulhi_epi16(_r01, _b0), _mm_mulhi_epi16(_r11, _b1));
                __m128i _acc10 = _mm_add_epi16(_mm_mulhi_epi16(_r00, _b2), _mm_mulhi_epi16(_r10, _b3));
                __m128i _acc11 = _mm_add_epi16(_mm_mulhi_epi16(_r01, _b2), _mm_mulhi_epi16(_r11, _b3));
                _acc00 = _mm_srai_epi16(_mm_add_epi16(_acc00, _v2), 2);
                _acc01 = _mm_srai_epi16(_mm_add_epi16(_acc01, _v2), 2);
                _acc10 = _mm_srai_epi16(_mm_add_epi16(_acc10, _v2), 2);
                _acc11 = _mm_srai_epi16(_mm_add_epi16(_acc11, _v2), 2);
                __m128i _Dp0 = _mm_packus_epi16(_acc00, _acc01);
                __m128i _Dp1 = _mm_packus_epi16(_acc10, _acc11);
                _mm_storeu_si128((__m128i*)Dp0, _Dp0);
                _mm_storeu_si128((__m128i*)Dp1, _Dp1);
                Dp0 += 16;
                Dp1 += 16;
                rows0p += 16;
                rows1p += 16;
            }
            for (; dx + 7 < w; dx += 8)
            {
                __m128i _r0 = _mm_loadu_si128((const __m128i*)rows0p);
                __m128i _r1 = _mm_loadu_si128((const __m128i*)rows1p);
                __m128i _acc0 = _mm_add_epi16(_mm_mulhi_epi16(_r0, _b0), _mm_mulhi_epi16(_r1, _b1));
                __m128i _acc1 = _mm_add_epi16(_mm_mulhi_epi16(_r0, _b2), _mm_mulhi_epi16(_r1, _b3));
                _acc0 = _mm_srai_epi16(_mm_add_epi16(_acc0, _v2), 2);
                _acc1 = _mm_srai_epi16(_mm_add_epi16(_acc1, _v2), 2);
                __m128i _Dp0 = _mm_packus_epi16(_acc0, _acc0);
                __m128i _Dp1 = _mm_packus_epi16(_acc1, _acc1);
                _mm_storel_epi64((__m128i*)Dp0, _Dp0);
                _mm_storel_epi64((__m128i*)Dp1, _Dp1);
                Dp0 += 8;
                Dp1 += 8;
                rows0p += 8;
                rows1p += 8;
            }
#endif // __SSE2__
            for (; dx < w; dx++)
            {
                short s0 = *rows0p++;
                short s1 = *rows1p++;

                *Dp0++ = (unsigned char)(((short)((b0 * s0) >> 16) + (short)((b1 * s1) >> 16) + 2) >> 2);
                *Dp1++ = (unsigned char)(((short)((b2 * s0) >> 16) + (short)((b3 * s1) >> 16) + 2) >> 2);
            }

            ibeta += 4;
            dy += 1;
        }
        else
        {
            // vresize
            short b0 = ibeta[0];
            short b1 = ibeta[1];

            short* rows0p = rows0;
            short* rows1p = rows1;
            unsigned char* Dp = resized_font_bitmap + w * (dy);

            int dx = 0;
#if __ARM_NEON
            int16x8_t _b0 = vdupq_n_s16(b0);
            int16x8_t _b1 = vdupq_n_s16(b1);
            for (; dx + 15 < w; dx += 16)
            {
                int16x8_t _r00 = vld1q_s16(rows0p);
                int16x8_t _r01 = vld1q_s16(rows0p + 8);
                int16x8_t _r10 = vld1q_s16(rows1p);
                int16x8_t _r11 = vld1q_s16(rows1p + 8);
                int16x8_t _acc0 = vaddq_s16(vqdmulhq_s16(_r00, _b0), vqdmulhq_s16(_r10, _b1));
                int16x8_t _acc1 = vaddq_s16(vqdmulhq_s16(_r01, _b0), vqdmulhq_s16(_r11, _b1));
                uint8x16_t _Dp = vcombine_u8(vqrshrun_n_s16(_acc0, 3), vqrshrun_n_s16(_acc1, 3));
                vst1q_u8(Dp, _Dp);
                Dp += 16;
                rows0p += 16;
                rows1p += 16;
            }
            for (; dx + 7 < w; dx += 8)
            {
                int16x8_t _r0 = vld1q_s16(rows0p);
                int16x8_t _r1 = vld1q_s16(rows1p);
                int16x8_t _acc = vaddq_s16(vqdmulhq_s16(_r0, _b0), vqdmulhq_s16(_r1, _b1));
                uint8x8_t _Dp = vqrshrun_n_s16(_acc, 3);
                vst1_u8(Dp, _Dp);
                Dp += 8;
                rows0p += 8;
                rows1p += 8;
            }
#endif // __ARM_NEON
#if __SSE2__
            __m128i _b0 = _mm_set1_epi16(b0);
            __m128i _b1 = _mm_set1_epi16(b1);
            __m128i _v2 = _mm_set1_epi16(2);
            for (; dx + 15 < w; dx += 16)
            {
                __m128i _r00 = _mm_loadu_si128((const __m128i*)rows0p);
                __m128i _r01 = _mm_loadu_si128((const __m128i*)(rows0p + 8));
                __m128i _r10 = _mm_loadu_si128((const __m128i*)rows1p);
                __m128i _r11 = _mm_loadu_si128((const __m128i*)(rows1p + 8));
                __m128i _acc0 = _mm_add_epi16(_mm_mulhi_epi16(_r00, _b0), _mm_mulhi_epi16(_r10, _b1));
                __m128i _acc1 = _mm_add_epi16(_mm_mulhi_epi16(_r01, _b0), _mm_mulhi_epi16(_r11, _b1));
                _acc0 = _mm_srai_epi16(_mm_add_epi16(_acc0, _v2), 2);
                _acc1 = _mm_srai_epi16(_mm_add_epi16(_acc1, _v2), 2);
                __m128i _Dp = _mm_packus_epi16(_acc0, _acc1);
                _mm_storeu_si128((__m128i*)Dp, _Dp);
                Dp += 16;
                rows0p += 16;
                rows1p += 16;
            }
            for (; dx + 7 < w; dx += 8)
            {
                __m128i _r0 = _mm_loadu_si128((const __m128i*)rows0p);
                __m128i _r1 = _mm_loadu_si128((const __m128i*)rows1p);
                __m128i _acc = _mm_add_epi16(_mm_mulhi_epi16(_r0, _b0), _mm_mulhi_epi16(_r1, _b1));
                _acc = _mm_srai_epi16(_mm_add_epi16(_acc, _v2), 2);
                __m128i _Dp = _mm_packus_epi16(_acc, _acc);
                _mm_storel_epi64((__m128i*)Dp, _Dp);
                Dp += 8;
                rows0p += 8;
                rows1p += 8;
            }
#endif // __SSE2__
            for (; dx < w; dx++)
            {
                short s0 = *rows0p++;
                short s1 = *rows1p++;

                *Dp++ = (unsigned char)(((short)((b0 * s0) >> 16) + (short)((b1 * s1) >> 16) + 2) >> 2);
            }

            ibeta += 2;
        }
    }

    delete[] buf;
}

static void draw_text_c1(unsigned char* pixels, int w, int h, int stride, const char* text, int x, int y, int fontpixelsize, unsigned int color, const FontFace& ff)
{
    const unsigned char* pen_color = (const unsigned char*)&color;

    unsigned char* resized_font_bitmap = new unsigned char[fontpixelsize * fontpixelsize * 4];

    std::vector<unsigned int> unicode;
    utf8_to_unicode(text, unicode);

    const int n = (int)unicode.size();

    int cursor_x = x;
    int cursor_y = y;
    for (int i = 0; i < n; i++)
    {
        unsigned int ch = unicode[i];

        if (ch == '\n')
        {
            // newline
            cursor_x = x;
            cursor_y += fontpixelsize * 2;
            continue;
        }

        if (ch == ' ')
        {
            cursor_x += fontpixelsize;
            continue;
        }

        if (ch < 128 && isprint(ch) != 0)
        {
            const unsigned char* font_bitmap = mono_font_data[ch - '!'];

            // draw resized character
            resize_bilinear_font(font_bitmap, resized_font_bitmap, fontpixelsize);

            const int ystart = std::max(cursor_y, 0);
            const int yend = std::min(cursor_y + fontpixelsize * 2, h);
            const int xstart = std::max(cursor_x, 0);
            const int xend = std::min(cursor_x + fontpixelsize, w);

            for (int j = ystart; j < yend; j++)
            {
                const unsigned char* palpha = resized_font_bitmap + (j - cursor_y) * fontpixelsize + xstart - cursor_x;
                unsigned char* p = pixels + stride * j + xstart;

                for (int k = xstart; k < xend; k++)
                {
                    unsigned char alpha = *palpha++;

                    p[0] = (p[0] * (255 - alpha) + pen_color[0] * alpha) / 255;
                    p += 1;
                }
            }

            cursor_x += fontpixelsize;
        }
        else
        {
            const unsigned char* font_bitmap = ff.d->get_glyph_bitmap(ch);
            if (font_bitmap)
            {
                // draw resized character
                resize_bilinear_font(font_bitmap, resized_font_bitmap, fontpixelsize, 1);

                const int ystart = std::max(cursor_y, 0);
                const int yend = std::min(cursor_y + fontpixelsize * 2, h);
                const int xstart = std::max(cursor_x, 0);
                const int xend = std::min(cursor_x + fontpixelsize * 2, w);

                for (int j = ystart; j < yend; j++)
                {
                    const unsigned char* palpha = resized_font_bitmap + (j - cursor_y) * fontpixelsize * 2 + xstart - cursor_x;
                    unsigned char* p = pixels + stride * j + xstart;

                    for (int k = xstart; k < xend; k++)
                    {
                        unsigned char alpha = *palpha++;

                        p[0] = (p[0] * (255 - alpha) + pen_color[0] * alpha) / 255;
                        p += 1;
                    }
                }

                cursor_x += fontpixelsize * 2;
            }
        }
    }

    delete[] resized_font_bitmap;
}

static void draw_text_c3(unsigned char* pixels, int w, int h, int stride, const char* text, int x, int y, int fontpixelsize, unsigned int color, const FontFace& ff)
{
    const unsigned char* pen_color = (const unsigned char*)&color;

    unsigned char* resized_font_bitmap = new unsigned char[fontpixelsize * fontpixelsize * 4];

    std::vector<unsigned int> unicode;
    utf8_to_unicode(text, unicode);

    const int n = (int)unicode.size();

    int cursor_x = x;
    int cursor_y = y;
    for (int i = 0; i < n; i++)
    {
        unsigned int ch = unicode[i];

        if (ch == '\n')
        {
            // newline
            cursor_x = x;
            cursor_y += fontpixelsize * 2;
            continue;
        }

        if (ch == ' ')
        {
            cursor_x += fontpixelsize;
            continue;
        }

        if (ch < 128 && isprint(ch) != 0)
        {
            const unsigned char* font_bitmap = mono_font_data[ch - '!'];

            // draw resized character
            resize_bilinear_font(font_bitmap, resized_font_bitmap, fontpixelsize);

            const int ystart = std::max(cursor_y, 0);
            const int yend = std::min(cursor_y + fontpixelsize * 2, h);
            const int xstart = std::max(cursor_x, 0);
            const int xend = std::min(cursor_x + fontpixelsize, w);

            for (int j = ystart; j < yend; j++)
            {
                const unsigned char* palpha = resized_font_bitmap + (j - cursor_y) * fontpixelsize + xstart - cursor_x;
                unsigned char* p = pixels + stride * j + xstart * 3;

                for (int k = xstart; k < xend; k++)
                {
                    unsigned char alpha = *palpha++;

                    p[0] = (p[0] * (255 - alpha) + pen_color[0] * alpha) / 255;
                    p[1] = (p[1] * (255 - alpha) + pen_color[1] * alpha) / 255;
                    p[2] = (p[2] * (255 - alpha) + pen_color[2] * alpha) / 255;
                    p += 3;
                }
            }

            cursor_x += fontpixelsize;
        }
        else
        {
            const unsigned char* font_bitmap = ff.d->get_glyph_bitmap(ch);
            if (font_bitmap)
            {
                // draw resized character
                resize_bilinear_font(font_bitmap, resized_font_bitmap, fontpixelsize, 1);

                const int ystart = std::max(cursor_y, 0);
                const int yend = std::min(cursor_y + fontpixelsize * 2, h);
                const int xstart = std::max(cursor_x, 0);
                const int xend = std::min(cursor_x + fontpixelsize * 2, w);

                for (int j = ystart; j < yend; j++)
                {
                    const unsigned char* palpha = resized_font_bitmap + (j - cursor_y) * fontpixelsize * 2 + xstart - cursor_x;
                    unsigned char* p = pixels + stride * j + xstart * 3;

                    for (int k = xstart; k < xend; k++)
                    {
                        unsigned char alpha = *palpha++;

                        p[0] = (p[0] * (255 - alpha) + pen_color[0] * alpha) / 255;
                        p[1] = (p[1] * (255 - alpha) + pen_color[1] * alpha) / 255;
                        p[2] = (p[2] * (255 - alpha) + pen_color[2] * alpha) / 255;
                        p += 3;
                    }
                }

                cursor_x += fontpixelsize * 2;
            }
        }
    }

    delete[] resized_font_bitmap;
}

static void draw_text_c4(unsigned char* pixels, int w, int h, int stride, const char* text, int x, int y, int fontpixelsize, unsigned int color, const FontFace& ff)
{
    const unsigned char* pen_color = (const unsigned char*)&color;

    unsigned char* resized_font_bitmap = new unsigned char[fontpixelsize * fontpixelsize * 4];

    std::vector<unsigned int> unicode;
    utf8_to_unicode(text, unicode);

    const int n = (int)unicode.size();

    int cursor_x = x;
    int cursor_y = y;
    for (int i = 0; i < n; i++)
    {
        unsigned int ch = unicode[i];

        if (ch == '\n')
        {
            // newline
            cursor_x = x;
            cursor_y += fontpixelsize * 2;
            continue;
        }

        if (ch == ' ')
        {
            cursor_x += fontpixelsize;
            continue;
        }

        if (ch < 128 && isprint(ch) != 0)
        {
            const unsigned char* font_bitmap = mono_font_data[ch - '!'];

            // draw resized character
            resize_bilinear_font(font_bitmap, resized_font_bitmap, fontpixelsize);

            const int ystart = std::max(cursor_y, 0);
            const int yend = std::min(cursor_y + fontpixelsize * 2, h);
            const int xstart = std::max(cursor_x, 0);
            const int xend = std::min(cursor_x + fontpixelsize, w);

            for (int j = ystart; j < yend; j++)
            {
                const unsigned char* palpha = resized_font_bitmap + (j - cursor_y) * fontpixelsize + xstart - cursor_x;
                unsigned char* p = pixels + stride * j + xstart * 4;

                for (int k = xstart; k < xend; k++)
                {
                    unsigned char alpha = *palpha++;

                    p[0] = (p[0] * (255 - alpha) + pen_color[0] * alpha) / 255;
                    p[1] = (p[1] * (255 - alpha) + pen_color[1] * alpha) / 255;
                    p[2] = (p[2] * (255 - alpha) + pen_color[2] * alpha) / 255;
                    p[3] = (p[3] * (255 - alpha) + pen_color[3] * alpha) / 255;
                    p += 4;
                }
            }

            cursor_x += fontpixelsize;
        }
        else
        {
            const unsigned char* font_bitmap = ff.d->get_glyph_bitmap(ch);
            if (font_bitmap)
            {
                // draw resized character
                resize_bilinear_font(font_bitmap, resized_font_bitmap, fontpixelsize, 1);

                const int ystart = std::max(cursor_y, 0);
                const int yend = std::min(cursor_y + fontpixelsize * 2, h);
                const int xstart = std::max(cursor_x, 0);
                const int xend = std::min(cursor_x + fontpixelsize * 2, w);

                for (int j = ystart; j < yend; j++)
                {
                    const unsigned char* palpha = resized_font_bitmap + (j - cursor_y) * fontpixelsize * 2 + xstart - cursor_x;
                    unsigned char* p = pixels + stride * j + xstart * 4;

                    for (int k = xstart; k < xend; k++)
                    {
                        unsigned char alpha = *palpha++;

                        p[0] = (p[0] * (255 - alpha) + pen_color[0] * alpha) / 255;
                        p[1] = (p[1] * (255 - alpha) + pen_color[1] * alpha) / 255;
                        p[2] = (p[2] * (255 - alpha) + pen_color[2] * alpha) / 255;
                        p[3] = (p[3] * (255 - alpha) + pen_color[3] * alpha) / 255;
                        p += 4;
                    }
                }

                cursor_x += fontpixelsize * 2;
            }
        }
    }

    delete[] resized_font_bitmap;
}

static void draw_text_c1(unsigned char* pixels, int w, int h, const char* text, int x, int y, int fontpixelsize, unsigned int color, const FontFace& ff = FontFace())
{
    return draw_text_c1(pixels, w, h, w, text, x, y, fontpixelsize, color, ff);
}

static void draw_text_c3(unsigned char* pixels, int w, int h, const char* text, int x, int y, int fontpixelsize, unsigned int color, const FontFace& ff = FontFace())
{
    return draw_text_c3(pixels, w, h, w * 3, text, x, y, fontpixelsize, color, ff);
}

static void draw_text_c4(unsigned char* pixels, int w, int h, const char* text, int x, int y, int fontpixelsize, unsigned int color, const FontFace& ff = FontFace())
{
    return draw_text_c4(pixels, w, h, w * 4, text, x, y, fontpixelsize, color, ff);
}

}
