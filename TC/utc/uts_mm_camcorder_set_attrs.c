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
* @ingroup CAMCORDER
* @addtogroup UTS_CAMCORDER Unit
*/

/**
* @ingroup  UTS_CAMCORDER
* @addtogroup UTS_mm_camcorder_set_attributes Uts_Mm_CamcorderSetAttrs
* @{
*/

/**
* @file		uts_mm_camcorder_set_attrs.c
* @brief	This is a suite of unit test cases to test mm_camcorder_set_attributes API.
* @author	Kishor Reddy (p.kishor@samsung.com)
* @version	Initial Creation V0.1
* @date		2008.11.27
*/

#include "uts_mm_camcorder_set_attrs.h"
#include "uts_mm_camcorder_utils.h"
#define MM_CAMCORDER_BRIGHTNESS_LEVEL1 1
#define MM_CAMCORDER_BRIGHTNESS_LEVEL9 9

struct tet_testlist tet_testlist[] = {
	{utc_camcorder_set_attrs_001,1},
	{utc_camcorder_set_attrs_002,2},
	{utc_camcorder_set_attrs_003,3},
	{utc_camcorder_set_attrs_004,4},
	{utc_camcorder_set_attrs_005,5},
	{utc_camcorder_set_attrs_006,6},
	{utc_camcorder_set_attrs_007,7},
	{utc_camcorder_set_attrs_008,8},
	{utc_camcorder_set_attrs_009,9},
	{NULL,0}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void utc_camcorder_set_attrs_001()
{
	tet_printf("\n ======================utc_camcorder_set_attrs_001====================== \n");

	gint errorValue = -1;
	MMHandleType camcorder = 0;
	gboolean bErrorValue = false;
	int AttrsValue = MM_CAMCORDER_MODE_VIDEO;
	char* err_attr_name = (char*)malloc(50);
	MMCamPreset info;

	info.videodev_type = MM_VIDEO_DEVICE_CAMERA0;

	bErrorValue = CreateCamcorder(&camcorder, &info);
	if (false == bErrorValue) {
		tet_printf("\n Failed in utc_camcorder_set_attrs_001 while creating the camcorder \n");
		tet_result(TET_UNTESTED);
		return;
	}

	errorValue = mm_camcorder_set_attributes(camcorder, &err_attr_name, MMCAM_MODE, AttrsValue, NULL);

	bErrorValue = DestroyCamcorder(camcorder);

	dts_check_eq("mm_camcorder_set_attributes", errorValue, MM_ERROR_NONE, "Failed in utc_camcorder_set_attrs_001 while setting attributes of the camcorder \n");
	tet_printf("\n Passed in utc_camcorder_set_attrs_001 while setting attributes of the camcorder \n");

	if (false == bErrorValue) {
		tet_printf("\n Failed in utc_camcorder_set_attrs_001 while destroying the camcorder \n");
	}

	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void utc_camcorder_set_attrs_002()
{
	tet_printf("\n ======================utc_camcorder_set_attrs_002====================== \n");

	gint errorValue = -1;
	MMHandleType camcorder = 0;
	gboolean bErrorValue = false;
	int AttrsValue = DISPLAY_ID;
	char* err_attr_name = (char*)malloc(50);
	MMCamPreset info;

	info.videodev_type = MM_VIDEO_DEVICE_CAMERA0;

	bErrorValue = CreateCamcorder(&camcorder, &info);
	if (false == bErrorValue) {
		tet_printf("\n Failed in utc_camcorder_set_attrs_002 while creating the camcorder \n");
		tet_result(TET_UNTESTED);
		return;
	}

	errorValue = mm_camcorder_set_attributes(camcorder, &err_attr_name, MMCAM_DISPLAY_SURFACE, AttrsValue, NULL);

	bErrorValue = DestroyCamcorder(camcorder);

	dts_check_eq("mm_camcorder_set_attributes", errorValue, MM_ERROR_NONE, "Failed in utc_camcorder_set_attrs_002 while setting attributes of the camcorder \n");
	tet_printf("\n Passed in utc_camcorder_set_attrs_002 while setting attributes of the camcorder \n");

	if (false == bErrorValue) {
		tet_printf("\n Failed in utc_camcorder_set_attrs_002 while destroying the camcorder \n");
	}

	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void utc_camcorder_set_attrs_003()
{
	tet_printf("\n ======================utc_camcorder_set_attrs_003====================== \n");

	gint errorValue = -1;
	MMHandleType camcorder = 0;
	gboolean bErrorValue = false;
	int AttrsValue = IMAGE_CAPTURE_COUNT_STILL;
	char* err_attr_name = (char*)malloc(50);
	MMCamPreset info;

	info.videodev_type = MM_VIDEO_DEVICE_CAMERA0;

	bErrorValue = CreateCamcorder(&camcorder, &info);
	if (false == bErrorValue) {
		tet_printf("\n Failed in utc_camcorder_set_attrs_003 while creating the camcorder \n");
		tet_result(TET_UNTESTED);
		return;
	}

	errorValue = mm_camcorder_set_attributes(camcorder, &err_attr_name, MMCAM_CAPTURE_COUNT, AttrsValue, NULL);

	bErrorValue = DestroyCamcorder(camcorder);

	dts_check_eq("mm_camcorder_set_attributes", errorValue, MM_ERROR_NONE, "Failed in utc_camcorder_set_attrs_003 while setting attributes of the camcorder \n");
	tet_printf("\n Passed in utc_camcorder_set_attrs_003 while setting attributes of the camcorder \n");

	if (false == bErrorValue) {
		tet_printf("\n Failed in utc_camcorder_set_attrs_003 while destroying the camcorder \n");
	}

	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void utc_camcorder_set_attrs_004()
{
	tet_printf("\n ======================utc_camcorder_set_attrs_004====================== \n");

	gint errorValue = -1;
	MMHandleType camcorder = 0;
	gboolean bErrorValue = false;
	int width = SRC_W_320;
	int height = SRC_H_240;
	char* err_attr_name = (char*)malloc(50);
	MMCamPreset info;

	info.videodev_type = MM_VIDEO_DEVICE_CAMERA0;

	bErrorValue = CreateCamcorder(&camcorder, &info);
	if (false == bErrorValue) {
		tet_printf("\n Failed in utc_camcorder_set_attrs_004 while creating the camcorder \n");
		tet_result(TET_UNTESTED);
		return;
	}

	errorValue = mm_camcorder_set_attributes(camcorder, &err_attr_name,
	                                         MMCAM_CAMERA_WIDTH, width,
	                                         MMCAM_CAMERA_HEIGHT, height,
	                                         NULL);

	bErrorValue = DestroyCamcorder(camcorder);

	dts_check_eq("mm_camcorder_set_attributes", errorValue, MM_ERROR_NONE, "Failed in utc_camcorder_set_attrs_004 while setting attributes of the camcorder \n");
	tet_printf("\n Passed in utc_camcorder_set_attrs_004 while setting attributes of the camcorder \n");

	if (false == bErrorValue) {
		tet_printf("\n Failed in utc_camcorder_set_attrs_004 while destroying the camcorder \n");
	}

	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void utc_camcorder_set_attrs_005()
{
	tet_printf("\n ======================utc_camcorder_set_attrs_005====================== \n");

	gint errorValue = -1;
	MMHandleType camcorder = 0;
	//gboolean bErrorValue = false;
	int AttrsValue = MM_CAMCORDER_MODE_VIDEO;
	char* err_attr_name = (char*)malloc(50);
	MMCamPreset info;

	info.videodev_type = MM_VIDEO_DEVICE_CAMERA0;

	/*
	bErrorValue = CreateCamcorder(&camcorder, &info);
	if (false == bErrorValue) {
		tet_printf("\n Failed in utc_camcorder_set_attrs_005 while creating the camcorder \n");
		tet_result(TET_UNTESTED);
		return;
	}
	*/

	errorValue = mm_camcorder_set_attributes(camcorder, &err_attr_name, MMCAM_MODE, AttrsValue, NULL);
	dts_check_ne("mm_camcorder_set_attributes", errorValue, MM_ERROR_NONE, "Failed in utc_camcorder_set_attrs_005 while setting attributes of the camcorder \n");
	tet_printf("\n Passed in utc_camcorder_set_attrs_005 while setting attributes of the camcorder \n");

	/*
	bErrorValue = DestroyCamcorder(camcorder);
	if (false == bErrorValue) {
		tet_printf("\n Failed in utc_camcorder_set_attrs_005 while destroying the camcorder \n");
	}
	*/

	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void utc_camcorder_set_attrs_006()
{
	tet_printf("\n ======================utc_camcorder_set_attrs_006====================== \n");

	gint errorValue = -1;
	MMHandleType camcorder = 0;
	gboolean bErrorValue = false;
	int AttrsValue = MM_CAMCORDER_MODE_VIDEO;
	//char* err_attr_name = (char*)malloc(50);
	MMCamPreset info;

	info.videodev_type = MM_VIDEO_DEVICE_CAMERA0;

	bErrorValue = CreateCamcorder(&camcorder, &info);
	if (false == bErrorValue) {
		tet_printf("\n Failed in utc_camcorder_set_attrs_006 while creating the camcorder \n");
		tet_result(TET_UNTESTED);
		return;
	}

	errorValue = mm_camcorder_set_attributes(camcorder, NULL, MMCAM_MODE, AttrsValue, NULL);

	bErrorValue = DestroyCamcorder(camcorder);

	dts_check_eq("mm_camcorder_set_attributes", errorValue, MM_ERROR_NONE, "Failed in utc_camcorder_set_attrs_006 while setting attributes of the camcorder \n");
	tet_printf("\n Passed in utc_camcorder_set_attrs_006 while setting attributes of the camcorder \n");

	if (false == bErrorValue) {
		tet_printf("\n Failed in utc_camcorder_set_attrs_006 while destroying the camcorder \n");
	}

	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void utc_camcorder_set_attrs_007()
{
	tet_printf("\n ======================utc_camcorder_set_attrs_007====================== \n");

	gint errorValue = -1;
	MMHandleType camcorder = 0;
	gboolean bErrorValue = false;
	int AttrsValue = 1;
	char* err_attr_name = (char*)malloc(50);
	MMCamPreset info;

	info.videodev_type = MM_VIDEO_DEVICE_CAMERA0;

	bErrorValue = CreateCamcorder(&camcorder, &info);
	if (false == bErrorValue) {
		tet_printf("\n Failed in utc_camcorder_set_attrs_007 while creating the camcorder \n");
		tet_result(TET_UNTESTED);
		return;
	}

	errorValue = mm_camcorder_set_attributes(camcorder, &err_attr_name, NULL, AttrsValue, NULL);

	bErrorValue = DestroyCamcorder(camcorder);

	dts_check_ne("mm_camcorder_set_attributes", errorValue, MM_ERROR_NONE, "Failed in utc_camcorder_set_attrs_007 while setting attributes of the camcorder \n");
	tet_printf("\n Passed in utc_camcorder_set_attrs_007 while setting attributes of the camcorder \n");

	if (false == bErrorValue) {
		tet_printf("\n Failed in utc_camcorder_set_attrs_007 while destroying the camcorder \n");
	}

	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void utc_camcorder_set_attrs_008()
{
	tet_printf("\n ======================utc_camcorder_set_attrs_008====================== \n");

	gint errorValue = -1;
	MMHandleType camcorder = 0;
	gboolean bErrorValue = false;
	//int AttrsValue = -1;
	char* err_attr_name = (char*)malloc(50);
	MMCamPreset info;

	info.videodev_type = MM_VIDEO_DEVICE_CAMERA0;

	bErrorValue = CreateCamcorder(&camcorder, &info);
	if (false == bErrorValue) {
		tet_printf("\n Failed in utc_camcorder_set_attrs_008 while creating the camcorder \n");
		tet_result(TET_UNTESTED);
		return;
	}

	errorValue = mm_camcorder_set_attributes(camcorder, &err_attr_name, MMCAM_MODE, -1, NULL);

	bErrorValue = DestroyCamcorder(camcorder);

	dts_check_ne("mm_camcorder_set_attributes", errorValue, MM_ERROR_NONE, "Failed in utc_camcorder_set_attrs_008 while setting attributes of the camcorder \n");
	tet_printf("\n Passed in utc_camcorder_set_attrs_008 while setting attributes of the camcorder \n");

	if (false == bErrorValue) {
		tet_printf("\n Failed in utc_camcorder_set_attrs_008 while destroying the camcorder \n");
	}

	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void utc_camcorder_set_attrs_009()
{
	tet_printf("\n ======================utc_camcorder_set_attrs_009====================== \n");

	gint errorValue = -1;
	MMHandleType camcorder = 0;
	gboolean bErrorValue = false;
	//int AttrsValue = -1;
	char* err_attr_name = (char*)malloc(50);
	MMCamPreset info;

	info.videodev_type = MM_VIDEO_DEVICE_CAMERA0;

	bErrorValue = CreateCamcorder(&camcorder, &info);
	if (false == bErrorValue) {
		tet_printf("\n Failed in utc_camcorder_set_attrs_009 while creating the camcorder \n");
		tet_result(TET_UNTESTED);
		return;
	}

	errorValue = mm_camcorder_set_attributes(camcorder, &err_attr_name, MMCAM_VIDEO_ENCODER, MM_VIDEO_CODEC_MPEG4, NULL);

	bErrorValue = DestroyCamcorder(camcorder);

	dts_check_eq("mm_camcorder_set_attributes", errorValue, MM_ERROR_NONE, "Failed in utc_camcorder_set_attrs_009 while setting attributes of the camcorder \n");
	tet_printf("\n Passed in utc_camcorder_set_attrs_009 while setting attributes of the camcorder \n");

	if (false == bErrorValue) {
		tet_printf("\n Failed in utc_camcorder_set_attrs_009 while destroying the camcorder \n");
	}

	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/** @} */
