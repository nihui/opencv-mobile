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

#include "v4l2_capture_rk_aiq.h"

#if defined __linux__
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dlfcn.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#if __ARM_NEON
#include <arm_neon.h>
#endif // __ARM_NEON


extern "C"
{

typedef enum {
    RK_AIQ_WORKING_MODE_NORMAL  = 0,
    RK_AIQ_WORKING_MODE_ISP_HDR2    = 0x10,
    RK_AIQ_WORKING_MODE_ISP_HDR3    = 0x20,
} rk_aiq_working_mode_t;

typedef enum {
    XCAM_RETURN_NO_ERROR        = 0,
    XCAM_RETURN_BYPASS          = 1,

    /* errors */
    XCAM_RETURN_ERROR_FAILED    = -1,
    XCAM_RETURN_ERROR_PARAM     = -2,
    XCAM_RETURN_ERROR_MEM       = -3,
    XCAM_RETURN_ERROR_FILE      = -4,
    XCAM_RETURN_ERROR_ANALYZER  = -5,
    XCAM_RETURN_ERROR_ISP       = -6,
    XCAM_RETURN_ERROR_SENSOR    = -7,
    XCAM_RETURN_ERROR_THREAD    = -8,
    XCAM_RETURN_ERROR_IOCTL     = -9,
    XCAM_RETURN_ERROR_ORDER     = -10,
    XCAM_RETURN_ERROR_TIMEOUT   = -20,
    XCAM_RETURN_ERROR_OUTOFRANGE = -21,
    XCAM_RETURN_ERROR_UNKNOWN   = -255,
} XCamReturn;

#define rk_fmt_fourcc(a, b, c, d)\
    ((uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))
#define rk_fmt_fourcc_be(a, b, c, d)    (rk_fmt_fourcc(a, b, c, d) | (1 << 31))

typedef enum {
    /*      Pixel format         FOURCC                          depth  Description  */

    /* RGB formats */
    RK_PIX_FMT_RGB332 = rk_fmt_fourcc('R', 'G', 'B', '1'), /*  8  RGB-3-3-2     */
    RK_PIX_FMT_RGB444 = rk_fmt_fourcc('R', '4', '4', '4'), /* 16  xxxxrrrr ggggbbbb */
    RK_PIX_FMT_ARGB444 = rk_fmt_fourcc('A', 'R', '1', '2'), /* 16  aaaarrrr ggggbbbb */
    RK_PIX_FMT_XRGB444 = rk_fmt_fourcc('X', 'R', '1', '2'), /* 16  xxxxrrrr ggggbbbb */
    RK_PIX_FMT_RGB555 = rk_fmt_fourcc('R', 'G', 'B', 'O'), /* 16  RGB-5-5-5     */
    RK_PIX_FMT_ARGB555 = rk_fmt_fourcc('A', 'R', '1', '5'), /* 16  ARGB-1-5-5-5  */
    RK_PIX_FMT_XRGB555 = rk_fmt_fourcc('X', 'R', '1', '5'), /* 16  XRGB-1-5-5-5  */
    RK_PIX_FMT_RGB565 = rk_fmt_fourcc('R', 'G', 'B', 'P'), /* 16  RGB-5-6-5     */
    RK_PIX_FMT_RGB555X = rk_fmt_fourcc('R', 'G', 'B', 'Q'), /* 16  RGB-5-5-5 BE  */
    RK_PIX_FMT_ARGB555X = rk_fmt_fourcc_be('A', 'R', '1', '5'), /* 16  ARGB-5-5-5 BE */
    RK_PIX_FMT_XRGB555X = rk_fmt_fourcc_be('X', 'R', '1', '5'), /* 16  XRGB-5-5-5 BE */
    RK_PIX_FMT_RGB565X = rk_fmt_fourcc('R', 'G', 'B', 'R'), /* 16  RGB-5-6-5 BE  */
    RK_PIX_FMT_BGR666 = rk_fmt_fourcc('B', 'G', 'R', 'H'), /* 18  BGR-6-6-6   */
    RK_PIX_FMT_BGR24  = rk_fmt_fourcc('B', 'G', 'R', '3'), /* 24  BGR-8-8-8     */
    RK_PIX_FMT_RGB24   = rk_fmt_fourcc('R', 'G', 'B', '3'), /* 24  RGB-8-8-8     */
    RK_PIX_FMT_BGR32  = rk_fmt_fourcc('B', 'G', 'R', '4'), /* 32  BGR-8-8-8-8   */
    RK_PIX_FMT_ABGR32  = rk_fmt_fourcc('A', 'R', '2', '4'), /* 32  BGRA-8-8-8-8  */
    RK_PIX_FMT_XBGR32 = rk_fmt_fourcc('X', 'R', '2', '4'), /* 32  BGRX-8-8-8-8  */
    RK_PIX_FMT_RGB32 = rk_fmt_fourcc('R', 'G', 'B', '4'), /* 32  RGB-8-8-8-8   */
    RK_PIX_FMT_ARGB32 = rk_fmt_fourcc('B', 'A', '2', '4'), /* 32  ARGB-8-8-8-8  */
    RK_PIX_FMT_XRGB32 = rk_fmt_fourcc('B', 'X', '2', '4'), /* 32  XRGB-8-8-8-8  */

    /* Grey formats */
    RK_PIX_FMT_GREY  = rk_fmt_fourcc('G', 'R', 'E', 'Y'), /*  8  Greyscale     */
    RK_PIX_FMT_Y4   = rk_fmt_fourcc('Y', '0', '4', ' '), /*  4  Greyscale     */
    RK_PIX_FMT_Y6   = rk_fmt_fourcc('Y', '0', '6', ' '), /*  6  Greyscale     */
    RK_PIX_FMT_Y10  = rk_fmt_fourcc('Y', '1', '0', ' '), /* 10  Greyscale     */
    RK_PIX_FMT_Y12  = rk_fmt_fourcc('Y', '1', '2', ' '), /* 12  Greyscale     */
    RK_PIX_FMT_Y16  = rk_fmt_fourcc('Y', '1', '6', ' '), /* 16  Greyscale     */
    RK_PIX_FMT_Y16_BE = rk_fmt_fourcc_be('Y', '1', '6', ' '), /* 16  Greyscale BE  */

    /* Grey bit-packed formats */
    RK_PIX_FMT_Y10BPACK  = rk_fmt_fourcc('Y', '1', '0', 'B'), /* 10  Greyscale bit-packed */

    /* Palette formats */
    RK_PIX_FMT_PAL8  = rk_fmt_fourcc('P', 'A', 'L', '8'), /*  8  8-bit palette */

    /* Chrominance formats */
    RK_PIX_FMT_UV8  = rk_fmt_fourcc('U', 'V', '8', ' '), /*  8  UV 4:4 */

    /* Luminance+Chrominance formats */
    RK_PIX_FMT_YVU410 = rk_fmt_fourcc('Y', 'V', 'U', '9'), /*  9  YVU 4:1:0     */
    RK_PIX_FMT_YVU420 = rk_fmt_fourcc('Y', 'V', '1', '2'), /* 12  YVU 4:2:0     */
    RK_PIX_FMT_YUYV  = rk_fmt_fourcc('Y', 'U', 'Y', 'V'), /* 16  YUV 4:2:2     */
    RK_PIX_FMT_YYUV  = rk_fmt_fourcc('Y', 'Y', 'U', 'V'), /* 16  YUV 4:2:2     */
    RK_PIX_FMT_YVYU  = rk_fmt_fourcc('Y', 'V', 'Y', 'U'), /* 16 YVU 4:2:2 */
    RK_PIX_FMT_UYVY  = rk_fmt_fourcc('U', 'Y', 'V', 'Y'), /* 16  YUV 4:2:2     */
    RK_PIX_FMT_VYUY  = rk_fmt_fourcc('V', 'Y', 'U', 'Y'), /* 16  YUV 4:2:2     */
    RK_PIX_FMT_YUV422P = rk_fmt_fourcc('4', '2', '2', 'P'), /* 16  YVU422 planar */
    RK_PIX_FMT_YUV411P = rk_fmt_fourcc('4', '1', '1', 'P'), /* 16  YVU411 planar */
    RK_PIX_FMT_Y41P  = rk_fmt_fourcc('Y', '4', '1', 'P'), /* 12  YUV 4:1:1     */
    RK_PIX_FMT_YUV444 = rk_fmt_fourcc('Y', '4', '4', '4'), /* 16  xxxxyyyy uuuuvvvv */
    RK_PIX_FMT_YUV555 = rk_fmt_fourcc('Y', 'U', 'V', 'O'), /* 16  YUV-5-5-5     */
    RK_PIX_FMT_YUV565 = rk_fmt_fourcc('Y', 'U', 'V', 'P'), /* 16  YUV-5-6-5     */
    RK_PIX_FMT_YUV32  = rk_fmt_fourcc('Y', 'U', 'V', '4'), /* 32  YUV-8-8-8-8   */
    RK_PIX_FMT_YUV410 = rk_fmt_fourcc('Y', 'U', 'V', '9'), /*  9  YUV 4:1:0     */
    RK_PIX_FMT_YUV420 = rk_fmt_fourcc('Y', 'U', '1', '2'), /* 12  YUV 4:2:0     */
    RK_PIX_FMT_HI240  = rk_fmt_fourcc('H', 'I', '2', '4'), /*  8  8-bit color   */
    RK_PIX_FMT_HM12  = rk_fmt_fourcc('H', 'M', '1', '2'), /*  8  YUV 4:2:0 16x16 macroblocks */
    RK_PIX_FMT_M420  = rk_fmt_fourcc('M', '4', '2', '0'), /* 12  YUV 4:2:0 2 lines y, 1 line uv interleaved */

    /* two planes -- one Y, one Cr + Cb interleaved  */
    RK_PIX_FMT_NV12  = rk_fmt_fourcc('N', 'V', '1', '2'), /* 12  Y/CbCr 4:2:0  */
    RK_PIX_FMT_NV21  = rk_fmt_fourcc('N', 'V', '2', '1'), /* 12  Y/CrCb 4:2:0  */
    RK_PIX_FMT_NV16  = rk_fmt_fourcc('N', 'V', '1', '6'), /* 16  Y/CbCr 4:2:2  */
    RK_PIX_FMT_NV61  = rk_fmt_fourcc('N', 'V', '6', '1'), /* 16  Y/CrCb 4:2:2  */
    RK_PIX_FMT_NV24  = rk_fmt_fourcc('N', 'V', '2', '4'), /* 24  Y/CbCr 4:4:4  */
    RK_PIX_FMT_NV42  = rk_fmt_fourcc('N', 'V', '4', '2'), /* 24  Y/CrCb 4:4:4  */

    /* two non contiguous planes - one Y, one Cr + Cb interleaved  */
    RK_PIX_FMT_NV12M = rk_fmt_fourcc('N', 'M', '1', '2'), /* 12  Y/CbCr 4:2:0  */
    RK_PIX_FMT_NV21M = rk_fmt_fourcc('N', 'M', '2', '1'), /* 21  Y/CrCb 4:2:0  */
    RK_PIX_FMT_NV16M = rk_fmt_fourcc('N', 'M', '1', '6'), /* 16  Y/CbCr 4:2:2  */
    RK_PIX_FMT_NV61M = rk_fmt_fourcc('N', 'M', '6', '1'), /* 16  Y/CrCb 4:2:2  */
    RK_PIX_FMT_NV12MT = rk_fmt_fourcc('T', 'M', '1', '2'), /* 12  Y/CbCr 4:2:0 64x32 macroblocks */
    RK_PIX_FMT_NV12MT_16X16 = rk_fmt_fourcc('V', 'M', '1', '2'), /* 12  Y/CbCr 4:2:0 16x16 macroblocks */

    /* three non contiguous planes - Y, Cb, Cr */
    RK_PIX_FMT_YUV420M = rk_fmt_fourcc('Y', 'M', '1', '2'), /* 12  YUV420 planar */
    RK_PIX_FMT_YVU420M = rk_fmt_fourcc('Y', 'M', '2', '1'), /* 12  YVU420 planar */
    RK_PIX_FMT_YUV422M = rk_fmt_fourcc('Y', 'M', '1', '6'), /* 16  YUV422 planar */
    RK_PIX_FMT_YVU422M = rk_fmt_fourcc('Y', 'M', '6', '1'), /* 16  YVU422 planar */
    RK_PIX_FMT_YUV444M = rk_fmt_fourcc('Y', 'M', '2', '4'), /* 24  YUV444 planar */
    RK_PIX_FMT_YVU444M = rk_fmt_fourcc('Y', 'M', '4', '2'), /* 24  YVU444 planar */

    /* Bayer formats - see http://www.siliconimaging.com/RGB%20Bayer.htm */
    RK_PIX_FMT_SBGGR8 = rk_fmt_fourcc('B', 'A', '8', '1'), /*  8  BGBG.. GRGR.. */
    RK_PIX_FMT_SGBRG8 = rk_fmt_fourcc('G', 'B', 'R', 'G'), /*  8  GBGB.. RGRG.. */
    RK_PIX_FMT_SGRBG8 = rk_fmt_fourcc('G', 'R', 'B', 'G'), /*  8  GRGR.. BGBG.. */
    RK_PIX_FMT_SRGGB8 = rk_fmt_fourcc('R', 'G', 'G', 'B'), /*  8  RGRG.. GBGB.. */
    RK_PIX_FMT_SBGGR10 = rk_fmt_fourcc('B', 'G', '1', '0'), /* 10  BGBG.. GRGR.. */
    RK_PIX_FMT_SGBRG10 = rk_fmt_fourcc('G', 'B', '1', '0'), /* 10  GBGB.. RGRG.. */
    RK_PIX_FMT_SGRBG10 = rk_fmt_fourcc('B', 'A', '1', '0'), /* 10  GRGR.. BGBG.. */
    RK_PIX_FMT_SRGGB10 = rk_fmt_fourcc('R', 'G', '1', '0'), /* 10  RGRG.. GBGB.. */
    /* 10bit raw bayer packed, 5 bytes for every 4 pixels */
    RK_PIX_FMT_SBGGR10P = rk_fmt_fourcc('p', 'B', 'A', 'A'),
    RK_PIX_FMT_SGBRG10P = rk_fmt_fourcc('p', 'G', 'A', 'A'),
    RK_PIX_FMT_SGRBG10P = rk_fmt_fourcc('p', 'g', 'A', 'A'),
    RK_PIX_FMT_SRGGB10P = rk_fmt_fourcc('p', 'R', 'A', 'A'),
    /* 10bit raw bayer a-law compressed to 8 bits */
    RK_PIX_FMT_SBGGR10ALAW8 = rk_fmt_fourcc('a', 'B', 'A', '8'),
    RK_PIX_FMT_SGBRG10ALAW8 = rk_fmt_fourcc('a', 'G', 'A', '8'),
    RK_PIX_FMT_SGRBG10ALAW8 = rk_fmt_fourcc('a', 'g', 'A', '8'),
    RK_PIX_FMT_SRGGB10ALAW8 = rk_fmt_fourcc('a', 'R', 'A', '8'),
    /* 10bit raw bayer DPCM compressed to 8 bits */
    RK_PIX_FMT_SBGGR10DPCM8 = rk_fmt_fourcc('b', 'B', 'A', '8'),
    RK_PIX_FMT_SGBRG10DPCM8 = rk_fmt_fourcc('b', 'G', 'A', '8'),
    RK_PIX_FMT_SGRBG10DPCM8 = rk_fmt_fourcc('B', 'D', '1', '0'),
    RK_PIX_FMT_SRGGB10DPCM8 = rk_fmt_fourcc('b', 'R', 'A', '8'),
    RK_PIX_FMT_SBGGR12 = rk_fmt_fourcc('B', 'G', '1', '2'), /* 12  BGBG.. GRGR.. */
    RK_PIX_FMT_SGBRG12 = rk_fmt_fourcc('G', 'B', '1', '2'), /* 12  GBGB.. RGRG.. */
    RK_PIX_FMT_SGRBG12 = rk_fmt_fourcc('B', 'A', '1', '2'), /* 12  GRGR.. BGBG.. */
    RK_PIX_FMT_SRGGB12 = rk_fmt_fourcc('R', 'G', '1', '2'), /* 12  RGRG.. GBGB.. */
    RK_PIX_FMT_SBGGR14 = rk_fmt_fourcc('B', 'G', '1', '4'), /* 14  BGBG.. GRGR.. */
    RK_PIX_FMT_SGBRG14 = rk_fmt_fourcc('G', 'B', '1', '4'), /* 14  GBGB.. RGRG.. */
    RK_PIX_FMT_SGRBG14 = rk_fmt_fourcc('B', 'A', '1', '4'), /* 14  GRGR.. BGBG.. */
    RK_PIX_FMT_SRGGB14 = rk_fmt_fourcc('R', 'G', '1', '4'), /* 14  RGRG.. GBGB.. */
    RK_PIX_FMT_SBGGR16 = rk_fmt_fourcc('B', 'Y', 'R', '6'), /* 16  BGBG.. GRGR.. */

    /* compressed formats */
    RK_PIX_FMT_MJPEG = rk_fmt_fourcc('M', 'J', 'P', 'G'), /* Motion-JPEG   */
    RK_PIX_FMT_JPEG  = rk_fmt_fourcc('J', 'P', 'E', 'G'), /* JFIF JPEG     */
    RK_PIX_FMT_DV   = rk_fmt_fourcc('d', 'v', 's', 'd'), /* 1394          */
    RK_PIX_FMT_MPEG  = rk_fmt_fourcc('M', 'P', 'E', 'G'), /* MPEG-1/2/4 Multiplexed */
    RK_PIX_FMT_H264  = rk_fmt_fourcc('H', '2', '6', '4'), /* H264 with start codes */
    RK_PIX_FMT_H264_NO_SC = rk_fmt_fourcc('A', 'V', 'C', '1'), /* H264 without start codes */
    RK_PIX_FMT_H264_MVC = rk_fmt_fourcc('M', '2', '6', '4'), /* H264 MVC */
    RK_PIX_FMT_H264_SLICE = rk_fmt_fourcc('S', '2', '6', '4'), /* H264 parsed slices */
    RK_PIX_FMT_H263   = rk_fmt_fourcc('H', '2', '6', '3'), /* H263          */
    RK_PIX_FMT_MPEG1 = rk_fmt_fourcc('M', 'P', 'G', '1'), /* MPEG-1 ES     */
    RK_PIX_FMT_MPEG2   = rk_fmt_fourcc('M', 'P', 'G', '2'), /* MPEG-2 ES     */
    RK_PIX_FMT_MPEG4   = rk_fmt_fourcc('M', 'P', 'G', '4'), /* MPEG-4 part 2 ES */
    RK_PIX_FMT_XVID    = rk_fmt_fourcc('X', 'V', 'I', 'D'), /* Xvid           */
    RK_PIX_FMT_VC1_ANNEX_G = rk_fmt_fourcc('V', 'C', '1', 'G'), /* SMPTE 421M Annex G compliant stream */
    RK_PIX_FMT_VC1_ANNEX_L = rk_fmt_fourcc('V', 'C', '1', 'L'), /* SMPTE 421M Annex L compliant stream */
    RK_PIX_FMT_VP8     = rk_fmt_fourcc('V', 'P', '8', '0'), /* VP8 */
    RK_PIX_FMT_VP8_FRAME = rk_fmt_fourcc('V', 'P', '8', 'F'), /* VP8 parsed frames */

    /*  Vendor-specific formats   */
    RK_PIX_FMT_CPIA1   = rk_fmt_fourcc('C', 'P', 'I', 'A'), /* cpia1 YUV */
    RK_PIX_FMT_WNVA    = rk_fmt_fourcc('W', 'N', 'V', 'A'), /* Winnov hw compress */
    RK_PIX_FMT_SN9C10X = rk_fmt_fourcc('S', '9', '1', '0'), /* SN9C10x compression */
    RK_PIX_FMT_SN9C20X_I420 = rk_fmt_fourcc('S', '9', '2', '0'), /* SN9C20x YUV 4:2:0 */
    RK_PIX_FMT_PWC1    = rk_fmt_fourcc('P', 'W', 'C', '1'), /* pwc older webcam */
    RK_PIX_FMT_PWC2    = rk_fmt_fourcc('P', 'W', 'C', '2'), /* pwc newer webcam */
    RK_PIX_FMT_ET61X251 = rk_fmt_fourcc('E', '6', '2', '5'), /* ET61X251 compression */
    RK_PIX_FMT_SPCA501 = rk_fmt_fourcc('S', '5', '0', '1'), /* YUYV per line */
    RK_PIX_FMT_SPCA505 = rk_fmt_fourcc('S', '5', '0', '5'), /* YYUV per line */
    RK_PIX_FMT_SPCA508 = rk_fmt_fourcc('S', '5', '0', '8'), /* YUVY per line */
    RK_PIX_FMT_SPCA561 = rk_fmt_fourcc('S', '5', '6', '1'), /* compressed GBRG bayer */
    RK_PIX_FMT_PAC207  = rk_fmt_fourcc('P', '2', '0', '7'), /* compressed BGGR bayer */
    RK_PIX_FMT_MR97310A = rk_fmt_fourcc('M', '3', '1', '0'), /* compressed BGGR bayer */
    RK_PIX_FMT_JL2005BCD = rk_fmt_fourcc('J', 'L', '2', '0'), /* compressed RGGB bayer */
    RK_PIX_FMT_SN9C2028 = rk_fmt_fourcc('S', 'O', 'N', 'X'), /* compressed GBRG bayer */
    RK_PIX_FMT_SQ905C  = rk_fmt_fourcc('9', '0', '5', 'C'), /* compressed RGGB bayer */
    RK_PIX_FMT_PJPG    = rk_fmt_fourcc('P', 'J', 'P', 'G'), /* Pixart 73xx JPEG */
    RK_PIX_FMT_OV511   = rk_fmt_fourcc('O', '5', '1', '1'), /* ov511 JPEG */
    RK_PIX_FMT_OV518   = rk_fmt_fourcc('O', '5', '1', '8'), /* ov518 JPEG */
    RK_PIX_FMT_STV0680 = rk_fmt_fourcc('S', '6', '8', '0'), /* stv0680 bayer */
    RK_PIX_FMT_TM6000  = rk_fmt_fourcc('T', 'M', '6', '0'), /* tm5600/tm60x0 */
    RK_PIX_FMT_CIT_YYVYUY = rk_fmt_fourcc('C', 'I', 'T', 'V'), /* one line of Y then 1 line of VYUY */
    RK_PIX_FMT_KONICA420 = rk_fmt_fourcc('K', 'O', 'N', 'I'), /* YUV420 planar in blocks of 256 pixels */
    RK_PIX_FMT_JPGL      =  rk_fmt_fourcc('J', 'P', 'G', 'L'), /* JPEG-Lite */
    RK_PIX_FMT_SE401     = rk_fmt_fourcc('S', '4', '0', '1'), /* se401 janggu compressed rgb */
    RK_PIX_FMT_S5C_UYVY_JPG = rk_fmt_fourcc('S', '5', 'C', 'I'), /* S5C73M3 interleaved UYVY/JPEG */
    RK_PIX_FMT_Y8I     = rk_fmt_fourcc('Y', '8', 'I', ' '), /* Greyscale 8-bit L/R interleaved */
    RK_PIX_FMT_Y12I    = rk_fmt_fourcc('Y', '1', '2', 'I'), /* Greyscale 12-bit L/R interleaved */
    RK_PIX_FMT_Z16     = rk_fmt_fourcc('Z', '1', '6', ' '), /* Depth data 16-bit */
} rk_aiq_format_t;

typedef struct rk_frame_fmt_s {
    int32_t width;
    int32_t height;
    rk_aiq_format_t format;
    int32_t fps;
    int32_t hdr_mode;
} rk_frame_fmt_t;

#define SUPPORT_FMT_MAX 10
typedef struct {
    char sensor_name[32];
    rk_frame_fmt_t  support_fmt[SUPPORT_FMT_MAX];
    int32_t num;
    /* binded pp stream media index */
    int8_t binded_strm_media_idx;
    int phyId;
} rk_aiq_sensor_info_t;

typedef struct {
    char len_name[32];
} rk_aiq_lens_info_t;

typedef struct {
    rk_aiq_sensor_info_t    sensor_info;
    rk_aiq_lens_info_t      lens_info;
    int  isp_hw_ver;
    bool has_lens_vcm; /*< has lens vcm */
    bool has_fl; /*< has flash light */
    bool fl_strth_adj_sup;
    bool has_irc; /*< has ircutter */
    bool fl_ir_strth_adj_sup;
    uint8_t is_multi_isp_mode;
    uint16_t multi_isp_extended_pixel;
    uint8_t reserved[13];
    // supported Antibanding modes
    // supported lock modes
    // supported ae compensation range/step
    // supported ae measure mode
    // supported af modes
    // other from iq
} rk_aiq_static_info_t;

typedef struct rk_aiq_tb_info_s {
    uint16_t magic;
    bool is_pre_aiq;
    uint8_t prd_type;
} rk_aiq_tb_info_t;

typedef struct rk_aiq_sys_ctx_s rk_aiq_sys_ctx_t;

typedef struct rk_aiq_err_msg_s {
    int err_code;
} rk_aiq_err_msg_t;

typedef struct rk_aiq_metas_s {
    uint32_t frame_id;
} rk_aiq_metas_t;

typedef XCamReturn (*rk_aiq_error_cb)(rk_aiq_err_msg_t* err_msg);
typedef XCamReturn (*rk_aiq_metas_cb)(rk_aiq_metas_t* metas);
// typedef XCamReturn (*rk_aiq_hwevt_cb)(rk_aiq_hwevt_t* hwevt);

typedef const char* (*PFN_rk_aiq_uapi2_sysctl_getBindedSnsEntNmByVd)(const char* vd);

typedef XCamReturn (*PFN_rk_aiq_uapi2_sysctl_getStaticMetas)(const char* sns_ent_name, rk_aiq_static_info_t* static_info);

typedef XCamReturn (*PFN_rk_aiq_uapi2_sysctl_preInit_tb_info)(const char* sns_ent_name, const rk_aiq_tb_info_t* info);

typedef XCamReturn (*PFN_rk_aiq_uapi2_sysctl_preInit_scene)(const char* sns_ent_name, const char *main_scene, const char *sub_scene);

// XCamReturn rk_aiq_uapi2_sysctl_preInit(const char* sns_ent_name, rk_aiq_working_mode_t mode, const char* force_iq_file);

typedef rk_aiq_sys_ctx_t* (*PFN_rk_aiq_uapi2_sysctl_init)(const char* sns_ent_name, const char* iq_file_dir, rk_aiq_error_cb err_cb, rk_aiq_metas_cb metas_cb);

typedef XCamReturn (*PFN_rk_aiq_uapi2_sysctl_prepare)(const rk_aiq_sys_ctx_t* ctx, uint32_t  width, uint32_t  height, rk_aiq_working_mode_t mode);

typedef XCamReturn (*PFN_rk_aiq_uapi2_sysctl_start)(const rk_aiq_sys_ctx_t* ctx);

typedef XCamReturn (*PFN_rk_aiq_uapi2_sysctl_stop)(const rk_aiq_sys_ctx_t* ctx, bool keep_ext_hw_st);

typedef void (*PFN_rk_aiq_uapi2_sysctl_deinit)(rk_aiq_sys_ctx_t* ctx);

}

