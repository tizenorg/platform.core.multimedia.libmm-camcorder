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
* @addtogroup 	UTS_MM_CAMCORDER_GET_ATTR_INFO Uts_Mm_Camcorder_Get_Attr_Info
* @{
*/

/**
* @file		uts_mm_camcorder_get_attr_info.c
* @brief	This is a suite of unit test cases to test mmcam_get_attr_info API.
* @author	Priyanka Aggarwal (priyanka.a@samsung.com)
* @version	Initial Creation V0.1
* @date		2010.06.10
*/

#include "uts_mm_camcorder_get_attr_info.h"
#include "uts_mm_camcorder_utils.h"

struct tet_testlist tet_testlist[] = {
	{utc_camcorder_get_attr_info_001,1},
	{utc_camcorder_get_attr_info_002,2},
	{utc_camcorder_get_attr_info_003,3},
	{utc_camcorder_get_attr_info_004,4},
	{NULL,0}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void utc_camcorder_get_attr_info_001()
{
	tet_printf("\n ======================utc_camcorder_get_attr_info_001====================== \n");

	gint errorValue = -1;
	MMHandleType camcorder = 0;
	gboolean bErrorValue = false;
	MMCamAttrsInfo attr_info;
	MMCamPreset info;

	info.videodev_type = MM_VIDEO_DEVICE_CAMERA0;

	bErrorValue = CreateCamcorder(&camcorder, &info);
	if (false == bErrorValue) {
		tet_printf("\n Failed in utc_camcorder_get_attr_info_001 while creating the camcorder \n");
		tet_result(TET_UNTESTED);
		return;
	}

	errorValue = mm_camcorder_get_attribute_info(camcorder, MMCAM_DISPLAY_DEVICE, &attr_info);

	bErrorValue = DestroyCamcorder(camcorder);

	dts_check_eq( "mm_camcorder_get_attribute_info", errorValue, MM_ERROR_NONE,
			"Failed in utc_camcorder_get_attr_info_001 while getting attributes of the camcorder [%x]", errorValue );

	if (false == bErrorValue) {
		tet_printf("\n Failed in utc_camcorder_get_attr_info_001 while destroying the camcorder \n");
	}

	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void utc_camcorder_get_attr_info_002()
{
	tet_printf("\n ======================utc_camcorder_get_attr_info_002====================== \n");

	gint errorValue = -1;
	MMHandleType camcorder = 0;
	//gboolean bErrorValue = false;
	MMCamAttrsInfo info;

	errorValue = mm_camcorder_get_attribute_info(camcorder, MMCAM_DISPLAY_DEVICE, &info);
	
	dts_check_ne( "mm_camcorder_get_attribute_info", errorValue, MM_ERROR_NONE,
			"Failed in utc_camcorder_get_attr_info_002 while getting attributes of the camcorder" );

	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void utc_camcorder_get_attr_info_003()
{
	tet_printf("\n ======================utc_camcorder_get_attr_info_003====================== \n");

	gint errorValue = -1;
	MMHandleType camcorder = 0;
	gboolean bErrorValue = false;
	MMCamAttrsInfo attr_info;
	MMCamPreset info;

	info.videodev_type = MM_VIDEO_DEVICE_CAMERA0;

	bErrorValue = CreateCamcorder(&camcorder, &info);
	if (false == bErrorValue) {
		tet_printf("\n Failed in utc_camcorder_get_attr_info_003 while creating the camcorder \n");
		tet_result(TET_UNTESTED);
		return;
	}

	errorValue = mm_camcorder_get_attribute_info(camcorder, NULL, &attr_info);

	bErrorValue = DestroyCamcorder(camcorder);

	dts_check_ne( "mm_camcorder_get_attribute_info", errorValue, MM_ERROR_NONE,
			"Failed in utc_camcorder_get_attr_info_003 while getting attributes of the camcorder" );

	if (false == bErrorValue) {
		tet_printf("\n Failed in utc_camcorder_get_attr_info_003 while destroying the camcorder \n");
	}

	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void utc_camcorder_get_attr_info_004()
{
	tet_printf("\n ======================utc_camcorder_get_attr_info_004====================== \n");

	gint errorValue = -1;
	MMHandleType camcorder = 0;
	gboolean bErrorValue = false;
	MMCamPreset info;

	info.videodev_type = MM_VIDEO_DEVICE_CAMERA0;

	bErrorValue = CreateCamcorder(&camcorder, &info);
	if (false == bErrorValue) {
		tet_printf("\n Failed in utc_camcorder_get_attr_info_004 while creating the camcorder \n");
		tet_result(TET_UNTESTED);
		return;
	}

	errorValue = mm_camcorder_get_attribute_info(camcorder, MMCAM_DISPLAY_DEVICE, NULL);

	bErrorValue = DestroyCamcorder(camcorder);

	dts_check_ne( "mm_camcorder_get_attribute_info", errorValue, MM_ERROR_NONE,
			"Failed in utc_camcorder_get_attr_info_004 while getting attributes of the camcorder" );

	if (false == bErrorValue) {
		tet_printf("\n Failed in utc_camcorder_get_attr_info_004 while destroying the camcorder \n");
	}

	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////


/** @} */
