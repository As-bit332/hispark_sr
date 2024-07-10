/*
 * Copyright (c) 2022 HiSilicon (Shanghai) Technologies CO., LIMITED.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/prctl.h>

#include "sample_comm_ive.h"
#include "ivs_md.h"
#include "ournnie.h"

#define SAMPLE_IVE_MD_IMAGE_NUM              2
#define SAMPLE_IVE_MD_X_WEIGHT               32768
#define SAMPLE_IVE_MD_Y_WEIGHT               32768
#define SAMPLE_IVE_MD_SAD_THR                (100 * (1 << 1))
#define SAMPLE_IVE_MD_CAL_WIN_SIZE(mod)      (1 << (2 + (mod)))
#define SAMPLE_IVE_MD_AREA_THR_STEP           8
#define SAMPLE_IVE_MD_COLOR                   0x0000FF00

typedef struct hiSAMPLE_IVE_MD_S {
    IVE_SRC_IMAGE_S astImg[SAMPLE_IVE_MD_IMAGE_NUM];
    IVE_DST_MEM_INFO_S stBlob;
    MD_ATTR_S stMdAttr;
    SAMPLE_RECT_ARRAY_S stRegion;
} SAMPLE_IVE_MD_S;

static HI_BOOL s_bStopSignal = HI_FALSE;
static pthread_t s_hMdThread = 0;
static SAMPLE_IVE_MD_S s_stMd;
static SAMPLE_IVE_SWITCH_S s_stMdSwitch = { HI_FALSE, HI_TRUE };
static SAMPLE_VI_CONFIG_S s_stViConfig = { 0 };

static HI_VOID SAMPLE_IVE_Md_Uninit(SAMPLE_IVE_MD_S *pstMd)
{
    HI_S32 i;
    HI_S32 s32Ret = HI_SUCCESS;

    for (i = 0; i < SAMPLE_IVE_MD_IMAGE_NUM; i++) {
        IVE_MMZ_FREE(pstMd->astImg[i].au64PhyAddr[0], pstMd->astImg[i].au64VirAddr[0]);
    }

    IVE_MMZ_FREE(pstMd->stBlob.u64PhyAddr, pstMd->stBlob.u64VirAddr);

    s32Ret = HI_IVS_MD_Exit();
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("HI_IVS_MD_Exit fail,Error(%#x)\n", s32Ret);
        return;
    }
}

static HI_S32 SAMPLE_IVE_Md_Init(SAMPLE_IVE_MD_S *pstMd, HI_U32 u32Width, HI_U32 u32Height)
{
    HI_S32 s32Ret = HI_SUCCESS;
    HI_S32 i;
    HI_U32 u32Size;
    HI_U8 u8WndSz;
    HI_U8 u8SadMode = pstMd->stMdAttr.enSadMode;

    for (i = 0; i < SAMPLE_IVE_MD_IMAGE_NUM; i++) {
        s32Ret = SAMPLE_COMM_IVE_CreateImage(&pstMd->astImg[i], IVE_IMAGE_TYPE_U8C1, u32Width, u32Height);
        SAMPLE_CHECK_EXPR_GOTO(s32Ret != HI_SUCCESS, MD_INIT_FAIL, "Error(%#x),Create img[%d] image failed!\n", s32Ret,
            i);
    }
    u32Size = sizeof(IVE_CCBLOB_S);
    s32Ret = SAMPLE_COMM_IVE_CreateMemInfo(&pstMd->stBlob, u32Size);
    SAMPLE_CHECK_EXPR_GOTO(s32Ret != HI_SUCCESS, MD_INIT_FAIL, "Error(%#x),Create blob mem info failed!\n", s32Ret);

    // Set attr info
    pstMd->stMdAttr.enAlgMode = MD_ALG_MODE_BG;
    pstMd->stMdAttr.enSadMode = IVE_SAD_MODE_MB_4X4;
    pstMd->stMdAttr.enSadOutCtrl = IVE_SAD_OUT_CTRL_THRESH;
    pstMd->stMdAttr.u16SadThr = SAMPLE_IVE_MD_SAD_THR; // 100 * (1 << 2);
    pstMd->stMdAttr.u32Width = u32Width;
    pstMd->stMdAttr.u32Height = u32Height;
    pstMd->stMdAttr.stAddCtrl.u0q16X = SAMPLE_IVE_MD_X_WEIGHT; // X: 32768
    pstMd->stMdAttr.stAddCtrl.u0q16Y = SAMPLE_IVE_MD_Y_WEIGHT; // Y: 32768
    pstMd->stMdAttr.stCclCtrl.enMode = IVE_CCL_MODE_4C;
    u8WndSz = SAMPLE_IVE_MD_CAL_WIN_SIZE(u8SadMode);  // (2 + u8SadMode) bit
    pstMd->stMdAttr.stCclCtrl.u16InitAreaThr = u8WndSz * u8WndSz;
    pstMd->stMdAttr.stCclCtrl.u16Step = u8WndSz;

    s32Ret = HI_IVS_MD_Init();
    SAMPLE_CHECK_EXPR_GOTO(s32Ret != HI_SUCCESS, MD_INIT_FAIL, "Error(%#x),HI_IVS_MD_Init failed!\n", s32Ret);

MD_INIT_FAIL:

    if (s32Ret != HI_SUCCESS) {
        SAMPLE_IVE_Md_Uninit(pstMd);
    }
    return s32Ret;
}
#define IMG_FULL_CHN    3 // Full channel / three channel, for YUV444, RGB888
#define IMG_HALF_CHN    2 // Half channel, for YUV420/422
#define THREE_TIMES     3
#define TWO_TIMES       2
/*
 * 调试log等级
 * Debug log level
 */
