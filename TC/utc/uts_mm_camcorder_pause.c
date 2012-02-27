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
* @addtogroup 	UTS_MM_CAMCORDER_PAUSE Uts_Mm_Camcorder_Pause
* @{
*/

/**
* @file		uts_mm_camcorder_pause.c
* @brief	This is a suite of unit test cases to test mm_camcorder_pause API.
* @author	Kishor Reddy (p.kishor@samsung.com)
* @version	Initial Creation V0.1
* @date		2008.11.27
*/

#include "uts_mm_camcorder_pause.h"
#include "uts_mm_camcorder_utils.h"

struct tet_testlist tet_testlist[] = {
	{utc_camcorder_pause_001,1},
	{utc_camcorder_pause_002,2},
	{NULL,0}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief This tests mm_camcorder_pause API in normal conditions.
 * @par ID:
 * UTC_CAMCORDER_PAUSE_001
 * @param [in] camcorder = Valid Camcorder Handle
 * @param [out] None
 * @pre Camcorder should be in RECORDING state.
 * @post Camcorder should come to PAUSE state.
 * @return MM_ERROR_NONE
 */
void utc_camcorder_pause_001()
{
	tet_printf("\n ======================utc_camcorder_pause_001====================== \n");

	gint errorValue = -1;
	gint errorValue_check = -1;
	gboolean bReturnValue = false;
	MMHandleType camcorder = 0;
	int mode = VIDEO_MODE;
	MMCamPreset info;

	info.videodev_type = MM_VIDEO_DEVICE_CAMERA0;

	bReturnValue = StartCamcorder(&camcorder, &info, mode);
	if (true != bReturnValue) {
		tet_printf("\n Failed in utc_camcorder_pause_001 while starting the camcorder \n");
		tet_result(TET_UNTESTED);
		return;
	}

	errorValue = mm_camcorder_record(camcorder);
	if (MM_ERROR_NONE != errorValue) {
		tet_printf("\n Failed in utc_camcorder_pause_001 while recording with error %x \n", errorValue);
		tet_result(TET_UNTESTED);
		return;
	}

	sleep(5);

	errorValue_check = mm_camcorder_pause(camcorder);

	errorValue =  mm_camcorder_cancel(camcorder);
	if (MM_ERROR_NONE != errorValue) {
		tet_printf("\n Failed in utc_camcorder_pause_001 while canceling the recording in the camcorder with error %x \n", errorValue);
		return;
	}

	bReturnValue = StopCamcorder(camcorder);

	dts_check_eq( "mm_camcorder_pause", errorValue_check, MM_ERROR_NONE,
			"Failed in utc_camcorder_pause_001 while pausing the camcorder with error %x", errorValue );

	if (true != bReturnValue) {
		tet_printf("\n Failed in utc_camcorder_pause_001 while stoping the camcorder \n");
		return;
	}

	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief This tests mm_camcorder_pause API with invalid camcorder.
 * @par ID:
 * UTC_CAMCORDER_PAUSE_002
 * @param [in] Invalid handle
 * @param [out] None
 * @return Error code
 */
void utc_camcorder_pause_002()
{
	tet_printf("\n ======================utc_camcorder_pause_002====================== \n");

	gint errorValue = -1; 

	errorValue = mm_camcorder_pause(0);
	
	dts_check_ne( "mm_camcorder_pause", errorValue, MM_ERROR_NONE,
			"Failed in utc_camcorder_pause_002 while pausing the camcorder with error %x", errorValue );

	return;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
/** @} */