static void* librkaiq = 0;

static PFN_rk_aiq_uapi2_sysctl_getBindedSnsEntNmByVd rk_aiq_uapi2_sysctl_getBindedSnsEntNmByVd = 0;
static PFN_rk_aiq_uapi2_sysctl_getStaticMetas rk_aiq_uapi2_sysctl_getStaticMetas = 0;
static PFN_rk_aiq_uapi2_sysctl_preInit_tb_info rk_aiq_uapi2_sysctl_preInit_tb_info = 0;
static PFN_rk_aiq_uapi2_sysctl_preInit_scene rk_aiq_uapi2_sysctl_preInit_scene = 0;
static PFN_rk_aiq_uapi2_sysctl_init rk_aiq_uapi2_sysctl_init = 0;
static PFN_rk_aiq_uapi2_sysctl_prepare rk_aiq_uapi2_sysctl_prepare = 0;
static PFN_rk_aiq_uapi2_sysctl_start rk_aiq_uapi2_sysctl_start = 0;
static PFN_rk_aiq_uapi2_sysctl_stop rk_aiq_uapi2_sysctl_stop = 0;
static PFN_rk_aiq_uapi2_sysctl_deinit rk_aiq_uapi2_sysctl_deinit = 0;

static int load_rkaiq_library()
{
    if (librkaiq)
        return 0;

    // check device whitelist
    bool whitelisted = false;
    {
        FILE* fp = fopen("/proc/device-tree/model", "rb");
        if (!fp)
            return -1;

        char buf[1024];
        fgets(buf, 1024, fp);
        fclose(fp);

        if (strncmp(buf, "Luckfox Pico", 12) == 0)
        {
            // luckfox pico family and plus pro max mini variants
            whitelisted = true;
        }
    }

    if (!whitelisted)
    {
        fprintf(stderr, "this device is not \n");
        return -1;
    }

    librkaiq = dlopen("librkaiq.so", RTLD_LOCAL | RTLD_NOW);
    if (!librkaiq)
    {
        librkaiq = dlopen("/oem/usr/lib/librkaiq.so", RTLD_LOCAL | RTLD_NOW);
    }
    if (!librkaiq)
    {
        return -1;
    }

    rk_aiq_uapi2_sysctl_getBindedSnsEntNmByVd = (PFN_rk_aiq_uapi2_sysctl_getBindedSnsEntNmByVd)dlsym(librkaiq, "rk_aiq_uapi2_sysctl_getBindedSnsEntNmByVd");
    rk_aiq_uapi2_sysctl_getStaticMetas = (PFN_rk_aiq_uapi2_sysctl_getStaticMetas)dlsym(librkaiq, "rk_aiq_uapi2_sysctl_getStaticMetas");
    rk_aiq_uapi2_sysctl_preInit_tb_info = (PFN_rk_aiq_uapi2_sysctl_preInit_tb_info)dlsym(librkaiq, "rk_aiq_uapi2_sysctl_preInit_tb_info");
    rk_aiq_uapi2_sysctl_preInit_scene = (PFN_rk_aiq_uapi2_sysctl_preInit_scene)dlsym(librkaiq, "rk_aiq_uapi2_sysctl_preInit_scene");
    rk_aiq_uapi2_sysctl_init = (PFN_rk_aiq_uapi2_sysctl_init)dlsym(librkaiq, "rk_aiq_uapi2_sysctl_init");
    rk_aiq_uapi2_sysctl_prepare = (PFN_rk_aiq_uapi2_sysctl_prepare)dlsym(librkaiq, "rk_aiq_uapi2_sysctl_prepare");
    rk_aiq_uapi2_sysctl_start = (PFN_rk_aiq_uapi2_sysctl_start)dlsym(librkaiq, "rk_aiq_uapi2_sysctl_start");
    rk_aiq_uapi2_sysctl_stop = (PFN_rk_aiq_uapi2_sysctl_stop)dlsym(librkaiq, "rk_aiq_uapi2_sysctl_stop");
    rk_aiq_uapi2_sysctl_deinit = (PFN_rk_aiq_uapi2_sysctl_deinit)dlsym(librkaiq, "rk_aiq_uapi2_sysctl_deinit");

    return 0;
}

