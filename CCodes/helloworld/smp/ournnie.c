#include "ournnie.h"
/*fill frm data for ournet5*/
static void U8DataCopy(HI_U8* DstDataBuf, HI_U8* SrcDataBuf,int CopyNum)
{
	for(int i = 0; i < CopyNum; i++)
	{
		DstDataBuf[i] = SrcDataBuf[i];
	}
}
/* function : Ournet software deinit */
static HI_S32 SAMPLE_SVP_NNIE_Ournet_SoftwareDeinit(SAMPLE_SVP_NNIE_CNN_SOFTWARE_PARAM_S *pstCnnSoftWarePara)
{
    HI_S32 s32Ret = HI_SUCCESS;
    SAMPLE_SVP_CHECK_EXPR_RET(pstCnnSoftWarePara == NULL, HI_INVALID_VALUE, SAMPLE_SVP_ERR_LEVEL_ERROR,
        "Error, pstCnnSoftWarePara can't be NULL!\n");
    if ((pstCnnSoftWarePara->stGetTopN.u64PhyAddr != 0) && (pstCnnSoftWarePara->stGetTopN.u64VirAddr != 0)) {
        SAMPLE_SVP_MMZ_FREE(pstCnnSoftWarePara->stGetTopN.u64PhyAddr, pstCnnSoftWarePara->stGetTopN.u64VirAddr);
        pstCnnSoftWarePara->stGetTopN.u64PhyAddr = 0;
        pstCnnSoftWarePara->stGetTopN.u64VirAddr = 0;
    }
    return s32Ret;
}
/* function : Cnn Deinit */
HI_S32 SAMPLE_SVP_NNIE_Ournet_Deinit(SAMPLE_SVP_NNIE_PARAM_S *pstNnieParam,
    SAMPLE_SVP_NNIE_CNN_SOFTWARE_PARAM_S *pstSoftWareParam, SAMPLE_SVP_NNIE_MODEL_S *pstNnieModel)
{
    HI_S32 s32Ret = HI_SUCCESS;
    /* hardware para deinit */
    if (pstNnieParam != NULL) {
        s32Ret = SAMPLE_COMM_SVP_NNIE_ParamDeinit(pstNnieParam);
        SAMPLE_SVP_CHECK_EXPR_TRACE(s32Ret != HI_SUCCESS, SAMPLE_SVP_ERR_LEVEL_ERROR,
            "Error,SAMPLE_COMM_SVP_NNIE_ParamDeinit failed!\n");
    }
    /* software para deinit */
    if (pstSoftWareParam != NULL) {
        s32Ret = SAMPLE_SVP_NNIE_Ournet_SoftwareDeinit(pstSoftWareParam);
        SAMPLE_SVP_CHECK_EXPR_TRACE(s32Ret != HI_SUCCESS, SAMPLE_SVP_ERR_LEVEL_ERROR,
            "Error,SAMPLE_SVP_NNIE_Cnn_SoftwareDeinit failed!\n");
    }
    /* model deinit */
    if (pstNnieModel != NULL) {
        s32Ret = SAMPLE_COMM_SVP_NNIE_UnloadModel(pstNnieModel);
        SAMPLE_SVP_CHECK_EXPR_TRACE(s32Ret != HI_SUCCESS, SAMPLE_SVP_ERR_LEVEL_ERROR,
            "Error,SAMPLE_COMM_SVP_NNIE_UnloadModel failed!\n");
    }
    return s32Ret;
}