#define HI_DLEV_NONE        0 // disable
#define HI_DLEV_ERR         1 // error
#define HI_DLEV_WARN        2 // warning
#define HI_DLEV_INFO        3 // informational
#define HI_DLEV_DBG         4 // debug normal
#define HI_DLEV_VERB        5 // debug vervose
#define HI_DLEV_BUTT        6
typedef enum AlignType {
    ALIGN_TYPE_2 = 2, // Align by 2 bytes
    ALIGN_TYPE_16 = 16, // Align by 16 bytes
    ALIGN_TYPE_32 = 32, // Align by 32 bytes
} AlignType;
int g_hiDbgLev = HI_DLEV_INFO;

int HiAlign16(int num)
{
    return (((num) + 16 - 1) / 16 * 16); // 16: Align16
}

int HiAlign32(int num)
{
    return (((num) + 32 - 1) / 32 * 32); // 32: Align32
}

/*
 * 根据类型和大小创建缓存
 * Create IVE image buffer based on type and size
 */
/*
 * 计算通道的步幅
 * Calculate the stride of a channel
 */
static uint32_t IveCalStride(IVE_IMAGE_TYPE_E enType, uint32_t width, AlignType align)
{
    uint32_t size = 1;

    switch (enType) {
        case IVE_IMAGE_TYPE_U8C1:
        case IVE_IMAGE_TYPE_S8C1:
        case IVE_IMAGE_TYPE_S8C2_PACKAGE:
        case IVE_IMAGE_TYPE_S8C2_PLANAR:
        case IVE_IMAGE_TYPE_U8C3_PACKAGE:
        case IVE_IMAGE_TYPE_U8C3_PLANAR:
            size = sizeof(HI_U8);
            break;
        case IVE_IMAGE_TYPE_S16C1:
        case IVE_IMAGE_TYPE_U16C1:
            size = sizeof(HI_U16);
            break;
        case IVE_IMAGE_TYPE_S32C1:
        case IVE_IMAGE_TYPE_U32C1:
            size = sizeof(uint32_t);
            break;
        case IVE_IMAGE_TYPE_S64C1:
        case IVE_IMAGE_TYPE_U64C1:
            size = sizeof(uint64_t);
            break;
        default:
            break;
    }

    if (align == ALIGN_TYPE_16) {
        return HiAlign16(width * size);
    } else if (align == ALIGN_TYPE_32) {
        return HiAlign32(width * size);
    } else {
        HI_ASSERT(0);
        return 0;
    }
}
int IveImgCreate(IVE_IMAGE_S* img,
    IVE_IMAGE_TYPE_E enType, uint32_t width, uint32_t height)
{
    HI_ASSERT(img);
    uint32_t oneChnSize;
    uint32_t size;
    int ret;

    if (memset_s(img, sizeof(*img), 0, sizeof(*img)) != EOK) {
        HI_ASSERT(0);
    }
    img->enType = enType;
    img->u32Width = width;
    img->u32Height = height;
    img->au32Stride[0] = IveCalStride(img->enType, img->u32Width, ALIGN_TYPE_16);

    switch (enType) {
        case IVE_IMAGE_TYPE_U8C1:
        case IVE_IMAGE_TYPE_S8C1: // Only 1 channel
            size = img->au32Stride[0] * img->u32Height;
            ret = HI_MPI_SYS_MmzAlloc(&img->au64PhyAddr[0], (void**)&img->au64VirAddr[0], NULL, NULL, size);
            SAMPLE_CHECK_EXPR_RET(HI_SUCCESS != ret, ret, "Error(%#x), HI_MPI_SYS_MmzAlloc!\n", ret);
            break;
        /*
         * 大小相当于像素的1.5倍(3/2), 相当于2个通道
         * The size is equivalent to 1.5 times (3/2) of the pixel, which is equivalent to 2 channels
         */
        case IVE_IMAGE_TYPE_YUV420SP:
        /*
         * 大小相当于像素的2倍，相当于2个通道
         * The size is equivalent to 2 times the pixel, which is equivalent to 2 channels
         */
        case IVE_IMAGE_TYPE_YUV422SP:
            if (enType == IVE_IMAGE_TYPE_YUV420SP) {
                size = img->au32Stride[0] * img->u32Height * THREE_TIMES / TWO_TIMES;
            } else {
                size = img->au32Stride[0] * img->u32Height * TWO_TIMES;
            }
            ret = HI_MPI_SYS_MmzAlloc(&img->au64PhyAddr[0], (void**)&img->au64VirAddr[0], NULL, NULL, size);
            SAMPLE_CHECK_EXPR_RET(HI_SUCCESS != ret, ret, "Error(%#x), HI_MPI_SYS_MmzAlloc!\n", ret);

            /*
             * 设置通道1地址的步长，两者都需要通道1
             * Set the stride of the address of channel 1, both of which require channel 1
             */
            img->au32Stride[1] = img->au32Stride[0];
            img->au64PhyAddr[1] = img->au64PhyAddr[0] + img->au32Stride[0] * (uint64_t)img->u32Height;
            img->au64VirAddr[1] = img->au64VirAddr[0] + img->au32Stride[0] * (uint64_t)img->u32Height;
            break;

        case IVE_IMAGE_TYPE_U8C3_PLANAR: // 3 channels, often used for RGB
            oneChnSize = img->au32Stride[0] * img->u32Height;
            size = oneChnSize * 3; // 3 channels have the same size
            ret = HI_MPI_SYS_MmzAlloc(&img->au64PhyAddr[0], (void**)&img->au64VirAddr[0], NULL, NULL, size);
            SAMPLE_CHECK_EXPR_RET(HI_SUCCESS != ret, ret, "Error(%#x), HI_MPI_SYS_MmzAlloc!\n", ret);

            /*
             * 设置通道1和通道2的地址和步长
             * Set the address and stride of channel 1 and channel 2
             */
            img->au64VirAddr[1] = img->au64VirAddr[0] + oneChnSize;
            img->au64PhyAddr[1] = img->au64PhyAddr[0] + oneChnSize;
            img->au32Stride[1] = img->au32Stride[0];
            img->au64VirAddr[2] = img->au64VirAddr[1] + oneChnSize; // 2: au64VirAddr array subscript, not out of bounds
            img->au64PhyAddr[2] = img->au64PhyAddr[1] + oneChnSize; // 2: au64VirAddr array subscript, not out of bounds
            img->au32Stride[2] = img->au32Stride[0]; // 2: au64VirAddr array subscript, not out of bounds
            break;

        /*
         * 目前如下格式不支持，主要为YVC420P, YUV422P, S8C2_PACKAGE, S8C2_PLANAR,
         * S32C1, U32C1, S64C1, U64C1, S16C1, U16C1, U8C3_PACKAGE等
         *
         * Types not currently supported: YVC420P, YUV422P, S8C2_PACKAGE, S8C2_PLANAR,
         * S32C1, U32C1, S64C1, U64C1, S16C1, U16C1, U8C3_PACKAGE,etc.
         */
        default:
            HI_ASSERT(0);
            break;
    }
    return HI_SUCCESS;
}

