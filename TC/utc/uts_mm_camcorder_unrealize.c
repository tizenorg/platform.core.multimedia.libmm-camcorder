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
* @addtogroup 	UTS_MM_CAMCORDER_UNREALIZE Uts_Mm_Camcorder_Unrealize
* @{
*/

/**
* @file		uts_mm_camcorder_unrealize.c
* @brief	This is a suite of unit test cases to test mm_camcorder_unrealize API.
* @author	Kishor Reddy (p.kishor@samsung.com)
* @version	Initial Creation V0.1
* @date		2008.11.27
*/

#include "uts_mm_camcorder_unrealize.h"
#include "uts_mm_camcorder_utils.h"

struct tet_testlist tet_testlist[] = {
	{utc_camcorder_unrealize_001,1},
	{utc_camcorder_unrealize_002,2},
	{NULL,0}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief This tests mm_camcorder_unrealize API in normal conditions.
 * @par ID:
 * UTC_CAMCORDER_UNREALIZE_001
 * @param [in] camcorder = Valid Camcorder Handle
 * @param [out] None
 * @pre Camcorder should be in READY state.
 * @post Camcorder should come to NULL state.
 * @return MM_ERROR_NONE
 */
void utc_camcorder_unrealize_001()
{
	tet_printf("\n ======================utc_camcorder_unrealize_001====================== \n");

	gint errorValue = -1;
	gboolean bReturnValue = false;
	MMHandleType camcorder = 0;
	MMCamPreset info;

	info.videodev_type = MM_VIDEO_DEVICE_CAMERA0;

	bReturnValue = CreateCamcorder(&camcorder, &info);
	if (true != bReturnValue) {
		tet_printf("\n Failed in utc_camcorder_unrealize_001 while creating the camcorder \n");
		tet_result(TET_UNTESTED);
		return;
	}

	bReturnValue = SetDefaultAttributes(camcorder,IMAGE_MODE);
	if (true != bReturnValue) {
		tet_printf("\n Failed in utc_camcorder_unrealize_001 while setting default attributes to the camcorder \n");
		tet_result(TET_UNTESTED);
		return;
	}

	errorValue = mm_camcorder_realize(camcorder);
	if (MM_ERROR_NONE != errorValue) {
		tet_printf("\n Failed in utc_camcorder_unrealize_001 while realizing the camcorder with error %x \n", errorValue);
		tet_result(TET_UNTESTED);
		return;
	}

	errorValue = mm_camcorder_start(camcorder);
	if (MM_ERROR_NONE != errorValue) {
		tet_printf("\n Failed in utc_camcorder_unrealize_001 while starting the camcorder with error %x \n", errorValue);
		tet_result(TET_UNTESTED);
		return;
	}

	sleep(2);

	errorValue = mm_camcorder_stop(camcorder);
	if (MM_ERROR_NONE != errorValue) {
		tet_printf("\n Failed in utc_camcorder_unrealize_001 while stopping the camcorder with error %x \n", errorValue);
		tet_result(TET_UNTESTED);
		return;
	}

	errorValue = mm_camcorder_unrealize(camcorder);

	bReturnValue = DestroyCamcorder(camcorder);

	dts_check_eq("mm_camcorder_unrealize", errorValue, MM_ERROR_NONE, "Failed in utc_camcorder_unrealize_001 while unrealizing the camcorder with error %x but mm_camcorder_realize API was passed \n", errorValue);

	if (true != bReturnValue) {
		tet_printf("\n Failed in utc_camcorder_unrealize_001 while destroying the camcorder \n");
		return;
	}

	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief This tests mm_camcorder_unrealize API iwith invalid handle.
 * @par ID:
 * UTC_CAMCORDER_UNREALIZE_002
 * @param [in] camcorder = Invalid Camcorder Handle
 * @param [out] None
 * @return Error code.
 */
void utc_camcorder_unrealize_002()
{
	tet_printf("\n ======================utc_camcorder_unrealize_002====================== \n");

	gint errorValue = -1;

	errorValue = mm_camcorder_unrealize(0);
	dts_check_ne("mm_camcorder_unrealize", errorValue, MM_ERROR_NONE, "Failed in utc_camcorder_unrealize_002 while unrealizing the camcorder with error %x \n", errorValue);

	return;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
/** @} */