/* function : Cnn software para init */
static HI_S32 SAMPLE_SVP_NNIE_Ournet_SoftwareParaInit(SAMPLE_SVP_NNIE_CFG_S *pstNnieCfg,
    SAMPLE_SVP_NNIE_PARAM_S *pstCnnPara, SAMPLE_SVP_NNIE_CNN_SOFTWARE_PARAM_S *pstCnnSoftWarePara)
{
    HI_U32 u32GetTopNMemSize = 0;
    HI_U32 u32GetTopNAssistBufSize = 0;
    HI_U32 u32GetTopNPerFrameSize = 0;
    HI_U32 u32TotalSize = 0;
    HI_U32 u32ClassNum = pstCnnPara->pstModel->astSeg[0].astDstNode[0].unShape.stWhc.u32Width;
    HI_U64 u64PhyAddr = 0;
    HI_U8 *pu8VirAddr = NULL;
    HI_S32 s32Ret = HI_SUCCESS;
    HI_U64 u64Tmp;

    /* get mem size */
    u64Tmp = (HI_U64)pstCnnSoftWarePara->u32TopN * sizeof(SAMPLE_SVP_NNIE_CNN_GETTOPN_UNIT_S);
    SAMPLE_SVP_CHECK_EXPR_RET(u64Tmp > SAMPLE_SVP_NNIE_MAX_MEM, HI_INVALID_VALUE, SAMPLE_SVP_ERR_LEVEL_ERROR,
        "Error,u64Tmp should be less than %u!\n", SAMPLE_SVP_NNIE_MAX_MEM);
    u32GetTopNPerFrameSize = (HI_U32)u64Tmp;

    u64Tmp = SAMPLE_SVP_NNIE_ALIGN16(u32GetTopNPerFrameSize) * (HI_U64)pstNnieCfg->u32MaxInputNum;
    SAMPLE_SVP_CHECK_EXPR_RET(u64Tmp > SAMPLE_SVP_NNIE_MAX_MEM, HI_INVALID_VALUE, SAMPLE_SVP_ERR_LEVEL_ERROR,
        "Error,u64Tmp should be less than %u!\n", SAMPLE_SVP_NNIE_MAX_MEM);
    u32GetTopNMemSize = (HI_U32)u64Tmp;

    u64Tmp = (HI_U64)u32ClassNum * sizeof(SAMPLE_SVP_NNIE_CNN_GETTOPN_UNIT_S);
    SAMPLE_SVP_CHECK_EXPR_RET(u64Tmp > SAMPLE_SVP_NNIE_MAX_MEM, HI_INVALID_VALUE, SAMPLE_SVP_ERR_LEVEL_ERROR,
        "Error,u64Tmp should be less than %u!\n", SAMPLE_SVP_NNIE_MAX_MEM);
    u32GetTopNAssistBufSize = (HI_U32)u64Tmp;

    SAMPLE_SVP_CHECK_EXPR_RET((HI_U64)u32GetTopNMemSize + u32GetTopNAssistBufSize > SAMPLE_SVP_NNIE_MAX_MEM,
        HI_INVALID_VALUE, SAMPLE_SVP_ERR_LEVEL_ERROR, "Error,total size should be less than %u!\n",
        SAMPLE_SVP_NNIE_MAX_MEM);
    u32TotalSize = u32GetTopNMemSize + u32GetTopNAssistBufSize;

    /* malloc mem */
    s32Ret =
        SAMPLE_COMM_SVP_MallocMem("SAMPLE_CNN_INIT", NULL, (HI_U64 *)&u64PhyAddr, (void **)&pu8VirAddr, u32TotalSize);
    SAMPLE_SVP_CHECK_EXPR_RET(s32Ret != HI_SUCCESS, s32Ret, SAMPLE_SVP_ERR_LEVEL_ERROR,
        "Error,Malloc memory failed!\n");
    (HI_VOID)memset_s(pu8VirAddr, u32TotalSize, 0, u32TotalSize);

    /* init GetTopn */
    pstCnnSoftWarePara->stGetTopN.u32Num = pstNnieCfg->u32MaxInputNum;
    pstCnnSoftWarePara->stGetTopN.unShape.stWhc.u32Chn = 1;
    pstCnnSoftWarePara->stGetTopN.unShape.stWhc.u32Height = 1;
    pstCnnSoftWarePara->stGetTopN.unShape.stWhc.u32Width = u32GetTopNPerFrameSize / sizeof(HI_U32);
    pstCnnSoftWarePara->stGetTopN.u32Stride = SAMPLE_SVP_NNIE_ALIGN16(u32GetTopNPerFrameSize);
    pstCnnSoftWarePara->stGetTopN.u64PhyAddr = u64PhyAddr;
    pstCnnSoftWarePara->stGetTopN.u64VirAddr = SAMPLE_SVP_NNIE_CONVERT_PTR_TO_ADDR(HI_U64, pu8VirAddr);

    /* init AssistBuf */
    pstCnnSoftWarePara->stAssistBuf.u32Size = u32GetTopNAssistBufSize;
    pstCnnSoftWarePara->stAssistBuf.u64PhyAddr = u64PhyAddr + u32GetTopNMemSize;
    pstCnnSoftWarePara->stAssistBuf.u64VirAddr =
        SAMPLE_SVP_NNIE_CONVERT_PTR_TO_ADDR(HI_U64, pu8VirAddr) + u32GetTopNMemSize;

    return s32Ret;
}
/* function : Ournet init */
HI_S32 SAMPLE_SVP_NNIE_Ournet_ParamInit(SAMPLE_SVP_NNIE_CFG_S *pstNnieCfg, SAMPLE_SVP_NNIE_PARAM_S *pstCnnPara,
    SAMPLE_SVP_NNIE_CNN_SOFTWARE_PARAM_S *pstCnnSoftWarePara)
{
    HI_S32 s32Ret = HI_SUCCESS;
    /* init hardware para */
    s32Ret = SAMPLE_COMM_SVP_NNIE_ParamInit(pstNnieCfg, pstCnnPara);
    SAMPLE_SVP_CHECK_EXPR_GOTO(s32Ret != HI_SUCCESS, INIT_FAIL_0, SAMPLE_SVP_ERR_LEVEL_ERROR,
        "Error(%#x),SAMPLE_COMM_SVP_NNIE_ParamInit failed!\n", s32Ret);

    /* init software para */
    if (pstCnnSoftWarePara != NULL) {
        s32Ret = SAMPLE_SVP_NNIE_Ournet_SoftwareParaInit(pstNnieCfg, pstCnnPara, pstCnnSoftWarePara);
        SAMPLE_SVP_CHECK_EXPR_GOTO(s32Ret != HI_SUCCESS, INIT_FAIL_0, SAMPLE_SVP_ERR_LEVEL_ERROR,
            "Error(%#x),SAMPLE_SVP_NNIE_Cnn_SoftwareParaInit failed!\n", s32Ret);
    }

    return s32Ret;
INIT_FAIL_0:
    s32Ret = SAMPLE_SVP_NNIE_Ournet_Deinit(pstCnnPara, pstCnnSoftWarePara, NULL);
    SAMPLE_SVP_CHECK_EXPR_RET(s32Ret != HI_SUCCESS, s32Ret, SAMPLE_SVP_ERR_LEVEL_ERROR,
        "Error(%#x),SAMPLE_SVP_NNIE_Cnn_Deinit failed!\n", s32Ret);
    return HI_FAILURE;
}
int test_input = 0;
HI_U8* ps8CatchAddr[3];
HI_U8* dps8CatchAddr[3];
HI_S32 SAMPLE_SVP_NNIE_FillOurFrmData(
                              SAMPLE_SVP_NNIE_PARAM_S* pstNnieParam, 
				   SAMPLE_SVP_NNIE_INPUT_DATA_INDEX_S* pstInputDataIdx,
											    HI_U8* pu8PicBufAddr)
{
	printf("---------------------------fill src data begin------------------------------------------\n\n");
    int i = 0, j = 0, k = 0, n = 0;
    HI_U32 u32Height = 0, u32Width = 0, u32Chn = 0, u32Stride = 0, u32Dim = 0;
    HI_U32 FrmHeight = 360, FrmWidth = 600, FrmStride = 608;
    //HI_U8 * input_data = (HI_U8 *)malloc(3 * 240 * 400);
    HI_U32 u32VarSize = 0;
    //HI_S32 s32Ret = HI_SUCCESS;
    HI_U8* pu8PicAddr = NULL;
    HI_U8* pu8PicBufAddrcp = NULL;
    //HI_U32*pu32StepAddr = NULL;
    HI_U32 u32SegIdx = pstInputDataIdx->u32SegIdx;
    HI_U32 u32NodeIdx = pstInputDataIdx->u32NodeIdx;
    HI_U8 Y, U, V;
    int num = 0;
	printf("-----------------------get data size start----------------------------------------------\n\n");
    if(SVP_BLOB_TYPE_U8 <= pstNnieParam->astSegData[u32SegIdx].astSrc[u32NodeIdx].enType &&
        SVP_BLOB_TYPE_YVU422SP >= pstNnieParam->astSegData[u32SegIdx].astSrc[u32NodeIdx].enType)
    {	
		// 进入此分支
        printf("branch 1, data = U8\n");
        u32VarSize = sizeof(HI_U8);//数据为U8
        printf("The Data Size(u32VarSize) is: %u\n", u32VarSize);
    }
    else
    {
        printf("branch 2, data = U32\n");
        u32VarSize = sizeof(HI_U32);
        printf("u32VarSize: %u\n", u32VarSize);
    }
	printf("-----------------------get data size over---------------------------------------------- \n\n");

    /*fill src data*/
	printf("-----------------------Fill Src Data Start---------------------------------------------n\n");
    if(SVP_BLOB_TYPE_SEQ_S32 == pstNnieParam->astSegData[u32SegIdx].astSrc[u32NodeIdx].enType)
    {
		
         printf("=====================SVP_BLOB_TYPE_SEQ_S32====================\n");
        // u32Dim = pstNnieParam->astSegData[u32SegIdx].astSrc[u32NodeIdx].unShape.stSeq.u32Dim;
        // u32Stride = pstNnieParam->astSegData[u32SegIdx].astSrc[u32NodeIdx].u32Stride;
        // pu32StepAddr = SAMPLE_SVP_NNIE_CONVERT_64BIT_ADDR(HI_U32,pstNnieParam->astSegData[u32SegIdx].astSrc[u32NodeIdx].unShape.stSeq.u64VirAddrStep);
        // pu8PicAddr = SAMPLE_SVP_NNIE_CONVERT_64BIT_ADDR(HI_U8,pstNnieParam->astSegData[u32SegIdx].astSrc[u32NodeIdx].u64VirAddr);
					// 这是一个宏定义，作用是把一个U64的数据转换为指向U8存储单元的64Bit地址数据
        // printf("55555555555555555555555555555555555555555555555555555555555555555555555555555555\n");
        // for(n = 0; n < pstNnieParam->astSegData[u32SegIdx].astSrc[u32NodeIdx].u32Num; n++)
        // {
            // for(i = 0;i < *(pu32StepAddr+n); i++)
            // {
                
                // printf("*pu8PicAddr is %c\n", *pu8PicAddr);
                // printf("u32Dim is %u\n", u32Dim);
                // printf("u32VarSize is %u\n", u32VarSize);
               
                // s32Ret = fread(pu8PicAddr,u32Dim*u32VarSize,1,fp);
                // SAMPLE_SVP_CHECK_EXPR_GOTO(1 != s32Ret,FAIL,SAMPLE_SVP_ERR_LEVEL_ERROR,"Error,Read image file failed!\n");
                // pu8PicAddr += u32Stride;
            // }
            // u32TotalStepNum += *(pu32StepAddr+n);
        // }
        // SAMPLE_COMM_SVP_FlushCache(pstNnieParam->astSegData[u32SegIdx].astSrc[u32NodeIdx].u64PhyAddr,
            // SAMPLE_SVP_NNIE_CONVERT_64BIT_ADDR(HI_VOID,pstNnieParam->astSegData[u32SegIdx].astSrc[u32NodeIdx].u64VirAddr),
            // u32TotalStepNum*u32Stride);
		
    }
    else
    {
        printf("--------------------Get Into The 1st Level Branch----------------------------------\n\n");
        u32Height = pstNnieParam->astSegData[u32SegIdx].astSrc[u32NodeIdx].unShape.stWhc.u32Height;
        u32Width = pstNnieParam->astSegData[u32SegIdx].astSrc[u32NodeIdx].unShape.stWhc.u32Width;
        u32Chn = pstNnieParam->astSegData[u32SegIdx].astSrc[u32NodeIdx].unShape.stWhc.u32Chn;
        u32Stride = pstNnieParam->astSegData[u32SegIdx].astSrc[u32NodeIdx].u32Stride;
		
        pu8PicAddr = SAMPLE_SVP_NNIE_CONVERT_64BIT_ADDR(HI_U8,pstNnieParam->astSegData[u32SegIdx].astSrc[u32NodeIdx].u64VirAddr);	
		             /* 这是一个宏定义，作用是把一个 U64 的数据转换为指向 U8 存储单元的地址数据 */
					 /* 输入的bgr图像数据必须缓存在起始地址为 u64VirAddr 的存储区 */
        pu8PicBufAddrcp = pu8PicBufAddr;
        printf("u32Chn is %u\n", u32Chn);
        printf("u32Width is %u\n", u32Width);
        printf("u32Height is %u\n", u32Height);
        printf("u32Stride is %u\n", u32Stride);

        // pstNnieParam->astSegData[u32SegIdx].astSrc[u32NodeIdx].enType = 0x1;
        if(SVP_BLOB_TYPE_YVU420SP== pstNnieParam->astSegData[u32SegIdx].astSrc[u32NodeIdx].enType)
        {
            printf("===================SVP_BLOB_TYPE_YVU420SP=====================\n");
            printf("pstNnieParam->astSegData[u32SegIdx].astSrc[u32NodeIdx].enType is %d\n", pstNnieParam->astSegData[u32SegIdx].astSrc[u32NodeIdx].enType);
            printf("U8 = %d\n", SVP_BLOB_TYPE_U8);
            printf("YVU420SP = %d\n", SVP_BLOB_TYPE_YVU420SP);    
            printf("YVU422SP = %d\n", SVP_BLOB_TYPE_YVU422SP);           
            printf("-----------------------Copy The rbg Pic Data-------------------------------------------\n\n");
			for (n = 0; n < pstNnieParam->astSegData[u32SegIdx].astSrc[u32NodeIdx].u32Num; n++) {
                for (i = 0; i < u32Chn * u32Height / 2; i++) { /* Brightness: 1 height, Chroma: 1/2 height */
                    //ulSize = fread(pu8PicAddr, u32Width * u32VarSize, 1, fp);
                    memcpy(pu8PicAddr, pu8PicBufAddrcp, u32Stride * u32VarSize);
                    //SAMPLE_SVP_CHECK_EXPR_GOTO(ulSize != 1, FAIL, SAMPLE_SVP_ERR_LEVEL_ERROR,
                    //    "Error,Read image file failed!\n");
                    pu8PicAddr += u32Stride;
                    if(i == (u32Height - 1))
                    {
                        pu8PicBufAddrcp = pu8PicBufAddr + FrmStride * FrmHeight;
                    }
                    else
                    {
                        pu8PicBufAddrcp += FrmStride;
                    }
                }
                
            }
            //pstNnieParam->astSegData[u32SegIdx].astSrc[u32NodeIdx].u64VirAddr = pu8PicBufAddr;
			//U8DataCopy(pu8PicAddr, pu8PicBufAddr, u32Chn * u32Height * u32Width);
			printf("-----------------------Finish Copy-----------------------------------------------------\n\n");
        }
        else if(SVP_BLOB_TYPE_YVU422SP== pstNnieParam->astSegData[u32SegIdx].astSrc[u32NodeIdx].enType)
        {
             printf("======================SVP_BLOB_TYPE_YVU422SP========================\n");
        }
        else	/*-----------最终执行的分支-------------- */
        {
            printf("------------------------Get Into The U8 Branch----------------------------------\n\n");
            printf("pstNnieParam->astSegData[u32SegIdx].astSrc[u32NodeIdx].enType is %d\n", pstNnieParam->astSegData[u32SegIdx].astSrc[u32NodeIdx].enType);
            printf("U8 = %d\n", SVP_BLOB_TYPE_U8);
            printf("YVU420SP = %d\n", SVP_BLOB_TYPE_YVU420SP);    
            printf("YVU422SP = %d\n", SVP_BLOB_TYPE_YVU422SP);   
            num = 0;
            ps8CatchAddr[0] = pu8PicBufAddr;
            ps8CatchAddr[1] = pu8PicBufAddr + 1920 * 1080;
            for(i = 0; i < u32Chn; i++)
            {
                for(j = 0; j < u32Height; j++)
                {
                    for(k = 0; k < u32Width; k++)
                    {
                        Y = *(ps8CatchAddr[0] + j * FrmWidth + k);
                        V = *(ps8CatchAddr[1] + (j / 2) * FrmWidth + (k / 2) * 2);
                        U = *(ps8CatchAddr[1] + (j / 2) * FrmWidth + (k / 2) * 2 + 1);
                        if(i == 0) //R
                        {
                            pu8PicAddr[num] = Y + ((360 * (V - 128)) >> 8); 
                            //input_data[num] = pu8PicAddr[num];
                            num++;
                        }
                        else if(i == 1) //G
                        {
                            pu8PicAddr[num] = Y - (( ( 88 * (U - 128)  + 184 * (V - 128)) ) >> 8); 
                            //input_data[num] = pu8PicAddr[num];
                            num++;
                        }
                        else //B
                        {
                            pu8PicAddr[num] = Y + ((455 * (U - 128)) >> 8); 
                            //input_data[num] = pu8PicAddr[num];
                            num++;
                        }
                    }
                }
            }
            printf("total input data is %d byte\n", num);
            /*if(test_input == 0)
            {
                input_file = fopen("input_data.bin", "wb");
                fwrite(input_data, 1, 3 * 240 * 400, input_file);
                fclose(input_file);
                test_input = 1;
            }*/
            
            /*
			for(n = 0; n < pstNnieParam->astSegData[u32SegIdx].astSrc[u32NodeIdx].u32Num; n++)
            {
                for(i = 0;i < u32Chn; i++)
                {
                    for(j = 0; j < u32Height; j++)
                    {
                        
                        printf("*pu8PicAddr is %d\n", *pu8PicAddr);
                        printf("u32Width is %u\n", u32Width);
                        printf("u32VarSize is %u\n", u32VarSize);
                        
                        s32Ret = fread(pu8PicAddr,u32Width*u32VarSize,1,fp);
                        SAMPLE_SVP_CHECK_EXPR_GOTO(1 != s32Ret,FAIL,SAMPLE_SVP_ERR_LEVEL_ERROR,"Error,Read image file failed!\n");
                        pu8PicAddr += u32Stride;
                    }
                }
            }
			*/
			printf("-----------------------Copy The rbg Pic Data-------------------------------------------\n\n");
			//pstNnieParam->astSegData[u32SegIdx].astSrc[u32NodeIdx].u64VirAddr = pu8PicBufAddr;
			//U8DataCopy(pu8PicAddr, pu8PicBufAddr, u32Chn * u32Height * u32Width);
			printf("-----------------------Finish Copy-----------------------------------------------------\n\n");
        }
        SAMPLE_COMM_SVP_FlushCache(pstNnieParam->astSegData[u32SegIdx].astSrc[u32NodeIdx].u64PhyAddr,
            SAMPLE_SVP_NNIE_CONVERT_64BIT_ADDR(HI_VOID, pstNnieParam->astSegData[u32SegIdx].astSrc[u32NodeIdx].u64VirAddr),
            pstNnieParam->astSegData[u32SegIdx].astSrc[u32NodeIdx].u32Num * u32Chn * u32Height * u32Stride);
    }
	printf("---------------------------fill src data over------------------------------------------\n\n");
    //fclose(fp);
    //free(input_data);
    return HI_SUCCESS;
FAIL:

    //fclose(fp);
    return HI_FAILURE;
}
/* function : NNIE Forward */
HI_S32 SAMPLE_SVP_OurNNIE_Forward(SAMPLE_SVP_NNIE_PARAM_S *pstNnieParam,
    SAMPLE_SVP_NNIE_INPUT_DATA_INDEX_S *pstInputDataIdx, SAMPLE_SVP_NNIE_PROCESS_SEG_INDEX_S *pstProcSegIdx,
    HI_BOOL bInstant)
{
    HI_S32 s32Ret = HI_SUCCESS;
    HI_U32 i, j;
    HI_BOOL bFinish = HI_FALSE;
    SVP_NNIE_HANDLE hSvpNnieHandle = 0;
    HI_U32 u32TotalStepNum = 0;

    SAMPLE_SVP_CHECK_EXPR_RET(pstProcSegIdx->u32SegIdx >= pstNnieParam->pstModel->u32NetSegNum ||
        pstInputDataIdx->u32SegIdx >= pstNnieParam->pstModel->u32NetSegNum ||
        pstNnieParam->pstModel->u32NetSegNum > SVP_NNIE_MAX_NET_SEG_NUM,
        HI_INVALID_VALUE, SAMPLE_SVP_ERR_LEVEL_ERROR,
        "Error, pstProcSegIdx->u32SegIdx(%u) and pstInputDataIdx->u32SegIdx(%u) "
        "should be less than %u, pstNnieParam->pstModel->u32NetSegNum(%u) can't be greater than %u!\n",
        pstProcSegIdx->u32SegIdx, pstInputDataIdx->u32SegIdx, pstNnieParam->pstModel->u32NetSegNum,
        pstNnieParam->pstModel->u32NetSegNum, SVP_NNIE_MAX_NET_SEG_NUM);

    SAMPLE_COMM_SVP_FlushCache(pstNnieParam->astForwardCtrl[pstProcSegIdx->u32SegIdx].stTskBuf.u64PhyAddr,
        SAMPLE_SVP_NNIE_CONVERT_64BIT_ADDR(HI_VOID,
        pstNnieParam->astForwardCtrl[pstProcSegIdx->u32SegIdx].stTskBuf.u64VirAddr),
        pstNnieParam->astForwardCtrl[pstProcSegIdx->u32SegIdx].stTskBuf.u32Size);

    for (i = 0; i < pstNnieParam->astForwardCtrl[pstProcSegIdx->u32SegIdx].u32DstNum; i++) {
        if (pstNnieParam->astSegData[pstProcSegIdx->u32SegIdx].astDst[i].enType == SVP_BLOB_TYPE_SEQ_S32) {
            for (j = 0; j < pstNnieParam->astSegData[pstProcSegIdx->u32SegIdx].astDst[i].u32Num; j++) {
                u32TotalStepNum += *(SAMPLE_SVP_NNIE_CONVERT_64BIT_ADDR(HI_U32,
                    pstNnieParam->astSegData[pstProcSegIdx->u32SegIdx].astDst[i].unShape.stSeq.u64VirAddrStep) +
                    j);
            }
            SAMPLE_COMM_SVP_FlushCache(pstNnieParam->astSegData[pstProcSegIdx->u32SegIdx].astDst[i].u64PhyAddr,
                SAMPLE_SVP_NNIE_CONVERT_64BIT_ADDR(HI_VOID,
                pstNnieParam->astSegData[pstProcSegIdx->u32SegIdx].astDst[i].u64VirAddr),
                u32TotalStepNum * pstNnieParam->astSegData[pstProcSegIdx->u32SegIdx].astDst[i].u32Stride);
        } else {
            SAMPLE_COMM_SVP_FlushCache(pstNnieParam->astSegData[pstProcSegIdx->u32SegIdx].astDst[i].u64PhyAddr,
                SAMPLE_SVP_NNIE_CONVERT_64BIT_ADDR(HI_VOID,
                pstNnieParam->astSegData[pstProcSegIdx->u32SegIdx].astDst[i].u64VirAddr),
                pstNnieParam->astSegData[pstProcSegIdx->u32SegIdx].astDst[i].u32Num *
                pstNnieParam->astSegData[pstProcSegIdx->u32SegIdx].astDst[i].unShape.stWhc.u32Chn *
                pstNnieParam->astSegData[pstProcSegIdx->u32SegIdx].astDst[i].unShape.stWhc.u32Height *
                pstNnieParam->astSegData[pstProcSegIdx->u32SegIdx].astDst[i].u32Stride);
        }
    }

    /* set input blob according to node name */
    if (pstInputDataIdx->u32SegIdx != pstProcSegIdx->u32SegIdx) {
        for (i = 0; i < pstNnieParam->pstModel->astSeg[pstProcSegIdx->u32SegIdx].u16SrcNum; i++) {
            for (j = 0; j < pstNnieParam->pstModel->astSeg[pstInputDataIdx->u32SegIdx].u16DstNum; j++) {
                if (strncmp(pstNnieParam->pstModel->astSeg[pstInputDataIdx->u32SegIdx].astDstNode[j].szName,
                    pstNnieParam->pstModel->astSeg[pstProcSegIdx->u32SegIdx].astSrcNode[i].szName,
                    SVP_NNIE_NODE_NAME_LEN) == 0) {
                    pstNnieParam->astSegData[pstProcSegIdx->u32SegIdx].astSrc[i] =
                        pstNnieParam->astSegData[pstInputDataIdx->u32SegIdx].astDst[j];
                    break;
                }
            }
            SAMPLE_SVP_CHECK_EXPR_RET((j == pstNnieParam->pstModel->astSeg[pstInputDataIdx->u32SegIdx].u16DstNum),
                HI_FAILURE, SAMPLE_SVP_ERR_LEVEL_ERROR, "Error,can't find %d-th seg's %d-th src blob!\n",
                pstProcSegIdx->u32SegIdx, i);
        }
    }

    /* NNIE_Forward */
    s32Ret = HI_MPI_SVP_NNIE_Forward(&hSvpNnieHandle, pstNnieParam->astSegData[pstProcSegIdx->u32SegIdx].astSrc,
        pstNnieParam->pstModel, pstNnieParam->astSegData[pstProcSegIdx->u32SegIdx].astDst,
        &pstNnieParam->astForwardCtrl[pstProcSegIdx->u32SegIdx], bInstant);
    SAMPLE_SVP_CHECK_EXPR_RET(s32Ret != HI_SUCCESS, s32Ret, SAMPLE_SVP_ERR_LEVEL_ERROR,
        "Error,HI_MPI_SVP_NNIE_Forward failed!\n");

    if (bInstant) {
        /* Wait NNIE finish */
        while (HI_ERR_SVP_NNIE_QUERY_TIMEOUT == (s32Ret = HI_MPI_SVP_NNIE_Query(
            pstNnieParam->astForwardCtrl[pstProcSegIdx->u32SegIdx].enNnieId, hSvpNnieHandle, &bFinish, HI_TRUE))) {
            usleep(100); /* sleep 100 micro_seconds */
            SAMPLE_SVP_TRACE(SAMPLE_SVP_ERR_LEVEL_INFO, "HI_MPI_SVP_NNIE_Query Query timeout!\n");
        }
    }
    u32TotalStepNum = 0;
    for (i = 0; i < pstNnieParam->astForwardCtrl[pstProcSegIdx->u32SegIdx].u32DstNum; i++) {
        if (SVP_BLOB_TYPE_SEQ_S32 == pstNnieParam->astSegData[pstProcSegIdx->u32SegIdx].astDst[i].enType) {
            for (j = 0; j < pstNnieParam->astSegData[pstProcSegIdx->u32SegIdx].astDst[i].u32Num; j++) {
                u32TotalStepNum += *(SAMPLE_SVP_NNIE_CONVERT_64BIT_ADDR(HI_U32,
                    pstNnieParam->astSegData[pstProcSegIdx->u32SegIdx].astDst[i].unShape.stSeq.u64VirAddrStep) +
                    j);
            }
            SAMPLE_COMM_SVP_FlushCache(pstNnieParam->astSegData[pstProcSegIdx->u32SegIdx].astDst[i].u64PhyAddr,
                SAMPLE_SVP_NNIE_CONVERT_64BIT_ADDR(HI_VOID,
                pstNnieParam->astSegData[pstProcSegIdx->u32SegIdx].astDst[i].u64VirAddr),
                u32TotalStepNum * pstNnieParam->astSegData[pstProcSegIdx->u32SegIdx].astDst[i].u32Stride);
        } else {
            SAMPLE_COMM_SVP_FlushCache(pstNnieParam->astSegData[pstProcSegIdx->u32SegIdx].astDst[i].u64PhyAddr,
                SAMPLE_SVP_NNIE_CONVERT_64BIT_ADDR(HI_VOID,
                pstNnieParam->astSegData[pstProcSegIdx->u32SegIdx].astDst[i].u64VirAddr),
                pstNnieParam->astSegData[pstProcSegIdx->u32SegIdx].astDst[i].u32Num *
                pstNnieParam->astSegData[pstProcSegIdx->u32SegIdx].astDst[i].unShape.stWhc.u32Chn *
                pstNnieParam->astSegData[pstProcSegIdx->u32SegIdx].astDst[i].unShape.stWhc.u32Height *
                pstNnieParam->astSegData[pstProcSegIdx->u32SegIdx].astDst[i].u32Stride);
        }
    }

    return s32Ret;
}
/* function : print report result */

