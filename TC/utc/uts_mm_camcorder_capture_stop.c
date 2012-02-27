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
* @addtogroup 	UTS_MM_CAMCORDER_CAPTURE_STOP Uts_Mm_Camcorder_Capture_Stop
* @{
*/

/**
* @file		uts_mm_camcorder_capture_stop.c
* @brief	This is a suite of unit test cases to test mm_camcorder_capture_stop API.
* @author	Kishor Reddy (p.kishor@samsung.com)
* @version	Initial Creation V0.1
* @date		2008.11.27
*/

#include "uts_mm_camcorder_capture_stop.h"
#include "uts_mm_camcorder_utils.h"


struct tet_testlist tet_testlist[] = {
	{utc_camcorder_capture_stop_001,1},
	{utc_camcorder_capture_stop_002,2},
	{NULL,0}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief This tests mm_camcorder_capture_stop API in normal conditions
 * @par ID:
 * UTC_CAMCORDER_CAPTURE_STOP_001
 * @param [in] camcorder = Valid Camcorder Handle 
 * @param [out] None
 * @pre Camcorder should be in CAPTURING state.
 * @post Camcorder should come to PREPARED state.
 * @return MM_ERROR_NONE
 */	
void utc_camcorder_capture_stop_001()
{
	tet_printf("\n ======================utc_camcorder_capture_stop_001====================== \n");

	gint errorValue = -1;
	gboolean bReturnValue = false;
	MMHandleType camcorder = 0;
	int mode = IMAGE_MODE;
	MMCamPreset info;

	info.videodev_type = MM_VIDEO_DEVICE_CAMERA0;

	bReturnValue = StartCamcorder(&camcorder, &info, mode);
	if (true != bReturnValue) {
		tet_printf("\n Failed in utc_camcorder_capture_stop_001 while starting the camcorder \n");
		tet_result(TET_UNTESTED);
       	return;
	}

	errorValue = mm_camcorder_capture_start(camcorder);
	if (MM_ERROR_NONE != errorValue) {
		tet_printf("\n Failed in utc_camcorder_capture_stop_001 while starting the capturing with error %x \n", errorValue);
		tet_result(TET_UNTESTED);
		return;
	}

	sleep(5);	

	errorValue = mm_camcorder_capture_stop(camcorder);
	
	bReturnValue = StopCamcorder(camcorder);

	dts_check_eq( "mm_camcorder_capture_stop", errorValue, MM_ERROR_NONE,
			"Failed in utc_camcorder_capture_stop_001 while stoping the capturing with error %x", errorValue );

	if (true != bReturnValue) {
		tet_printf("\n Failed in utc_camcorder_capture_stop_001 while stoping the camcorder \n");
		return ;
	}

	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief This tests mm_camcorder_capture_stop API with invalid camcorder.
 * @par ID:
 * UTC_CAMCORDER_CAPTURE_STOP_002
 * @param [in] Invalid handle
 * @param [out] None
 * @return Error code
 */	
void utc_camcorder_capture_stop_002()
{
	tet_printf("\n ======================utc_camcorder_capture_stop_002====================== \n");

	gint errorValue = -1; 

	errorValue = mm_camcorder_capture_stop(0);
	
	dts_check_ne( "mm_camcorder_capture_stop", errorValue, MM_ERROR_NONE,
			"Failed in utc_camcorder_capture_stop_002 while stoping the capturing with error %x", errorValue );

	return;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
/** @} */
