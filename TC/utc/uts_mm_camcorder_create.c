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
* @addtogroup 	UTS_MM_CAMCORDER_CREATE Uts_Mm_Camcorder_Create
* @{
*/

/**
* @file		uts_mm_camcorder_create.c
* @brief	This is a suite of unit test cases to test mm_camcorder_create API.
* @author	Kishor Reddy (p.kishor@samsung.com)
* @version	Initial Creation V0.1
* @date		2008.11.27
*/

#include "uts_mm_camcorder_create.h"
#include "uts_mm_camcorder_utils.h"

struct tet_testlist tet_testlist[] = {
	{utc_camcorder_create_001,1},
	{utc_camcorder_create_002,2},
	{utc_camcorder_create_003,3},
	{utc_camcorder_create_004,4},
	{NULL,0}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief This tests mm_camcorder_create API in normal conditions with primary camera.
 * @par ID:
 * UTC_CAMCORDER_CREATE_001
 * @param [in] info
 * @code
 MMCamPreset info;
 info.videodev_type = MM_VIDEO_DEVICE_CAMERA0;
 * @endcode
 * @param [out] camcorder
 * @pre None.
 * @post Camcorder should come to NULL state.
 * @return MM_ERROR_NONE
 */	
void utc_camcorder_create_001()
{
	tet_printf("\n ======================utc_camcorder_create_001====================== \n");

	gint errorValue = -1;
	gint errorValue_check = -1;
	MMHandleType camcorder = 0;
	MMCamPreset info;

	info.videodev_type = MM_VIDEO_DEVICE_CAMERA0;

	errorValue_check = mm_camcorder_create(&camcorder, &info);
	errorValue = mm_camcorder_destroy(camcorder);

	dts_check_eq( "mm_camcorder_create", errorValue_check, MM_ERROR_NONE,
			"Failed in utc_camcorder_create_001 while creating the camcorder with error %x", errorValue );

	if (MM_ERROR_NONE != errorValue) {
		tet_printf("\n Failed in utc_camcorder_create_001 while destroying the camcorder with error %x but mm_camcorder_create API was passed \n", errorValue);
		return;
	}

	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 /**
 * @brief This tests mm_camcorder_create API with invalid output parameter.
 * @par ID:
 * UTC_CAMCORDER_CREATE_002
 * @param [in] info
 * @code
 MMCamPreset info;
 info.videodev_type = MM_VIDEO_DEVICE_CAMERA0;
 * @endcode
 * @param [out] pCamcorder
 * @code
MMHandleType *pCamcorder = NULL;
 * @endcode
 * @return Error code
 */	
void utc_camcorder_create_002()
{
	tet_printf("\n ======================utc_camcorder_create_002====================== \n");

	gint errorValue = -1;
	MMHandleType *pCamcorder = NULL;
	MMCamPreset info;

	info.videodev_type = MM_VIDEO_DEVICE_CAMERA0;

	errorValue = mm_camcorder_create(pCamcorder, &info);
	
	dts_check_ne( "mm_camcorder_create", errorValue, MM_ERROR_NONE,
			"Failed in utc_camcorder_create_002 while creating the camcorder with error %x", errorValue );

	return;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
 /**
 * @brief This tests mm_camcorder_create API in normal conditions with secondary camera.
 * @par ID:
 * UTC_CAMCORDER_CREATE_003
 * @param [in] info
 * @code
 MMCamPreset info;
 info.videodev_type = MM_VIDEO_DEVICE_CAMERA1;
 * @endcode
 * @param [out] camcorder
 * @pre None.
 * @post Camcorder should come to NULL state.
 * @return MM_ERROR_NONE
 */	
 void utc_camcorder_create_003()
{
	tet_printf("\n ======================utc_camcorder_create_003====================== \n");

	gint errorValue = -1;
	gint errorValue_check = -1;
	MMHandleType camcorder = 0;
	MMCamPreset info;

	info.videodev_type = MM_VIDEO_DEVICE_CAMERA1;

	errorValue_check = mm_camcorder_create(&camcorder, &info);
	errorValue = mm_camcorder_destroy(camcorder);

	dts_check_eq( "mm_camcorder_create", errorValue_check, MM_ERROR_NONE,
			"Failed in utc_camcorder_create_003 while creating the camcorder with error %x", errorValue );

	if (MM_ERROR_NONE != errorValue)  {
		tet_printf("\n Failed in utc_camcorder_create_003 while destroying the camcorder with error %x but mm_camcorder_create API was passed \n", errorValue);
		return;
	}

	return;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
 /**
 * @brief This tests mm_camcorder_create API with invalid preset information.
 * @par ID:
 * UTC_CAMCORDER_CREATE_004
 * @param [in] info
 * @code
 MMCamPreset info;
 info.videodev_type = MM_VIDEO_DEVICE_NUM;
 * @endcode
 * @param [out] camcorder
 * @pre None.
 * @post Camcorder should come to NULL state.
 * @return MM_ERROR_NONE
 */	
void utc_camcorder_create_004()
{
	tet_printf("\n ======================utc_camcorder_create_004====================== \n");

	gint errorValue = -1; 
	MMHandleType camcorder = 0;
	MMCamPreset info;

	info.videodev_type = MM_VIDEO_DEVICE_NUM;

	errorValue = mm_camcorder_create(&camcorder, &info);
	
	dts_check_ne( "mm_camcorder_create", errorValue, MM_ERROR_NONE,
			"Failed in utc_camcorder_create_004 while creating the camcorder with error %x", errorValue );

	return;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
/** @} */