static int unload_rkaiq_library()
{
    if (!librkaiq)
        return 0;

    dlclose(librkaiq);
    librkaiq = 0;

    rk_aiq_uapi2_sysctl_getBindedSnsEntNmByVd = 0;
    rk_aiq_uapi2_sysctl_getStaticMetas = 0;
    rk_aiq_uapi2_sysctl_preInit_tb_info = 0;
    rk_aiq_uapi2_sysctl_preInit_scene = 0;
    rk_aiq_uapi2_sysctl_init = 0;
    rk_aiq_uapi2_sysctl_prepare = 0;
    rk_aiq_uapi2_sysctl_start = 0;
    rk_aiq_uapi2_sysctl_stop = 0;
    rk_aiq_uapi2_sysctl_deinit = 0;

    return 0;
}

class rkaiq_library_loader
{
public:
    bool ready;

    rkaiq_library_loader()
    {
        ready = (load_rkaiq_library() == 0);
    }

    ~rkaiq_library_loader()
    {
        unload_rkaiq_library();
    }
};

static rkaiq_library_loader rkaiq;

extern "C"
{

typedef enum {
  IM_STATUS_NOERROR = 2,
  IM_STATUS_SUCCESS = 1,
  IM_STATUS_NOT_SUPPORTED = -1,
  IM_STATUS_OUT_OF_MEMORY = -2,
  IM_STATUS_INVALID_PARAM = -3,
  IM_STATUS_ILLEGAL_PARAM = -4,
  IM_STATUS_ERROR_VERSION = -5,
  IM_STATUS_FAILED = 0,
} IM_STATUS;

/* In order to be compatible with RK_FORMAT_XX and HAL_PIXEL_FORMAT_XX,
 * RK_FORMAT_XX is shifted to the left by 8 bits to distinguish.  */
typedef enum _Rga_SURF_FORMAT {
  RK_FORMAT_RGBA_8888 = 0x0 << 8,
  RK_FORMAT_RGBX_8888 = 0x1 << 8,
  RK_FORMAT_RGB_888 = 0x2 << 8,
  RK_FORMAT_BGRA_8888 = 0x3 << 8,
  RK_FORMAT_RGB_565 = 0x4 << 8,
  RK_FORMAT_RGBA_5551 = 0x5 << 8,
  RK_FORMAT_RGBA_4444 = 0x6 << 8,
  RK_FORMAT_BGR_888 = 0x7 << 8,

  RK_FORMAT_YCbCr_422_SP = 0x8 << 8,
  RK_FORMAT_YCbCr_422_P = 0x9 << 8,
  RK_FORMAT_YCbCr_420_SP = 0xa << 8,
  RK_FORMAT_YCbCr_420_P = 0xb << 8,

  RK_FORMAT_YCrCb_422_SP = 0xc << 8,
  RK_FORMAT_YCrCb_422_P = 0xd << 8,
  RK_FORMAT_YCrCb_420_SP = 0xe << 8,
  RK_FORMAT_YCrCb_420_P = 0xf << 8,

  RK_FORMAT_BPP1 = 0x10 << 8,
  RK_FORMAT_BPP2 = 0x11 << 8,
  RK_FORMAT_BPP4 = 0x12 << 8,
  RK_FORMAT_BPP8 = 0x13 << 8,

  RK_FORMAT_Y4 = 0x14 << 8,
  RK_FORMAT_YCbCr_400 = 0x15 << 8,

  RK_FORMAT_BGRX_8888 = 0x16 << 8,

  RK_FORMAT_YVYU_422 = 0x18 << 8,
  RK_FORMAT_YVYU_420 = 0x19 << 8,
  RK_FORMAT_VYUY_422 = 0x1a << 8,
  RK_FORMAT_VYUY_420 = 0x1b << 8,
  RK_FORMAT_YUYV_422 = 0x1c << 8,
  RK_FORMAT_YUYV_420 = 0x1d << 8,
  RK_FORMAT_UYVY_422 = 0x1e << 8,
  RK_FORMAT_UYVY_420 = 0x1f << 8,

  RK_FORMAT_YCbCr_420_SP_10B = 0x20 << 8,
  RK_FORMAT_YCrCb_420_SP_10B = 0x21 << 8,
  RK_FORMAT_YCbCr_422_SP_10B = 0x22 << 8,
  RK_FORMAT_YCrCb_422_SP_10B = 0x23 << 8,
  /* For compatibility with misspellings */
  RK_FORMAT_YCbCr_422_10b_SP = RK_FORMAT_YCbCr_422_SP_10B,
  RK_FORMAT_YCrCb_422_10b_SP = RK_FORMAT_YCrCb_422_SP_10B,

  RK_FORMAT_BGR_565 = 0x24 << 8,
  RK_FORMAT_BGRA_5551 = 0x25 << 8,
  RK_FORMAT_BGRA_4444 = 0x26 << 8,

  RK_FORMAT_ARGB_8888 = 0x28 << 8,
  RK_FORMAT_XRGB_8888 = 0x29 << 8,
  RK_FORMAT_ARGB_5551 = 0x2a << 8,
  RK_FORMAT_ARGB_4444 = 0x2b << 8,
  RK_FORMAT_ABGR_8888 = 0x2c << 8,
  RK_FORMAT_XBGR_8888 = 0x2d << 8,
  RK_FORMAT_ABGR_5551 = 0x2e << 8,
  RK_FORMAT_ABGR_4444 = 0x2f << 8,

  RK_FORMAT_RGBA2BPP = 0x30 << 8,

  RK_FORMAT_UNKNOWN = 0x100 << 8,
} RgaSURF_FORMAT;

/* Status codes, returned by any blit function */
typedef enum {
  IM_YUV_TO_RGB_BT601_LIMIT = 1 << 0,
  IM_YUV_TO_RGB_BT601_FULL = 2 << 0,
  IM_YUV_TO_RGB_BT709_LIMIT = 3 << 0,
  IM_YUV_TO_RGB_MASK = 3 << 0,
  IM_RGB_TO_YUV_BT601_FULL = 1 << 2,
  IM_RGB_TO_YUV_BT601_LIMIT = 2 << 2,
  IM_RGB_TO_YUV_BT709_LIMIT = 3 << 2,
  IM_RGB_TO_YUV_MASK = 3 << 2,
  IM_RGB_TO_Y4 = 1 << 4,
  IM_RGB_TO_Y4_DITHER = 2 << 4,
  IM_RGB_TO_Y1_DITHER = 3 << 4,
  IM_Y4_MASK = 3 << 4,
  IM_RGB_FULL = 1 << 8,
  IM_RGB_CLIP = 2 << 8,
  IM_YUV_BT601_LIMIT_RANGE = 3 << 8,
  IM_YUV_BT601_FULL_RANGE = 4 << 8,
  IM_YUV_BT709_LIMIT_RANGE = 5 << 8,
  IM_YUV_BT709_FULL_RANGE = 6 << 8,
  IM_FULL_CSC_MASK = 0xf << 8,
  IM_COLOR_SPACE_DEFAULT = 0,
} IM_COLOR_SPACE_MODE;

typedef uint32_t rga_buffer_handle_t;

typedef struct {
  int max; /* The Maximum value of the color key */
  int min; /* The minimum value of the color key */
} im_colorkey_range;

typedef struct im_nn {
  int scale_r;  /* scaling factor on R channal */
  int scale_g;  /* scaling factor on G channal */
  int scale_b;  /* scaling factor on B channal */
  int offset_r; /* offset on R channal */
  int offset_g; /* offset on G channal */
  int offset_b; /* offset on B channal */
} im_nn_t;

/* im_info definition */
typedef struct {
  void *vir_addr; /* virtual address */
  void *phy_addr; /* physical address */
  int fd;         /* shared fd */

  int width;   /* width */
  int height;  /* height */
  int wstride; /* wstride */
  int hstride; /* hstride */
  int format;  /* format */

  int color_space_mode; /* color_space_mode */
  int global_alpha;     /* global_alpha */
  int rd_mode;

  /* legarcy */
  int color;                        /* color, used by color fill */
  im_colorkey_range colorkey_range; /* range value of color key */
  im_nn_t nn;
  int rop_code;

  rga_buffer_handle_t handle; /* buffer handle */
} rga_buffer_t;

typedef rga_buffer_t (*PFN_wrapbuffer_handle_t)(rga_buffer_handle_t handle, int width, int height, int wstride, int hstride, int format);
typedef IM_STATUS (*PFN_releasebuffer_handle)(rga_buffer_handle_t handle);

typedef const char* (*PFN_imStrError_t)(IM_STATUS status);

typedef IM_STATUS (*PFN_imcvtcolor_t)(rga_buffer_t src, rga_buffer_t dst, int sfmt, int dfmt, int mode, int sync);

}