HI_S32 SAMPLE_SVP_NNIE_OurPrintReportResult(SAMPLE_SVP_NNIE_PARAM_S *pstNnieParam, HI_U8* dstVirAddr)
{
    HI_U32 u32SegNum = pstNnieParam->pstModel->u32NetSegNum;
    HI_U32 i = 0, j = 0, k = 0, n = 0, ratio = 2;
    HI_U32 u32SegIdx = 0, u32NodeIdx = 0;
    HI_S32 s32Ret = HI_SUCCESS;
    HI_CHAR acReportFileName[SAMPLE_SVP_NNIE_REPORT_NAME_LENGTH] = {'\0'};
    HI_U32* pu32StepAddr = NULL;
    HI_S32* ps32ResultAddr = NULL;
    HI_S32* ps32ResultAddrcp = NULL;
    HI_S32* BAddr = NULL,* GAddr = NULL, *RAddr = NULL;
	HI_S32 Btmpdata, Gtmpdata, Rtmpdata, B, G, R, Y, U, V;
    HI_U32 u32Height = 0, u32Width = 0, u32Chn = 0, u32Stride = 0, u32Dim = 0;
    HI_U32 u32Heightn = 720, u32Widthn = 1200, u32Chnn = 3;
    HI_U32 ComputeStride;
	static int pic_name_idx = 0;
    int yIndex = 0;
    int uvIndex = u32Heightn * u32Widthn;
	
    printf("---------------------PrintReportResult   Start-------------------------------------\n\n");

    //printf("u32SegNum is %d\n", u32SegNum);
    //printf("pstNnieParam->pstModel->astSeg[u32SegIdx].u16DstNum is %d\n\n", pstNnieParam->pstModel->astSeg[u32SegIdx].u16DstNum);
    for(u32SegIdx = 0; u32SegIdx < u32SegNum; u32SegIdx++)
    {
        for(u32NodeIdx = 0; u32NodeIdx < pstNnieParam->pstModel->astSeg[u32SegIdx].u16DstNum; u32NodeIdx++)
        {
            if(SVP_BLOB_TYPE_SEQ_S32 == pstNnieParam->astSegData[u32SegIdx].astDst[u32NodeIdx].enType)
            {
                printf("=============================SVP_BLOB_TYPE_SEQ_S32=================================\n");
                u32Dim = pstNnieParam->astSegData[u32SegIdx].astDst[u32NodeIdx].unShape.stSeq.u32Dim;
                u32Stride = pstNnieParam->astSegData[u32SegIdx].astDst[u32NodeIdx].u32Stride;
                pu32StepAddr = SAMPLE_SVP_NNIE_CONVERT_64BIT_ADDR(HI_U32,pstNnieParam->astSegData[u32SegIdx].astDst[u32NodeIdx].unShape.stSeq.u64VirAddrStep);
                ps32ResultAddr = SAMPLE_SVP_NNIE_CONVERT_64BIT_ADDR(HI_S32,pstNnieParam->astSegData[u32SegIdx].astDst[u32NodeIdx].u64VirAddr);

            }
            else
            {
                printf("===========================   PrintReportResult  ==========================\n");
                printf("pstNnieParam->astSegData[u32SegIdx].astDst[u32NodeIdx].enType is %d\n\n", pstNnieParam->astSegData[u32SegIdx].astDst[u32NodeIdx].enType);
                u32Height = pstNnieParam->astSegData[u32SegIdx].astDst[u32NodeIdx].unShape.stWhc.u32Height;
                u32Width = pstNnieParam->astSegData[u32SegIdx].astDst[u32NodeIdx].unShape.stWhc.u32Width;
                u32Chn = pstNnieParam->astSegData[u32SegIdx].astDst[u32NodeIdx].unShape.stWhc.u32Chn;
                u32Stride = pstNnieParam->astSegData[u32SegIdx].astDst[u32NodeIdx].u32Stride;

                printf("u32Height is %u\n", u32Height);
                printf("u32Width  is %u\n", u32Width);
                printf("u32Chn    is %u\n", u32Chn);
                printf("u32Stride is %u\n\n", u32Stride);

                ps32ResultAddr = SAMPLE_SVP_NNIE_CONVERT_64BIT_ADDR(HI_S32,
                pstNnieParam->astSegData[u32SegIdx].astDst[u32NodeIdx].u64VirAddr);  
                //获取 Forward 函数输出结果的存储起始地址，准备从该地址提取输出数据
                //printf("pstNnieParam->astSegData[u32SegIdx].astDst[u32NodeIdx].u32Num is %d\n\n", pstNnieParam->astSegData[u32SegIdx].astDst[u32NodeIdx].u32Num);
                ComputeStride = ratio * ratio * u32Width * u32Height;
                for(n = 0; n < pstNnieParam->astSegData[u32SegIdx].astDst[u32NodeIdx].u32Num; n++)
                {
                    //SVP_BLOB_TYPE_YVU420SP: order: RGB; SVP_BLOB_TYPE_U8: order:BGR
                    if(SVP_BLOB_TYPE_YVU420SP== pstNnieParam->astSegData[u32SegIdx].astSrc[u32NodeIdx].enType)
                    {
                        for (int i = 0; i < u32Heightn; i++)
	                    {
		                    for (int j = 0; j < u32Widthn; j++)
		                    {
                                //k = 0;
                                //RAddr = ps32ResultAddr + (k * ratio * ratio + (i % ratio) * ratio + (j % ratio)) * u32Width * u32Height + (i / ratio) * u32Width + j / ratio;
                                //k = 1;
                                //GAddr = ps32ResultAddr + (k * ratio * ratio + (i % ratio) * ratio + (j % ratio)) * u32Width * u32Height + (i / ratio) * u32Width + j / ratio;
                                //k = 2;
                                //BAddr = ps32ResultAddr + (k * ratio * ratio + (i % ratio) * ratio + (j % ratio)) * u32Width * u32Height + (i / ratio) * u32Width + j / ratio;
                                RAddr = ps32ResultAddr + ( (i % ratio) * ratio + (j % ratio)) * u32Width * u32Height + (i / ratio) * u32Width + j / ratio;
                                GAddr = RAddr + ComputeStride;
                                BAddr = GAddr + ComputeStride;
                                
                                Btmpdata = (*(BAddr)) * 255 / 4096;
                                //Btmpdata = (*(BAddr)) >> 4;
                                if(Btmpdata > 255)
								    B = 255;
							    else if(Btmpdata < 0)
								    B = 0;
							    else
								    B = Btmpdata;
                                Gtmpdata = (*(GAddr)) * 255 / 4096;
                                //Gtmpdata = (*(GAddr)) >> 4;
                                if(Gtmpdata > 255)
								    G = 255;
							    else if(Gtmpdata < 0)
								    G = 0;
							    else
								    G = Gtmpdata;
                                Rtmpdata = (*(RAddr)) * 255 / 4096;
                                //Rtmpdata = (*(RAddr)) >> 4;
                                if(Rtmpdata > 255)
								    R = 255;
							    else if(Rtmpdata < 0)
								    R = 0;
							    else
								    R = Rtmpdata;
                                
			                    //RGB to YUV
			                    Y = ((66 * R + 129 * G + 25 * B + 128) >> 8) + 16;
			                    dstVirAddr[yIndex++] = (unsigned char)((Y < 0) ? 0 : ((Y > 255) ? 255 : Y));
			                    if (i % 2 == 0 && j % 2 == 0)
			                    {
                                    U = ((-38 * R - 74 * G + 112 * B + 128) >> 8) + 128;
			                        V = ((112 * R - 94 * G - 18 * B + 128) >> 8) + 128;
				                    dstVirAddr[uvIndex++] = (unsigned char)((V < 0) ? 0 : ((V > 255) ? 255 : V));
				                    dstVirAddr[uvIndex++] = (unsigned char)((U < 0) ? 0 : ((U > 255) ? 255 : U));
			                    }
		                    }
	                    }
                    }
                    if(SVP_BLOB_TYPE_U8== pstNnieParam->astSegData[u32SegIdx].astSrc[u32NodeIdx].enType)
                    {
                        for (int i = 0; i < u32Heightn; i++)
	                    {
		                    for (int j = 0; j < u32Widthn; j++)
		                    {
                                k = 0;
                                BAddr = ps32ResultAddr + (k * ratio * ratio + (i % ratio) * ratio + (j % ratio)) * u32Width * u32Height + (i / ratio) * u32Width + j / ratio;
                                k = 1;
                                GAddr = ps32ResultAddr + (k * ratio * ratio + (i % ratio) * ratio + (j % ratio)) * u32Width * u32Height + (i / ratio) * u32Width + j / ratio;
                                k = 2;
                                RAddr = ps32ResultAddr + (k * ratio * ratio + (i % ratio) * ratio + (j % ratio)) * u32Width * u32Height + (i / ratio) * u32Width + j / ratio;
                                if(((*(BAddr)) * 255 / 4096) > 255)
								    B = 255;
							    else if(((*(BAddr)) * 255 / 4096) < 0)
								    B = 0;
							    else
								    B = (*(BAddr)) * 255 / 4096;
                            
                                if(((*(GAddr)) * 255 / 4096) > 255)
								    G = 255;
							    else if(((*(GAddr)) * 255 / 4096) < 0)
								    G = 0;
							    else
								    G = (*(GAddr)) * 255 / 4096;

                                if(((*(RAddr)) * 255 / 4096) > 255)
								    R = 255;
							    else if(((*(RAddr)) * 255 / 4096) < 0)
								    R = 0;
							    else
								    R = (*(RAddr)) * 255 / 4096;
			
			                    //RGB to YUV
			                    Y = ((66 * R + 129 * G + 25 * B + 128) >> 8) + 16;
			                    U = ((-38 * R - 74 * G + 112 * B + 128) >> 8) + 128;
			                    V = ((112 * R - 94 * G - 18 * B + 128) >> 8) + 128;
 
			                    dstVirAddr[yIndex++] = (unsigned char)((Y < 0) ? 0 : ((Y > 255) ? 255 : Y));
			                    if (i % 2 == 0 && j % 2 == 0)
			                    {
				                    dstVirAddr[uvIndex++] = (unsigned char)((V < 0) ? 0 : ((V > 255) ? 255 : V));
				                    dstVirAddr[uvIndex++] = (unsigned char)((U < 0) ? 0 : ((U > 255) ? 255 : U));
			                    }
		                    }
	                    }
                    }
                    printf("yIndex is %d, uvIndex is %d\n", yIndex, uvIndex);
					
                }
            }
        }
    }
    printf("-------------------------------------PrintReportResult   Over-------------------------------------------\n\n");
    return HI_SUCCESS;

PRINT_FAIL:
    return HI_FAILURE;
}