/*
 * 销毁IVE image
 * Destory IVE image
 */
void IveImgDestroy(IVE_IMAGE_S* img)
{
    for (int i = 0; i < IMG_FULL_CHN; i++) {
        if (img->au64PhyAddr[0] && img->au64VirAddr[0]) {
            HI_MPI_SYS_MmzFree(img->au64PhyAddr[i], (void*)((uintptr_t)img->au64VirAddr[i]));
            img->au64PhyAddr[i] = 0;
            img->au64VirAddr[i] = 0;
        }
    }
    if (memset_s(img, sizeof(*img), 0, sizeof(*img)) != EOK) {
        HI_ASSERT(0);
    }
}

int OrigImgToFrm(const IVE_IMAGE_S *img, VIDEO_FRAME_INFO_S* frm)
{
    static const int chnNum = 2;
    IVE_IMAGE_TYPE_E enType = img->enType;
    if (memset_s(frm, sizeof(*frm), 0, sizeof(*frm)) != EOK) {
        HI_ASSERT(0);
    }

    frm->stVFrame.u32Width = img->u32Width;
    frm->stVFrame.u32Height = img->u32Height;

    if (enType == IVE_IMAGE_TYPE_YUV420SP) {
        frm->stVFrame.enPixelFormat = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    } else if (enType == IVE_IMAGE_TYPE_YUV422SP) {
        frm->stVFrame.enPixelFormat = PIXEL_FORMAT_YVU_SEMIPLANAR_422;
    } else {
        HI_ASSERT(0);
        return -1;
    }

    for (int i = 0; i < chnNum; i++) {
        frm->stVFrame.u64PhyAddr[i] = img->au64PhyAddr[i];
        frm->stVFrame.u64VirAddr[i] = img->au64VirAddr[i];
        frm->stVFrame.u32Stride[i] = img->au32Stride[i];
    }
    return 0;
}