typedef rga_buffer_handle_t (*PFN_importbuffer_fd)(int fd, int size);

typedef float (*PFN_get_bpp_from_format)(int format);

static void* librga = 0;

static PFN_wrapbuffer_handle_t wrapbuffer_handle_t = 0;
static PFN_importbuffer_fd importbuffer_fd = 0;
static PFN_releasebuffer_handle releasebuffer_handle = 0;
static PFN_imStrError_t imStrError_t = 0;
static PFN_imcvtcolor_t imcvtcolor_t = 0;

#define wrapbuffer_handle(handle, width, height, format) wrapbuffer_handle_t(handle, width, height, width, height, format)

static int load_rga_library()
{
    if (librga)
        return 0;

    // check device whitelist
    bool whitelisted = false;
    {
        FILE* fp = fopen("/proc/device-tree/model", "rb");
        if (!fp)
            return -1;

        char buf[1024];
        fgets(buf, 1024, fp);
        fclose(fp);

        if (strncmp(buf, "Luckfox Pico", 12) == 0)
        {
            // luckfox pico family and plus pro max mini variants
            whitelisted = true;
        }
    }

    if (!whitelisted)
    {
        fprintf(stderr, "this device is not \n");
        return -1;
    }

    librga = dlopen("librga.so", RTLD_LOCAL | RTLD_NOW);
    if (!librga)
    {
        librga = dlopen("/oem/usr/lib/librga.so", RTLD_LOCAL | RTLD_NOW);
    }
    if (!librga)
    {
        return -1;
    }

    wrapbuffer_handle_t = (PFN_wrapbuffer_handle_t)dlsym(librga, "wrapbuffer_handle_t");
    importbuffer_fd = (PFN_importbuffer_fd)dlsym(librga, "_Z15importbuffer_fdii");
    releasebuffer_handle = (PFN_releasebuffer_handle)dlsym(librga, "releasebuffer_handle");
    imStrError_t = (PFN_imStrError_t)dlsym(librga, "imStrError_t");
    imcvtcolor_t = (PFN_imcvtcolor_t)dlsym(librga, "imcvtcolor_t");

    return 0;
}

