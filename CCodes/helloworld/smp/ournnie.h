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
#ifndef OURNNIE_H
#define OURNNIE_H
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
#include <pthread.h>
#include <sys/prctl.h>
#include <math.h>
#include "hi_common.h"
#include "hi_comm_sys.h"
#include "hi_comm_svp.h"
#include "sample_comm.h"
#include "sample_comm_svp.h"
#include "sample_comm_nnie.h"
#include "sample_svp_nnie_software.h"
#include "sample_comm_ive.h"
#include "hi_type.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */
HI_S32 SAMPLE_SVP_NNIE_Ournet_Deinit(SAMPLE_SVP_NNIE_PARAM_S *,
    SAMPLE_SVP_NNIE_CNN_SOFTWARE_PARAM_S *, SAMPLE_SVP_NNIE_MODEL_S *);
HI_S32 SAMPLE_SVP_NNIE_Ournet_ParamInit(SAMPLE_SVP_NNIE_CFG_S *, SAMPLE_SVP_NNIE_PARAM_S *,
    SAMPLE_SVP_NNIE_CNN_SOFTWARE_PARAM_S *);
HI_S32 SAMPLE_SVP_NNIE_FillOurFrmData(SAMPLE_SVP_NNIE_PARAM_S*, SAMPLE_SVP_NNIE_INPUT_DATA_INDEX_S*, HI_U8*);
HI_S32 SAMPLE_SVP_OurNNIE_Forward(SAMPLE_SVP_NNIE_PARAM_S *,
    SAMPLE_SVP_NNIE_INPUT_DATA_INDEX_S *, SAMPLE_SVP_NNIE_PROCESS_SEG_INDEX_S *,
    HI_BOOL);

HI_S32 SAMPLE_SVP_NNIE_OurPrintReportResult(SAMPLE_SVP_NNIE_PARAM_S *, HI_U8*);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif /* SAMPLE_SVP_MAIN_H */