/*
 * YUV VIDEO_FRAME_INFO_S格式转成RGB IVE_DST_IMAGE_S格式
 * YUV video frame to RGB IVE image
 */
int FrmToRgbImg(VIDEO_FRAME_INFO_S* srcFrm, IVE_DST_IMAGE_S *dstImg)
{
    HI_ASSERT(srcFrm && dstImg);
    const static int chnNum = 3;
    IVE_HANDLE iveHnd;
    IVE_SRC_IMAGE_S srcImg;
    HI_BOOL finish;
    int ret;

    if (memset_s(&srcImg, sizeof(srcImg), 0, sizeof(srcImg)) != EOK) {
        HI_ASSERT(0);
    }
    srcImg.u32Width = srcFrm->stVFrame.u32Width;
    srcImg.u32Height = srcFrm->stVFrame.u32Height;

    PIXEL_FORMAT_E enPixelFormat = srcFrm->stVFrame.enPixelFormat;
    if (enPixelFormat == PIXEL_FORMAT_YVU_SEMIPLANAR_420) {
        srcImg.enType = IVE_IMAGE_TYPE_YUV420SP;
    } else if (enPixelFormat == PIXEL_FORMAT_YVU_SEMIPLANAR_422) {
        srcImg.enType = IVE_IMAGE_TYPE_YUV422SP;
    } else {
        HI_ASSERT(0);
        return -1;
    }

    /*
     * 分别复制3个通道的地址
     * Copy the addresses of the 3 channels separately
     */
    for (int i = 0; i < chnNum; i++) {
        srcImg.au64PhyAddr[i] = srcFrm->stVFrame.u64PhyAddr[i];
        srcImg.au64VirAddr[i] = srcFrm->stVFrame.u64VirAddr[i];
        srcImg.au32Stride[i] = srcFrm->stVFrame.u32Stride[i];
    }

    ret = IveImgCreate(dstImg, IVE_IMAGE_TYPE_U8C3_PLANAR, srcImg.u32Width, srcImg.u32Height);
    SAMPLE_CHECK_EXPR_RET(HI_SUCCESS != ret, ret, "Error(%#x), IveImgCreate failed!\n", ret);

    IVE_CSC_CTRL_S stCSCCtrl = { IVE_CSC_MODE_PIC_BT601_YUV2RGB};
    ret = HI_MPI_IVE_CSC(&iveHnd, &srcImg, dstImg, &stCSCCtrl, HI_TRUE);
    SAMPLE_CHECK_EXPR_GOTO(HI_SUCCESS != ret, FAIL, "Error(%#x), HI_MPI_IVE_CSC failed!\n", ret);

    ret = HI_MPI_IVE_Query(iveHnd, &finish, HI_TRUE);
    SAMPLE_CHECK_EXPR_GOTO(HI_SUCCESS != ret, FAIL, "Error(%#x), HI_MPI_IVE_Query failed!\n", ret);
    return ret;

    FAIL:
        IveImgDestroy(dstImg);
        return ret;
}
int ImgRgbToYuv(IVE_IMAGE_S *src, IVE_IMAGE_S *dst, IVE_IMAGE_TYPE_E dstType)
{
    IVE_HANDLE iveHnd;
    HI_BOOL finish;
    HI_S32 ret;

    if (memset_s(dst, sizeof(*dst), 0, sizeof(*dst)) != EOK) {
        HI_ASSERT(0);
    }

    ret = IveImgCreate(dst, dstType, src->u32Width, src->u32Height);
    SAMPLE_CHECK_EXPR_RET(HI_SUCCESS != ret, ret, "Error(%#x), IveImgCreate failed!\n", ret);

    IVE_CSC_CTRL_S stCSCCtrl = { IVE_CSC_MODE_VIDEO_BT601_RGB2YUV};
    ret = HI_MPI_IVE_CSC(&iveHnd, src, dst, &stCSCCtrl, HI_TRUE);
    SAMPLE_CHECK_EXPR_RET(HI_SUCCESS != ret, ret, "Error(%#x), HI_MPI_IVE_CSC failed!\n", ret);

    ret = HI_MPI_IVE_Query(iveHnd, &finish, HI_TRUE);
    SAMPLE_CHECK_EXPR_RET(HI_SUCCESS != ret, ret, "Error(%#x), HI_MPI_IVE_Query failed!\n", ret);
    return ret;
}
static SAMPLE_SVP_NNIE_MODEL_S s_stSegnetModel ;
static SAMPLE_SVP_NNIE_PARAM_S s_stSegnetNnieParam = { 0 };
int catchone = 0;
//HI_S32* ps32CatchAddr = NULL;
HI_U8* ps8CatchAddr[3];
static HI_VOID *SAMPLE_IVE_MdProc(HI_VOID *pArgs)
{
    HI_S32 s32Ret;
    SAMPLE_IVE_MD_S *pstMd = NULL;
    MD_ATTR_S *pstMdAttr = NULL;
    VIDEO_FRAME_INFO_S stBaseFrmInfo, frm;
    VIDEO_FRAME_INFO_S stExtFrmInfo;
    HI_S32 s32VpssGrp = 0;
    HI_S32 as32VpssChn[] = {VPSS_CHN0, VPSS_CHN1};
    HI_S32 s32MilliSec = SAMPLE_IVE_FRM_TIMEOUT;
    MD_CHN MdChn = 0;
    VO_LAYER voLayer = 0;
    VO_CHN voChn = 0;
    HI_BOOL bInstant = HI_TRUE;
    HI_S32 s32CurIdx = 0;
    HI_BOOL bFirstFrm = HI_TRUE;

    HI_U32 i = 0, j = 0, k = 0, n = 0;
    HI_U32 u32Height = 0, u32Width = 0, u32Chn = 0, u32Stride = 0, u32Dim = 0;
    int num = 0;

    VB_POOL ourpool;
    VB_BLK ourblock;
    HI_U64 ourPhyAddr;
    HI_U8* ourVirAddr, *ExtVirAddr;
    HI_U32 ourWidth = 1200, ourHeight = 720, ourChn = 2, ourStride = 1200;
    HI_U32 inputWidth = 600, inputHeight = 360, inputChn = 3, FrmStride = 608;

    HI_CHAR *pcModelName = "./data/nnie_model/ournet/OURS_inst.wk";
    HI_U32 u32PicNum = 1;
    SAMPLE_SVP_NNIE_CFG_S   stNnieCfg = {0};
    SAMPLE_SVP_NNIE_INPUT_DATA_INDEX_S stInputDataIdx = {0};
    SAMPLE_SVP_NNIE_PROCESS_SEG_INDEX_S stProcSegIdx = {0};
    VB_POOL_CONFIG_S ourVBconfig;

    pstMd = (SAMPLE_IVE_MD_S *)(pArgs);
    pstMdAttr = &(pstMd->stMdAttr);
    // Create chn
    s32Ret = HI_IVS_MD_CreateChn(MdChn, &(pstMd->stMdAttr));
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("HI_IVS_MD_CreateChn fail,Error(%#x)\n", s32Ret);
        return NULL;
    }
    printf("Version: 2023/8/23/16:28 input: W: 600 H: 360\n");
    (hi_void)memset_s(&ourVBconfig, sizeof(VB_POOL_CONFIG_S), 0, sizeof(VB_POOL_CONFIG_S));
    ourVBconfig.u32BlkCnt = 1;
    ourVBconfig.u64BlkSize = ourWidth * ourHeight * 3 / 2;
    ourVBconfig.enRemapMode = VB_REMAP_MODE_NONE;
    ourpool = HI_MPI_VB_CreatePool(&ourVBconfig);
    printf("Ourpool = %u\n", ourpool);
    if (ourpool == VB_INVALID_POOLID) {
        SAMPLE_PRT("HI_MPI_VB_CreatePool failed!\n");
        printf("Create pool error\n");
    }
    ourblock = HI_MPI_VB_GetBlock(ourpool, ourWidth * ourHeight * 3 / 2, "ourspace");
    ourPhyAddr = HI_MPI_VB_Handle2PhysAddr(ourblock);
    ourVirAddr = (HI_U8 *)HI_MPI_SYS_Mmap(ourPhyAddr, ourWidth * ourHeight * 3 / 2);
    //VB_POOL poolid= HI_MPI_VB_Handle2PoolId(ourblock); //poolid = ourpool
    //printf("Poolid = %u\n", poolid);
    frm.stVFrame.enPixelFormat = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    frm.stVFrame.u64PhyAddr[0] = ourPhyAddr;
    frm.stVFrame.u64PhyAddr[1] = ourPhyAddr + ourWidth * ourHeight;
    frm.stVFrame.u64VirAddr[0] = (HI_U64)(HI_UINTPTR_T)ourVirAddr;
    frm.stVFrame.u64VirAddr[1] = frm.stVFrame.u64VirAddr[0] + ourWidth * ourHeight;
    frm.stVFrame.enField = VIDEO_FIELD_FRAME;
    frm.u32PoolId = ourpool;
    frm.stVFrame.u32Width = ourWidth;
    frm.stVFrame.u32Height = ourHeight;
    frm.stVFrame.u32Stride[0] = ourStride;
    frm.stVFrame.u32Stride[1] = ourStride;
    //frm.stVFrame.u32Stride[2] = ourStride;
    //printf("Our Transform\n");
    /*Set configuration parameter*/
	//stNnieCfg.pszPic= pcSrcFile;
	stNnieCfg.u32MaxInputNum = u32PicNum; //max input image num in each batch
	stNnieCfg.u32MaxRoiNum = 0;
	stNnieCfg.aenNnieCoreId[0] = SVP_NNIE_ID_0;
	
    /*Sys init*/
    //SAMPLE_COMM_SVP_CheckSysInit();
    //printf("We have checked system initiation\n");
    /*Ournet Load model*/
    SAMPLE_SVP_TRACE_INFO("Ournet Loading model!\n");

    s32Ret = SAMPLE_COMM_SVP_NNIE_LoadModel(pcModelName, &s_stSegnetModel);
    SAMPLE_SVP_CHECK_EXPR_GOTO(HI_SUCCESS != s32Ret, EXT_RELEASE, SAMPLE_SVP_ERR_LEVEL_ERROR,
        "Error, SAMPLE_COMM_SVP_NNIE_LoadModel failed!\n");
    printf("We have loaded model\n");
    /*Segnet parameter initialization*/
    SAMPLE_SVP_TRACE_INFO("Ournet parameter initializing!\n");
    s_stSegnetNnieParam.pstModel = &s_stSegnetModel.stModel;

	s32Ret = SAMPLE_SVP_NNIE_Ournet_ParamInit(&stNnieCfg,&s_stSegnetNnieParam,NULL);
    SAMPLE_SVP_CHECK_EXPR_GOTO(HI_SUCCESS != s32Ret, EXT_RELEASE, SAMPLE_SVP_ERR_LEVEL_ERROR,
        "Error, SAMPLE_SVP_NNIE_Ournet_ParamInit failed!\n");
	printf("We have paramInit\n");	
    while (HI_FALSE == s_bStopSignal) {
        s32Ret = HI_MPI_VPSS_GetChnFrame(s32VpssGrp, as32VpssChn[1], &stExtFrmInfo, s32MilliSec);
        if (s32Ret != HI_SUCCESS) {
            SAMPLE_PRT("Error(%#x),HI_MPI_VPSS_GetChnFrame failed, VPSS_GRP(%d), VPSS_CHN(%d)!\n", s32Ret, s32VpssGrp,
                as32VpssChn[1]);
            continue;
        }
        FrmStride = stExtFrmInfo.stVFrame.u32Stride[0];
        ExtVirAddr = (HI_U8 *)HI_MPI_SYS_Mmap(stExtFrmInfo.stVFrame.u64PhyAddr[0], FrmStride * inputHeight * 3 / 2);        
        printf("Ext Transform\n");

        s32Ret = HI_MPI_VPSS_GetChnFrame(s32VpssGrp, as32VpssChn[0], &stBaseFrmInfo, s32MilliSec);
        SAMPLE_CHECK_EXPR_GOTO(s32Ret != HI_SUCCESS, EXT_RELEASE,
            "Error(%#x),HI_MPI_VPSS_GetChnFrame failed, VPSS_GRP(%d), VPSS_CHN(%d)!\n", s32Ret, s32VpssGrp,
            as32VpssChn[0]);
        printf("SRCVIDEO_FRAME_INFO_S====width:%d, height:%d, entype:%d, stride:%u, %u, %u\n",
        stExtFrmInfo.stVFrame.u32Width, stExtFrmInfo.stVFrame.u32Height, stExtFrmInfo.stVFrame.enPixelFormat
        , stExtFrmInfo.stVFrame.u32Stride[0], stExtFrmInfo.stVFrame.u32Stride[1], stExtFrmInfo.stVFrame.u32Stride[2]);
        SAMPLE_CHECK_EXPR_GOTO(s32Ret != HI_SUCCESS, EXT_RELEASE, "frmtorgb fail,Error(%#x)\n", s32Ret);
        if (HI_TRUE != bFirstFrm) {
            u32Stride = stExtFrmInfo.stVFrame.u32Stride[0];

            /*Fill src data*/
	        stInputDataIdx.u32SegIdx = 0;
	        stInputDataIdx.u32NodeIdx = 0;
	        s32Ret = SAMPLE_SVP_NNIE_FillOurFrmData(&s_stSegnetNnieParam, &stInputDataIdx, ExtVirAddr);
            SAMPLE_SVP_CHECK_EXPR_GOTO(HI_SUCCESS != s32Ret, EXT_RELEASE, SAMPLE_SVP_ERR_LEVEL_ERROR,
			    "Error,SAMPLE_SVP_NNIE_FillSrcData failed!\n");

	        /*NNIE process(process the 0-th segment)*/ 
	        stProcSegIdx.u32SegIdx = 0;
	        s32Ret = SAMPLE_SVP_OurNNIE_Forward(&s_stSegnetNnieParam, &stInputDataIdx, &stProcSegIdx, HI_TRUE);
	        SAMPLE_SVP_CHECK_EXPR_GOTO(HI_SUCCESS != s32Ret, EXT_RELEASE, SAMPLE_SVP_ERR_LEVEL_ERROR,
			"Error,SAMPLE_SVP_NNIE_Forward failed!\n");

	        /*print report result*/    
	        s32Ret = SAMPLE_SVP_NNIE_OurPrintReportResult(&s_stSegnetNnieParam, ourVirAddr);

	        SAMPLE_SVP_CHECK_EXPR_GOTO(HI_SUCCESS != s32Ret, EXT_RELEASE, SAMPLE_SVP_ERR_LEVEL_ERROR,
			    "Error,SAMPLE_SVP_NNIE_PrintReportResult failed!\n");

            s32Ret = HI_MPI_VO_SendFrame(voLayer, voChn, &stExtFrmInfo, s32MilliSec);
            SAMPLE_CHECK_EXPR_GOTO(s32Ret != HI_SUCCESS, EXT_RELEASE, "HI_MPI_VO_SendFrame fail,Error(%#x)\n", s32Ret);
            if(s32Ret == HI_SUCCESS)
            {
                printf("send frm success\n");
            }
            s32Ret = HI_MPI_VO_SendFrame(voLayer, voChn + 1, &frm, s32MilliSec);
            SAMPLE_CHECK_EXPR_GOTO(s32Ret != HI_SUCCESS, EXT_RELEASE, "HI_MPI_VO_SendFrame fail,Error(%#x)\n", s32Ret);
            if(s32Ret == HI_SUCCESS)
            {
                printf("send frm success\n");
            }
        } else {
            //s32Ret = SAMPLE_COMM_IVE_DmaImage(&stExtFrmInfo, &pstMd->astImg[1 - s32CurIdx], bInstant);
            //SAMPLE_CHECK_EXPR_GOTO(s32Ret != HI_SUCCESS, BASE_RELEASE, "SAMPLE_COMM_IVE_DmaImage fail,Error(%#x)\n",
            //    s32Ret);

            bFirstFrm = HI_FALSE;
            goto CHANGE_IDX; // first frame just init reference frame
        }
    CHANGE_IDX:
        // Change reference and current frame index
        s32CurIdx = 1 - s32CurIdx;

    BASE_RELEASE:
        s32Ret = HI_MPI_VPSS_ReleaseChnFrame(s32VpssGrp, as32VpssChn[0], &stBaseFrmInfo);
        if (s32Ret != HI_SUCCESS) {
            SAMPLE_PRT("Error(%#x),HI_MPI_VPSS_ReleaseChnFrame failed,Grp(%d) chn(%d)!\n", s32Ret, s32VpssGrp,
                as32VpssChn[0]);
        }

    EXT_RELEASE:
        s32Ret = HI_MPI_VPSS_ReleaseChnFrame(s32VpssGrp, as32VpssChn[1], &stExtFrmInfo);
        if (s32Ret != HI_SUCCESS) {
            SAMPLE_PRT("Error(%#x),HI_MPI_VPSS_ReleaseChnFrame failed,Grp(%d) chn(%d)!\n", s32Ret, s32VpssGrp,
                as32VpssChn[1]);
        }
        HI_MPI_SYS_Munmap(ExtVirAddr, FrmStride * inputHeight * 3 / 2);
    }

    // destroy
    s32Ret = HI_IVS_MD_DestroyChn(MdChn);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("HI_IVS_MD_DestroyChn fail,Error(%#x)\n", s32Ret);
    }

    return HI_NULL;
}