static int unload_rga_library()
{
    if (!librga)
        return 0;

    dlclose(librga);
    librga = 0;

    wrapbuffer_handle_t = 0;
    importbuffer_fd = 0;
    releasebuffer_handle = 0;
    imStrError_t = 0;
    imcvtcolor_t = 0;

    return 0;
}

class rga_library_loader
{
public:
    bool ready;

    rga_library_loader()
    {
        ready = (load_rga_library() == 0);
    }

    ~rga_library_loader()
    {
        unload_rga_library();
    }
};

static rga_library_loader rga;



struct dma_heap_allocation_data
{
	__u64 len;
	__u32 fd;
	__u32 fd_flags;
	__u64 heap_flags;
};

#define DMA_HEAP_IOC_MAGIC		'H'
#define DMA_HEAP_IOCTL_ALLOC	_IOWR(DMA_HEAP_IOC_MAGIC, 0x0, struct dma_heap_allocation_data)

#define DMA_BUF_SYNC_READ      (1 << 0)
#define DMA_BUF_SYNC_WRITE     (2 << 0)
#define DMA_BUF_SYNC_RW        (DMA_BUF_SYNC_READ | DMA_BUF_SYNC_WRITE)
#define DMA_BUF_SYNC_START     (0 << 2)
#define DMA_BUF_SYNC_END       (1 << 2)

struct dma_buf_sync
{
	__u64 flags;
};

#define DMA_BUF_BASE		'b'
#define DMA_BUF_IOCTL_SYNC	_IOW(DMA_BUF_BASE, 0, struct dma_buf_sync)

static int dma_sync_device_to_cpu(int fd)
{
    struct dma_buf_sync sync = {0};

    sync.flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_RW;
    return ioctl(fd, DMA_BUF_IOCTL_SYNC, &sync);
}

static int dma_sync_cpu_to_device(int fd)
{
    struct dma_buf_sync sync = {0};

    sync.flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_RW;
    return ioctl(fd, DMA_BUF_IOCTL_SYNC, &sync);
}

static int dma_buf_alloc(const char* path, size_t size, int* fd, void** va)
{
    int dma_heap_fd = open(path, O_RDWR);
    if (dma_heap_fd < 0)
    {
        printf("open %s fail!\n", path);
        return dma_heap_fd;
    }

    struct dma_heap_allocation_data buf_data;
    memset(&buf_data, 0x0, sizeof(buf_data));
    buf_data.len = size;
    buf_data.fd_flags = O_CLOEXEC | O_RDWR;
    int ret = ioctl(dma_heap_fd, DMA_HEAP_IOCTL_ALLOC, &buf_data);
    if (ret < 0)
    {
        printf("RK_DMA_HEAP_ALLOC_BUFFER failed\n");
        return ret;
    }

    /* mmap va */
    int prot;
    if (fcntl(buf_data.fd, F_GETFL) & O_RDWR)
        prot = PROT_READ | PROT_WRITE;
    else
        prot = PROT_READ;

    /* mmap contiguors buffer to user */
    void* mmap_va = (void*)mmap(NULL, buf_data.len, prot, MAP_SHARED, buf_data.fd, 0);
    if (mmap_va == MAP_FAILED)
    {
        printf("mmap failed: %s\n", strerror(errno));
        return -errno;
    }

    *va = mmap_va;
    *fd = buf_data.fd;

    close(dma_heap_fd);

    return 0;
}

static void dma_buf_free(size_t size, int *fd, void *va)
{
    int len = size;
    munmap(va, len);

    close(*fd);
    *fd = -1;
}

class v4l2_capture_rk_aiq_impl
{
public:
    v4l2_capture_rk_aiq_impl();
    ~v4l2_capture_rk_aiq_impl();

    int open(int width, int height, float fps);

    int start_streaming();

    int read_frame(unsigned char* bgrdata);

    int stop_streaming();

    int close();

public:
    rk_aiq_sys_ctx_t* aiq_ctx;
    char devpath[32];
    int fd;
    v4l2_buf_type buf_type;

    __u32 cap_pixelformat;
    __u32 cap_width;
    __u32 cap_height;
    __u32 cap_numerator;
    __u32 cap_denominator;

    int output_width;
    int output_height;

    void* data;
    __u32 data_length;
    __s32 dmafd;

    void* dst_data;
    __u32 dst_data_length;
    int dst_dmafd;

    rga_buffer_handle_t src_handle;
    rga_buffer_handle_t dst_handle;
    rga_buffer_t src_img;
    rga_buffer_t dst_img;
};

v4l2_capture_rk_aiq_impl::v4l2_capture_rk_aiq_impl()
{
    aiq_ctx = 0;
    fd = -1;
    buf_type = (v4l2_buf_type)0;

    cap_pixelformat = 0;
    cap_width = 0;
    cap_height = 0;
    cap_numerator = 0;
    cap_denominator = 0;

    output_width = 0;
    output_height = 0;

    data = 0;
    data_length = 0;
    dmafd = 0;

    dst_data = 0;
    dst_data_length = 0;
    dst_dmafd = 0;

    src_handle = 0;
    dst_handle = 0;
}

v4l2_capture_rk_aiq_impl::~v4l2_capture_rk_aiq_impl()
{
    close();
}

