/*
 * libmm-camcorder TC
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Jeongmo Yang <jm80.yang@samsung.com>
 *
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
 *
 */

/** 
* @ingroup 	Camcorder_API
* @addtogroup 	CAMCORDER Camcorder
*/

/** 
* @ingroup 	CAMCORDER Camcorder
* @addtogroup 	UTS_CAMCORDER Unit
*/

/** 
* @ingroup 	UTS_CAMCORDER Unit
* @addtogroup 	UTS_MM_CAMCORDER_COMMIT Uts_Mm_Camcorder_Commit
* @{
*/

/**
* @file		uts_mm_camcorder_commit.c
* @brief	This is a suite of unit test cases to test mm_camcorder_commit API.
* @author	Kishor Reddy (p.kishor@samsung.com)
* @version	Initial Creation V0.1
* @date		2008.11.27
*/

#include "uts_mm_camcorder_commit.h"
#include "uts_mm_camcorder_utils.h"

struct tet_testlist tet_testlist[] = {
	{utc_camcorder_commit_001,1},
	{utc_camcorder_commit_002,2},
	{NULL,0}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief This tests mm_camcorder_commit API in normal conditions
 * @par ID:
 * UTC_CAMCORDER_COMMIT_001
 * @param [in] camcorder = Valid Camcorder Handle 
 * @param [out] None
 * @pre Camcorder should be in RECORDING state.
 * @post Camcorder should come to PREPARED state.
 * @return MM_ERROR_NONE
 */	
void utc_camcorder_commit_001()
{
	tet_printf("\n ======================utc_camcorder_commit_001====================== \n");

	gint errorValue = -1;
	gboolean bReturnValue = false;
	MMHandleType camcorder = 0;
	int mode = VIDEO_MODE;
	MMCamPreset info;

	info.videodev_type = MM_VIDEO_DEVICE_CAMERA0;

	bReturnValue = StartCamcorder(&camcorder, &info, mode);
	if (true != bReturnValue) {
		tet_printf("\n Failed in utc_camcorder_commit_001 while starting the camcorder \n");
		tet_result(TET_UNTESTED);
		return;
	}

	errorValue = mm_camcorder_record(camcorder);
	if (MM_ERROR_NONE != errorValue) {
		tet_printf("\n Failed in utc_camcorder_commit_001 while recording with error %x \n", errorValue);
		tet_result(TET_UNTESTED);
		return;
	}

	sleep(5);

	errorValue = mm_camcorder_commit(camcorder);

	bReturnValue = StopCamcorder(camcorder);

	dts_check_eq( "mm_camcorder_commit", errorValue, MM_ERROR_NONE,
			"Failed in utc_camcorder_commit_001 while commiting with error %x", errorValue );

	if (true != bReturnValue) {
		tet_printf("\n Failed in utc_camcorder_commit_001 while stoping the camcorder \n");
		return;
	}

	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief This tests mm_camcorder_commit API with invalid camcorder.
 * @par ID:
 * UTC_CAMCORDER_COMMIT_002
 * @param [in] Invalid handle
 * @param [out] None
 * @return MM_ERROR_NONE
 */	
void utc_camcorder_commit_002()
{
	tet_printf("\n ======================utc_camcorder_commit_002====================== \n");

	gint errorValue = -1; 

	errorValue = mm_camcorder_commit(0);
	
	dts_check_ne( "mm_camcorder_commit", errorValue, MM_ERROR_NONE,
			"Failed in utc_camcorder_commit_002 while commiting the camcorder with error %x", errorValue );

	return;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
/** @} */