static HI_S32 SAMPLE_IVE_MdPause(HI_VOID)
{
    printf("---------------press Enter key to exit!---------------\n");
    if (s_bStopSignal == HI_TRUE) {
        if (s_hMdThread != 0) {
            pthread_join(s_hMdThread, HI_NULL);
            s_hMdThread = 0;
        }
        SAMPLE_IVE_Md_Uninit(&(s_stMd));
        (HI_VOID)memset_s(&s_stMd, sizeof(s_stMd), 0, sizeof(s_stMd));

        SAMPLE_COMM_IVE_StopViVpssVencVo(&s_stViConfig, &s_stMdSwitch);
        printf("\033[0;31mprogram termination abnormally!\033[0;39m\n");
        return HI_FAILURE;
    }
    (HI_VOID)getchar();
    if (s_bStopSignal == HI_TRUE) {
        if (s_hMdThread != 0) {
            pthread_join(s_hMdThread, HI_NULL);
            s_hMdThread = 0;
        }
        SAMPLE_IVE_Md_Uninit(&(s_stMd));
        (HI_VOID)memset_s(&s_stMd, sizeof(s_stMd), 0, sizeof(s_stMd));

        SAMPLE_COMM_IVE_StopViVpssVencVo(&s_stViConfig, &s_stMdSwitch);
        printf("\033[0;31mprogram termination abnormally!\033[0;39m\n");
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}

HI_VOID SAMPLE_IVE_Md(HI_VOID)
{
    SIZE_S stSize;
    PIC_SIZE_E enSize = PIC_D1_PAL;
    HI_S32 s32Ret = HI_SUCCESS;

    (HI_VOID)memset_s(&s_stMd, sizeof(s_stMd), 0, sizeof(s_stMd));
    /* step 1: start vi vpss venc vo */
    s32Ret = SAMPLE_COMM_IVE_StartViVpssVencVo(&s_stViConfig, &s_stMdSwitch, &enSize);
    SAMPLE_CHECK_EXPR_GOTO(s32Ret != HI_SUCCESS, END_MD_0, "Error(%#x),SAMPLE_COMM_IVE_StartViVpssVencVo failed!\n",
        s32Ret);

    enSize = PIC_D1_PAL;
    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enSize, &stSize);
    SAMPLE_CHECK_EXPR_GOTO(s32Ret != HI_SUCCESS, END_MD_0, "Error(%#x),SAMPLE_COMM_SYS_GetPicSize failed!\n", s32Ret);
    /* step 2: Init Md */
    s32Ret = SAMPLE_IVE_Md_Init(&s_stMd, stSize.u32Width, stSize.u32Height);
    SAMPLE_CHECK_EXPR_GOTO(s32Ret != HI_SUCCESS, END_MD_0, " Error(%#x),SAMPLE_IVE_Md_Init failed!\n", s32Ret);
    s_bStopSignal = HI_FALSE;
    /* step 3: Create work thread */
    s32Ret = prctl(PR_SET_NAME, "IVE_MdProc", 0, 0, 0);
    SAMPLE_CHECK_EXPR_GOTO(s32Ret != HI_SUCCESS, END_MD_0, "thread set name failed!\n");
    s32Ret = pthread_create(&s_hMdThread, 0, SAMPLE_IVE_MdProc, (HI_VOID *)&s_stMd);
    SAMPLE_CHECK_EXPR_GOTO(s32Ret != HI_SUCCESS, END_MD_0, "thread create failed!\n");

    s32Ret = SAMPLE_IVE_MdPause();
    SAMPLE_CHECK_EXPR_RET_VOID(s32Ret != HI_SUCCESS, "pause failed!\n");

    s_bStopSignal = HI_TRUE;
    pthread_join(s_hMdThread, HI_NULL);
    s_hMdThread = 0;

    SAMPLE_IVE_Md_Uninit(&(s_stMd));
    (HI_VOID)memset_s(&s_stMd, sizeof(s_stMd), 0, sizeof(s_stMd));

END_MD_0:
    s_bStopSignal = HI_TRUE;
    s_hMdThread = 0;
    SAMPLE_COMM_IVE_StopViVpssVencVo(&s_stViConfig, &s_stMdSwitch);
    return;
}

/* function : Md sample signal handle */
HI_VOID SAMPLE_IVE_Md_HandleSig(HI_VOID)
{
    s_bStopSignal = HI_TRUE;
}