int v4l2_capture_rk_aiq_impl::open(int width, int height, float fps)
{
    if (!rkaiq.ready)
    {
        fprintf(stderr, "rkaiq not ready\n");
        return -1;
    }

    if (!rga.ready)
    {
        fprintf(stderr, "rga not ready\n");
        return -1;
    }

    // enumerate /sys/class/video4linux/videoN/name and find rkisp_mainpath
    int rkisp_index = -1;
    for (int i = 0; i < 64; i++)
    {
        char path[256];
        sprintf(path, "/sys/class/video4linux/video%d/name", i);

        FILE* fp = fopen(path, "rb");
        if (!fp)
            continue;

        char line[32];
        fgets(line, 32, fp);

        fclose(fp);

        if (strncmp(line, "rkisp_mainpath", 14) == 0)
        {
            rkisp_index = i;
            break;
        }
    }

    if (rkisp_index == -1)
    {
        fprintf(stderr, "cannot find v4l device with name rkisp_mainpath\n");
        return -1;
    }

    sprintf(devpath, "/dev/video%d", rkisp_index);

    fd = ::open(devpath, O_RDWR | O_NONBLOCK, 0);
    if (fd < 0)
    {
        fprintf(stderr, "open %s failed %d %s\n", devpath, errno, strerror(errno));
        return -1;
    }

    // query cap
    {
        struct v4l2_capability caps;
        memset(&caps, 0, sizeof(caps));

        if (ioctl(fd, VIDIOC_QUERYCAP, &caps))
        {
            fprintf(stderr, "%s ioctl VIDIOC_QUERYCAP failed %d %s\n", devpath, errno, strerror(errno));
            goto OUT;
        }

        fprintf(stderr, "%s <----- video capture\n", devpath);
        fprintf(stderr, "   driver = %s\n", caps.driver);
        fprintf(stderr, "   card = %s\n", caps.card);
        fprintf(stderr, "   bus_info = %s\n", caps.bus_info);
        fprintf(stderr, "   version = %x\n", caps.version);
        fprintf(stderr, "   capabilities = %x\n", caps.capabilities);
        fprintf(stderr, "   device_caps = %x\n", caps.device_caps);

        if (caps.capabilities & V4L2_CAP_VIDEO_CAPTURE)
        {
            buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        }
        else if (caps.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
        {
            buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        }
        else
        {
            fprintf(stderr, "%s is not V4L2_CAP_VIDEO_CAPTURE or V4L2_CAP_VIDEO_CAPTURE_MPLANE\n", devpath);
            goto OUT;
        }

        if (!(caps.capabilities & V4L2_CAP_STREAMING))
        {
            fprintf(stderr, "%s is not V4L2_CAP_STREAMING\n", devpath);
            goto OUT;
        }
    }

    // pick the supported width height
    // pick the supported fps
    // prefer BGR/RGB
    // prefer uncompressed

    // pick area
    // pick aspect ratio

    // __u32 area = width * height;
    // float ratio = width / (float)height;

    // conditions
    // 1. w >= width && h >= height
    // 2. denominator / numerator >= fps
    // 3. BGR or RGB

    // pixelformat + width + height + numerator + denominator
    // struct format_type
    // {
    //     __u32 pixelformat;
    //     __u32 width;
    //     __u32 height;
    //     __u32 numerator;
    //     __u32 denominator;
    // };

    // enumerate format
    for (int i = 0; ; i++)
    {
        struct v4l2_fmtdesc fmtdesc;
        memset(&fmtdesc, 0, sizeof(fmtdesc));
        fmtdesc.index = i;
        fmtdesc.type = buf_type;
        if (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc))
        {
            if (errno == EINVAL)
                break;

            fprintf(stderr, "%s ioctl VIDIOC_ENUM_FMT failed %d %s\n", devpath, errno, strerror(errno));
            goto OUT;
        }

        // fprintf(stderr, "   fmt = %s  %x\n", fmtdesc.description, fmtdesc.pixelformat);

        if (fmtdesc.flags & V4L2_FMT_FLAG_COMPRESSED)
        {
            continue;
        }

        if (cap_pixelformat == 0)
        {
            cap_pixelformat = fmtdesc.pixelformat;
        }

        // enumerate size
        for (int j = 0; ; j++)
        {
            struct v4l2_frmsizeenum frmsizeenum;
            memset(&frmsizeenum, 0, sizeof(frmsizeenum));
            frmsizeenum.index = j;
            frmsizeenum.pixel_format = fmtdesc.pixelformat;
            if (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsizeenum))
            {
                if (errno == EINVAL)
                    break;

                fprintf(stderr, "%s ioctl VIDIOC_ENUM_FRAMESIZES failed %d %s\n", devpath, errno, strerror(errno));
                goto OUT;
            }

            if (frmsizeenum.type == V4L2_FRMSIZE_TYPE_DISCRETE)
            {
                __u32 w = frmsizeenum.discrete.width;
                __u32 h = frmsizeenum.discrete.height;
                // fprintf(stderr, "       size = %d x %d\n", w, h);

                if (cap_width == 0 || cap_height == 0)
                {
                    cap_width = w;
                    cap_height = h;
                }
            }
            if (frmsizeenum.type == V4L2_FRMSIZE_TYPE_CONTINUOUS)
            {
                __u32 minw = frmsizeenum.stepwise.min_width;
                __u32 maxw = frmsizeenum.stepwise.max_width;
                __u32 minh = frmsizeenum.stepwise.min_height;
                __u32 maxh = frmsizeenum.stepwise.max_height;
                // fprintf(stderr, "       size = %d x %d  ~  %d x %d\n", minw, minh, maxw, maxh);

                if (cap_width == 0 || cap_height == 0)
                {
                    // cap_width = std::max(minw, maxw / 2);
                    // cap_height = std::max(minh, maxh / 2);
                    cap_width = maxw;
                    cap_height = maxh;
                }
            }
            if (frmsizeenum.type == V4L2_FRMSIZE_TYPE_STEPWISE)
            {
                __u32 minw = frmsizeenum.stepwise.min_width;
                __u32 maxw = frmsizeenum.stepwise.max_width;
                __u32 sw = frmsizeenum.stepwise.step_width;
                __u32 minh = frmsizeenum.stepwise.min_height;
                __u32 maxh = frmsizeenum.stepwise.max_height;
                __u32 sh = frmsizeenum.stepwise.step_height;
                // fprintf(stderr, "       size = %d x %d  ~  %d x %d  (+%d +%d)\n", minw, minh, maxw, maxh, sw, sh);

                if (cap_width == 0 || cap_height == 0)
                {
                    // cap_width = std::max(minw, maxw - maxw / 2 / sh * sh);
                    // cap_height = std::max(minh, maxh - maxh / 2 / sh * sh);
                    cap_width = maxw;
                    cap_height = maxh;
                }
            }

            // enumerate fps
            for (int k = 0; ; k++)
            {
                struct v4l2_frmivalenum frmivalenum;
                memset(&frmivalenum, 0, sizeof(frmivalenum));
                frmivalenum.index = k;
                frmivalenum.pixel_format = fmtdesc.pixelformat;
                frmivalenum.width = cap_width;
                frmivalenum.height = cap_height;

                if (ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmivalenum))
                {
                    if (errno == EINVAL)
                        break;

                    fprintf(stderr, "%s ioctl VIDIOC_ENUM_FRAMEINTERVALS failed %d %s\n", devpath, errno, strerror(errno));
                    goto OUT;
                }

                if (frmivalenum.type == V4L2_FRMIVAL_TYPE_DISCRETE)
                {
                    __u32 numer = frmivalenum.discrete.numerator;
                    __u32 denom = frmivalenum.discrete.denominator;
                    // fprintf(stderr, "           fps = %d / %d\n", numer, denom);

                    if (cap_numerator == 0 || cap_denominator == 0)
                    {
                        cap_numerator = numer;
                        cap_denominator = denom;
                    }
                }
                if (frmivalenum.type == V4L2_FRMIVAL_TYPE_CONTINUOUS)
                {
                    __u32 min_numer = frmivalenum.stepwise.min.numerator;
                    __u32 max_numer = frmivalenum.stepwise.max.numerator;
                    __u32 min_denom = frmivalenum.stepwise.min.denominator;
                    __u32 max_denom = frmivalenum.stepwise.max.denominator;
                    // fprintf(stderr, "           fps = %d / %d  ~   %d / %d\n", min_numer, min_denom, max_numer, max_denom);

                    if (cap_numerator == 0 || cap_denominator == 0)
                    {
                        cap_numerator = std::max(min_numer, max_numer / 2);
                        cap_denominator = std::max(min_denom, max_denom / 2);
                    }
                }
                if (frmivalenum.type == V4L2_FRMIVAL_TYPE_STEPWISE)
                {
                    __u32 min_numer = frmivalenum.stepwise.min.numerator;
                    __u32 max_numer = frmivalenum.stepwise.max.numerator;
                    __u32 snumer = frmivalenum.stepwise.step.numerator;
                    __u32 min_denom = frmivalenum.stepwise.min.denominator;
                    __u32 max_denom = frmivalenum.stepwise.max.denominator;
                    __u32 sdenom = frmivalenum.stepwise.step.denominator;
                    // fprintf(stderr, "           fps = %d / %d  ~   %d / %d  (+%d +%d)\n", min_numer, min_denom, max_numer, max_denom, snumer, sdenom);

                    if (cap_numerator == 0 || cap_denominator == 0)
                    {
                        cap_numerator = std::max(min_numer, max_numer - max_numer / 2 / snumer * snumer);
                        cap_denominator = std::max(min_denom, max_denom - max_denom / 2 / sdenom * sdenom);
                    }
                }
            }
        }
    }

    // NOTE
    // width must be a multiple of 16
    // height must be a multiple of 2
    if (width / (float)height > cap_width / (float)cap_height)
    {
        // fatter
        cap_height = (width * cap_height / cap_width + 1) / 2 * 2;
        cap_width = (width + 15) / 16 * 16;
    }
    else
    {
        // thinner
        cap_width = (height * cap_width / cap_height + 15) / 16 * 16;
        cap_height = (height + 1) / 2 * 2;
    }

    cap_pixelformat = V4L2_PIX_FMT_NV21;

    // {
    //     const char* pp = (const char*)&cap_pixelformat;
    //     fprintf(stderr, "cap_pixelformat = %x  %c%c%c%c\n", cap_pixelformat, pp[0], pp[1], pp[2], pp[3]);
    //     fprintf(stderr, "cap_width = %d\n", cap_width);
    //     fprintf(stderr, "cap_height = %d\n", cap_height);
    //     fprintf(stderr, "cap_numerator = %d\n", cap_numerator);
    //     fprintf(stderr, "cap_denominator = %d\n", cap_denominator);
    // }

    if (cap_pixelformat == 0 || cap_width == 0 || cap_height == 0)
    {
        fprintf(stderr, "%s no supported pixel format or size\n", devpath);
        goto OUT;
    }

    {

    const char* sns_entity_name = rk_aiq_uapi2_sysctl_getBindedSnsEntNmByVd("/dev/video11");

    // printf("sns_entity_name = %s\n", sns_entity_name);

    // // query sensor meta info
    // {
    //     rk_aiq_static_info_t s_info;
    //     XCamReturn ret = rk_aiq_uapi2_sysctl_getStaticMetas(sns_entity_name, &s_info);
    //     if (ret != XCAM_RETURN_NO_ERROR)
    //     {
    //         fprintf(stderr, "rk_aiq_uapi2_sysctl_getStaticMetas %s failed %d\n", sns_entity_name, ret);
    //     }
    //
    //     for (int i = 0; i < SUPPORT_FMT_MAX; i++)
    //     {
    //         rk_frame_fmt_t& fmt = s_info.sensor_info.support_fmt[i];
    //         const char* pf = (const char*)&fmt.format;
    //         fprintf(stderr, "fmt %d x %d  %c%c%c%c  %d %d\n", fmt.width, fmt.height, pf[0], pf[1], pf[2], pf[3], fmt.fps, fmt.hdr_mode);
    //     }
    // }

    // preinit tb info
    {
        rk_aiq_tb_info_t tb_info;
        tb_info.magic = sizeof(rk_aiq_tb_info_t) - 2;
        tb_info.is_pre_aiq = false;
        XCamReturn ret = rk_aiq_uapi2_sysctl_preInit_tb_info(sns_entity_name, &tb_info);
        if (ret != XCAM_RETURN_NO_ERROR)
        {
            fprintf(stderr, "rk_aiq_uapi2_sysctl_preInit_tb_info %s failed %d\n", sns_entity_name, ret);
        }
    }

    // printf("preinit tb info done\n");

    // preinit scene
    {
        XCamReturn ret = rk_aiq_uapi2_sysctl_preInit_scene(sns_entity_name, "normal", "day");
        if (ret != XCAM_RETURN_NO_ERROR)
        {
            fprintf(stderr, "rk_aiq_uapi2_sysctl_preInit_scene %s failed %d\n", sns_entity_name, ret);
        }
    }

    // printf("preinit scene done\n");

    {
        // TODO /oem/usr/share/iqfiles/sc3336_CMK-OT2119-PC1_30IRC-F16.json
        aiq_ctx = rk_aiq_uapi2_sysctl_init(sns_entity_name, "/oem/usr/share/iqfiles", NULL, NULL);
        if (!aiq_ctx)
        {
            fprintf(stderr, "rk_aiq_uapi2_sysctl_init %s failed\n", sns_entity_name);
        }
    }

    // printf("rk_aiq_uapi2_sysctl_init done\n");

    /*
        * rk_aiq_uapi_setFecEn(aiq_ctx, true);
        * rk_aiq_uapi_setFecCorrectDirection(aiq_ctx, FEC_CORRECT_DIRECTION_Y);
        */

    {
        XCamReturn ret = rk_aiq_uapi2_sysctl_prepare(aiq_ctx, cap_width, cap_height, RK_AIQ_WORKING_MODE_NORMAL);
        if (ret != XCAM_RETURN_NO_ERROR)
        {
            fprintf(stderr, "rk_aiq_uapi2_sysctl_prepare failed %d\n", ret);
        }
    }

    // printf("rk_aiq_uapi2_sysctl_prepare done\n");

    // ret = rk_aiq_uapi2_setMirrorFlip(aiq_ctx, false, false, 3);

    }

    // control format and size
    {
        struct v4l2_format fmt;
        memset(&fmt, 0, sizeof(fmt));
        fmt.type = buf_type;
        fmt.fmt.pix.width = cap_width;
        fmt.fmt.pix.height = cap_height;
        fmt.fmt.pix.pixelformat = cap_pixelformat;
        // fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

        if (ioctl(fd, VIDIOC_S_FMT, &fmt))
        {
            fprintf(stderr, "%s ioctl VIDIOC_S_FMT failed %d %s\n", devpath, errno, strerror(errno));
            goto OUT;
        }

        if (ioctl(fd, VIDIOC_G_FMT, &fmt))
        {
            fprintf(stderr, "%s ioctl VIDIOC_G_FMT failed %d %s\n", devpath, errno, strerror(errno));
            goto OUT;
        }

        cap_pixelformat = fmt.fmt.pix.pixelformat;
        cap_width = fmt.fmt.pix.width;
        cap_height = fmt.fmt.pix.height;

        // const char* pp = (const char*)&cap_pixelformat;
        // fprintf(stderr, "cap_pixelformat = %x  %c%c%c%c\n", cap_pixelformat, pp[0], pp[1], pp[2], pp[3]);
        // fprintf(stderr, "cap_width = %d\n", cap_width);
        // fprintf(stderr, "cap_height = %d\n", cap_height);
        // fprintf(stderr, "fmt.fmt.pix.field = %d\n", fmt.fmt.pix.field);
        //
        //
        // fprintf(stderr, "bytesperline: %d\n", fmt.fmt.pix.bytesperline);

        output_width = std::min((int)cap_width, width);
        output_height = std::min((int)cap_height, height);
    }

    // control fps
    {
        struct v4l2_streamparm streamparm;
        memset(&streamparm, 0, sizeof(streamparm));
        streamparm.type = buf_type;

        if (ioctl(fd, VIDIOC_G_PARM, &streamparm))
        {
            if (errno == ENOTTY)
            {
                // VIDIOC_G_PARM not implemented
            }
            else
            {
                fprintf(stderr, "%s ioctl VIDIOC_G_PARM failed %d %s\n", devpath, errno, strerror(errno));
                goto OUT;
            }
        }

        if (streamparm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME)
        {
            streamparm.parm.capture.timeperframe.numerator = cap_numerator;
            streamparm.parm.capture.timeperframe.denominator = cap_denominator;
            if (ioctl(fd, VIDIOC_S_PARM, &streamparm))
            {
                fprintf(stderr, "%s ioctl VIDIOC_S_PARM failed %d %s\n", devpath, errno, strerror(errno));
                goto OUT;
            }

            cap_numerator = streamparm.parm.capture.timeperframe.numerator;
            cap_denominator = streamparm.parm.capture.timeperframe.denominator;
        }
        else
        {
            fprintf(stderr, "%s does not support changing fps\n", devpath);
        }

        // fprintf(stderr, "cap_numerator = %d\n", cap_numerator);
        // fprintf(stderr, "cap_denominator = %d\n", cap_denominator);
    }

    // mmap
    {
        struct v4l2_requestbuffers requestbuffers;
        memset(&requestbuffers, 0, sizeof(requestbuffers));
        requestbuffers.count = 1;// FIXME hardcode
        requestbuffers.type = buf_type;
        requestbuffers.memory = V4L2_MEMORY_MMAP;
        if (ioctl(fd, VIDIOC_REQBUFS, &requestbuffers))
        {
            fprintf(stderr, "%s ioctl VIDIOC_REQBUFS failed %d %s\n", devpath, errno, strerror(errno));
            goto OUT;
        }

        // fprintf(stderr, "requestbuffers.count = %d\n", requestbuffers.count);

        {
            struct v4l2_plane planes[2];
            memset(&planes[0], 0, sizeof(planes[0]));
            memset(&planes[1], 0, sizeof(planes[1]));

            struct v4l2_buffer buf;
            memset(&buf, 0, sizeof(buf));
            buf.type = buf_type;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = 0;

            if (buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
            {
                buf.m.planes = planes;
                buf.length = 2;
            }

            if (ioctl(fd, VIDIOC_QUERYBUF, &buf))
            {
                fprintf(stderr, "%s ioctl VIDIOC_QUERYBUF failed %d %s\n", devpath, errno, strerror(errno));
                goto OUT;
            }

            // fprintf(stderr, "planes count = %d\n", buf.length);

            if (buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
            {
                data = mmap(NULL, buf.m.planes[0].length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.planes[0].m.mem_offset);
                data_length = buf.m.planes[0].length;
            }
            else
            {
                data = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
                data_length = buf.length;
            }

            memset(data, 0, data_length);
        }

        {
            struct v4l2_exportbuffer exportbuffer;
            memset(&exportbuffer, 0, sizeof(exportbuffer));
            exportbuffer.type = buf_type;
            exportbuffer.index = 0;

            if (ioctl(fd, VIDIOC_EXPBUF, &exportbuffer))
            {
                fprintf(stderr, "%s ioctl VIDIOC_EXPBUF failed\n", devpath);
                goto OUT;
            }

            dmafd = exportbuffer.fd;
        }

        {
            struct v4l2_plane planes[2];
            memset(&planes[0], 0, sizeof(planes[0]));
            memset(&planes[1], 0, sizeof(planes[1]));

            struct v4l2_buffer buf;
            memset(&buf, 0, sizeof(buf));

            buf.type = buf_type;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = 0;

            if (buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
            {
                buf.m.planes = planes;
                buf.length = 2;
            }

            if (ioctl(fd, VIDIOC_QBUF, &buf))
            {
                fprintf(stderr, "%s ioctl VIDIOC_QBUF failed\n", devpath);
                goto OUT;
            }
        }
    }

    // allocate rga dst
    {
        dst_data = 0;
        dst_data_length = cap_width * cap_height * 3;
        dst_dmafd = 0;
        int ret = dma_buf_alloc("/dev/rk_dma_heap/rk-dma-heap-cma", dst_data_length, &dst_dmafd, (void**)&dst_data);
        if (ret < 0)
        {
            fprintf(stderr, "dma_buf_alloc %d failed\n", dst_data_length);
            goto OUT;
        }
    }

    // setup rga handle
    {
        src_handle = importbuffer_fd(dmafd, data_length);

        dst_handle = importbuffer_fd(dst_dmafd, dst_data_length);

        src_img = wrapbuffer_handle(src_handle, cap_width, cap_height, RK_FORMAT_YCrCb_420_SP);

        dst_img = wrapbuffer_handle(dst_handle, cap_width, cap_height, RK_FORMAT_BGR_888);
    }

    return 0;

OUT:

    close();
    return -1;
}

int v4l2_capture_rk_aiq_impl::start_streaming()
{

    {
        XCamReturn ret = rk_aiq_uapi2_sysctl_start(aiq_ctx);
        if (ret != XCAM_RETURN_NO_ERROR)
        {
            fprintf(stderr, "rk_aiq_uapi2_sysctl_start failed %d\n", ret);
        }
    }

    // printf("rk_aiq_uapi2_sysctl_start done\n");

    v4l2_buf_type type = buf_type;
    if (ioctl(fd, VIDIOC_STREAMON, &type))
    {
        fprintf(stderr, "%s ioctl VIDIOC_STREAMON failed %d %s\n", devpath, errno, strerror(errno));
        return -1;
    }

    return 0;
}

static inline void my_memcpy(unsigned char* dst, const unsigned char* src, int size)
{
#if __ARM_NEON
    int nn = size / 64;
    size -= nn * 64;
    while (nn--)
    {
        __builtin_prefetch(src + 64);
        uint8x16_t _p0 = vld1q_u8(src);
        uint8x16_t _p1 = vld1q_u8(src + 16);
        uint8x16_t _p2 = vld1q_u8(src + 32);
        uint8x16_t _p3 = vld1q_u8(src + 48);
        vst1q_u8(dst, _p0);
        vst1q_u8(dst + 16, _p1);
        vst1q_u8(dst + 32, _p2);
        vst1q_u8(dst + 48, _p3);
        src += 64;
        dst += 64;
    }
    if (size > 16)
    {
        uint8x16_t _p0 = vld1q_u8(src);
        vst1q_u8(dst, _p0);
        src += 16;
        dst += 16;
        size -= 16;
    }
    if (size > 8)
    {
        uint8x8_t _p0 = vld1_u8(src);
        vst1_u8(dst, _p0);
        src += 8;
        dst += 8;
        size -= 8;
    }
    while (size--)
    {
        *dst++ = *src++;
    }
#else
    memcpy(dst, src, size);
#endif
}

int v4l2_capture_rk_aiq_impl::read_frame(unsigned char* bgrdata)
{
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;

    int r = select(fd + 1, &fds, NULL, NULL, &tv);

    if (r == -1)
    {
        fprintf(stderr, "select %s failed\n", devpath);
        return -1;
    }

    if (r == 0)
    {
        fprintf(stderr, "select %s timeout\n", devpath);
        return -1;
    }

    struct v4l2_plane planes[2];
    memset(&planes[0], 0, sizeof(planes[0]));
    memset(&planes[1], 0, sizeof(planes[1]));

    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = buf_type;
    buf.memory = V4L2_MEMORY_MMAP;

    if (buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
    {
        buf.m.planes = planes;
        buf.length = 2;
    }

    if (ioctl(fd, VIDIOC_DQBUF, &buf))
    {
        fprintf(stderr, "%s ioctl VIDIOC_DQBUF failed %d %s\n", devpath, errno, strerror(errno));
        return -1;
    }

    // consume data
    {
        // https://gitlab.com/rk3588_linux/linux/linux-rga/-/tree/6dcee5b53480cb16bc41d17c5fb0b6253d9872e3

        IM_STATUS ret = imcvtcolor_t(src_img, dst_img, RK_FORMAT_YCrCb_420_SP, RK_FORMAT_BGR_888, IM_COLOR_SPACE_DEFAULT, 1);
        if (ret != IM_STATUS_SUCCESS)
        {
            fprintf(stderr, "imcvtcolor failed %d %s\n", ret, imStrError_t(ret));
            return -1;
        }

        dma_sync_device_to_cpu(dst_dmafd);

        // crop and copy to bgrdata
        {
            int x0 = (cap_width - output_width) / 2;
            int y0 = (cap_height - output_height) / 2;
            unsigned char* pout = bgrdata;
            const unsigned char* pin = (const unsigned char*)dst_data + y0 * cap_width * 3 + x0 * 3;
            for (int y = y0; y < output_height; y++)
            {
                my_memcpy(pout, pin, output_width * 3);
                pout += output_width * 3;
                pin += cap_width * 3;
            }
        }
    }

    // requeue buf
    // memset(&planes[0], 0, sizeof(planes[0]));
    // memset(&planes[1], 0, sizeof(planes[1]));

    if (buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
    {
        buf.m.planes = planes;
        buf.length = 2;
    }

    if (ioctl(fd, VIDIOC_QBUF, &buf))
    {
        fprintf(stderr, "%s ioctl VIDIOC_QBUF failed %d %s\n", devpath, errno, strerror(errno));
        return -1;
    }

    return 0;
}

int v4l2_capture_rk_aiq_impl::stop_streaming()
{
    v4l2_buf_type type = buf_type;
    if (ioctl(fd, VIDIOC_STREAMOFF, &type))
    {
        fprintf(stderr, "%s ioctl VIDIOC_STREAMOFF failed %d %s\n", devpath, errno, strerror(errno));
        return -1;
    }

    {
        XCamReturn ret = rk_aiq_uapi2_sysctl_stop(aiq_ctx, false);
        if (ret != XCAM_RETURN_NO_ERROR)
        {
            fprintf(stderr, "rk_aiq_uapi2_sysctl_stop failed %d\n", ret);
        }
    }

    // printf("rk_aiq_uapi2_sysctl_stop done\n");

    return 0;
}

int v4l2_capture_rk_aiq_impl::close()
{

    {
    if (aiq_ctx)
    {
        rk_aiq_uapi2_sysctl_deinit(aiq_ctx);
        aiq_ctx = 0;
    }

    // printf("rk_aiq_uapi2_sysctl_deinit done\n");
    }

    if (data)
    {
        munmap(data, data_length);
        data = 0;
        data_length = 0;
        dmafd = 0;
    }

    if (dst_data)
    {
        dma_buf_free(dst_data_length, &dst_dmafd, (void*)dst_data);
        dst_data = 0;
        dst_data_length = 0;
        dst_dmafd = 0;
    }

    if (src_handle)
    {
        releasebuffer_handle(src_handle);
        src_handle = 0;
    }
    if (dst_handle)
    {
        releasebuffer_handle(dst_handle);
        dst_handle = 0;
    }

    // printf("munmap done\n");
    if (fd >= 0)
    {
        ::close(fd);
        fd = -1;
    }

    // printf("close done\n");
    buf_type = (v4l2_buf_type)0;

    cap_pixelformat = 0;
    cap_width = 0;
    cap_height = 0;
    cap_numerator = 0;
    cap_denominator = 0;

    output_width = 0;
    output_height = 0;

    return 0;
}

bool v4l2_capture_rk_aiq::supported()
{
    if (!rkaiq.ready)
        return false;

    if (!rga.ready)
        return false;

    return true;
}

v4l2_capture_rk_aiq::v4l2_capture_rk_aiq() : d(new v4l2_capture_rk_aiq_impl)
{
}

v4l2_capture_rk_aiq::~v4l2_capture_rk_aiq()
{
    delete d;
}

int v4l2_capture_rk_aiq::open(int width, int height, float fps)
{
    return d->open(width, height, fps);
}

int v4l2_capture_rk_aiq::get_width() const
{
    return d->output_width;
}

int v4l2_capture_rk_aiq::get_height() const
{
    return d->output_height;
}

float v4l2_capture_rk_aiq::get_fps() const
{
    return d->cap_numerator ? d->cap_denominator / (float)d->cap_numerator : 0;
}

int v4l2_capture_rk_aiq::start_streaming()
{
    return d->start_streaming();
}

int v4l2_capture_rk_aiq::read_frame(unsigned char* bgrdata)
{
    return d->read_frame(bgrdata);
}

int v4l2_capture_rk_aiq::stop_streaming()
{
    return d->stop_streaming();
}

int v4l2_capture_rk_aiq::close()
{
    return d->close();
}

#else // defined __linux__

bool v4l2_capture_rk_aiq::supported()
{
    return false;
}

v4l2_capture_rk_aiq::v4l2_capture_rk_aiq()
{
}

v4l2_capture_rk_aiq::~v4l2_capture_rk_aiq()
{
}

int v4l2_capture_rk_aiq::open(int width, int height, int fps)
{
    return -1;
}

int v4l2_capture_rk_aiq::get_width() const
{
    return -1;
}

int v4l2_capture_rk_aiq::get_height() const
{
    return -1;
}

float v4l2_capture_rk_aiq::get_fps() const
{
    return 0.f;
}

int v4l2_capture_rk_aiq::start_streaming()
{
    return -1;
}

int v4l2_capture_rk_aiq::read_frame(unsigned char* bgrdata)
{
    return -1;
}

int v4l2_capture_rk_aiq::stop_streaming()
{
    return -1;
}

int v4l2_capture_rk_aiq::close()
{
    return -1;
}

#endif // defined __linux__
