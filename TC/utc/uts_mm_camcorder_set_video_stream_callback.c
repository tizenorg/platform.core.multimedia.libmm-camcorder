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
* @addtogroup 	UTS_MM_CAMCORDER_SET_VIDEO_STREAM_CALLBACK Uts_Mm_Camcorder_Set_Video_Stream_Callback
* @{
*/


/**
* @file		uts_mm_camcorder_set_video_stream_callback.c
* @brief	This is a suite of unit test cases to test mm_camcorder_set_video_stream_callback API.
* @author	Kishor Reddy (p.kishor@samsung.com)
* @version	Initial Creation V0.1
* @date		2008.11.27
*/

#include "uts_mm_camcorder_set_video_stream_callback.h"
#include "uts_mm_camcorder_utils.h"

struct tet_testlist tet_testlist[] = {
	{utc_camcorder_set_video_stream_callback_001,1},
	{utc_camcorder_set_video_stream_callback_002,2},
	{utc_camcorder_set_video_stream_callback_003,3},
	{utc_camcorder_set_video_stream_callback_004,4},
	{utc_camcorder_set_video_stream_callback_005,5},
	{utc_camcorder_set_video_stream_callback_006,6},
	{utc_camcorder_set_video_stream_callback_007,7},
	{utc_camcorder_set_video_stream_callback_008,8},
	{NULL,0}
};
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief This tests mm_camcorder_set_video_stream_callback API in normal conditions.
 * @par ID:
 * UTC_CAMCORDER_SET_VIDEO_STREAM_CALLBACK_001
 * @param [in] camcorder = Valid Camcorder Handle
 * @param [in] VideoStreamCallbackFunctionCalledByCamcorder (This itself is a function)
 * @param [in] gCamcorderCallbackParameters
 * @code
	gCamcorderCallbackParameters->camcorder = *camcorder;
	gCamcorderCallbackParameters->mode = IMAGE_MODE;
	gCamcorderCallbackParameters->isMultishot = false;
	gCamcorderCallbackParameters->stillshot_count = 0;
	gCamcorderCallbackParameters->multishot_count = 0;
	gCamcorderCallbackParameters->stillshot_filename = "stillshot_file";;
	gCamcorderCallbackParameters->multishot_filename = "multishot_file";;
 * @endcode
 * @param [out] None
 * @return MM_ERROR_NONE
 */
void utc_camcorder_set_video_stream_callback_001()
{
	tet_printf("\n ======================utc_camcorder_set_video_stream_callback_001====================== \n");

	gint errorValue = -1;
	gboolean bReturnValue = false;
	MMHandleType camcorder = 0;
	MMCamPreset info;

	info.videodev_type = MM_VIDEO_DEVICE_CAMERA0;

	bReturnValue = CreateCamcorder(&camcorder, &info);
	if (true != bReturnValue) {
		tet_printf("\n Failed in utc_camcorder_set_video_stream_callback_001 while starting the camcorder \n");
		tet_result(TET_UNTESTED);
		return;
	}

	errorValue = mm_camcorder_set_video_stream_callback(camcorder, VideoStreamCallbackFunctionCalledByCamcorder, gCamcorderCallbackParameters);

	bReturnValue = DestroyCamcorder(camcorder);

	dts_check_eq("mm_camcorder_set_video_stream_callback", errorValue, MM_ERROR_NONE, "Failed in utc_camcorder_set_video_stream_callback_001 while setting video stream callback function with error %x \n", errorValue);

	if (true != bReturnValue) {
		tet_printf("\n Failed in utc_camcorder_set_video_stream_callback_001 while destroying the camcorder \n");
		return;
	}

	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief This tests mm_camcorder_set_video_stream_callback API with NULL user parameter.
 * @par ID:
 * UTC_CAMCORDER_SET_VIDEO_STREAM_CALLBACK_002
 * @param [in] camcorder = Valid Camcorder Handle
 * @param [in] VideoStreamCallbackFunctionCalledByCamcorder (This itself is a function)
 * @param [in] Invalid handle
 * @param [out] None
 * @return MM_ERROR_NONE
 */
void utc_camcorder_set_video_stream_callback_002()
{
	tet_printf("\n ======================utc_camcorder_set_video_stream_callback_002====================== \n");

	gint errorValue = -1;
	gboolean bReturnValue = false;
	MMHandleType camcorder = 0;
	MMCamPreset info;

	info.videodev_type = MM_VIDEO_DEVICE_CAMERA0;

	bReturnValue = CreateCamcorder(&camcorder, &info);
	if (true != bReturnValue) {
		tet_printf("\n Failed in utc_camcorder_set_video_stream_callback_002 while starting the camcorder \n");
		tet_result(TET_UNTESTED);
		return;
	}

	errorValue = mm_camcorder_set_video_stream_callback(camcorder, VideoStreamCallbackFunctionCalledByCamcorder, NULL);

	bReturnValue = DestroyCamcorder(camcorder);

	dts_check_eq("mm_camcorder_set_video_stream_callback", errorValue, MM_ERROR_NONE, "Failed in utc_camcorder_set_video_stream_callback_002 while setting video stream callback function with error %x \n", errorValue);

	if (true != bReturnValue) {
		tet_printf("\n Failed in utc_camcorder_set_video_stream_callback_002 while destroying the camcorder \n");
		return;
	}

	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief This tests mm_camcorder_set_video_stream_callback API with NULL callback function.
 * @par ID:
 * UTC_CAMCORDER_SET_VIDEO_STREAM_CALLBACK_003
 * @param [in] camcorder = Valid Camcorder Handle
 * @param [in] Invalid handle
 * @param [in] gCamcorderCallbackParameters
 * @code
	gCamcorderCallbackParameters->camcorder = *camcorder;
	gCamcorderCallbackParameters->mode = IMAGE_MODE;
	gCamcorderCallbackParameters->isMultishot = false;
	gCamcorderCallbackParameters->stillshot_count = 0;
	gCamcorderCallbackParameters->multishot_count = 0;
	gCamcorderCallbackParameters->stillshot_filename = "stillshot_file";;
	gCamcorderCallbackParameters->multishot_filename = "multishot_file";;
 * @endcode
 * @param [out] None
 * @return Error code
 */
void utc_camcorder_set_video_stream_callback_003()
{
	tet_printf("\n ======================utc_camcorder_set_video_stream_callback_003====================== \n");

	gint errorValue = -1;
	gboolean bReturnValue = false;
	MMHandleType camcorder = 0;
	MMCamPreset info;

	info.videodev_type = MM_VIDEO_DEVICE_CAMERA0;

	bReturnValue = CreateCamcorder(&camcorder, &info);
	if (true != bReturnValue) {
		tet_printf("\n Failed in utc_camcorder_set_video_stream_callback_003 while starting the camcorder \n");
		tet_result(TET_UNTESTED);
		return;
	}

	errorValue = mm_camcorder_set_video_stream_callback(camcorder, NULL, gCamcorderCallbackParameters);

	bReturnValue = DestroyCamcorder(camcorder);

	dts_check_eq("mm_camcorder_set_video_stream_callback", errorValue, MM_ERROR_NONE, "Failed in utc_camcorder_set_video_stream_callback_003 while setting video stream callback function with error %x \n", errorValue);

	if (true != bReturnValue) {
		tet_printf("\n Failed in utc_camcorder_set_video_stream_callback_003 while destroying the camcorder \n");
		return;
	}

	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief This tests mm_camcorder_set_video_stream_callback API with NULL callback function as well as NULL user parameter.
 * @par ID:
 * UTC_CAMCORDER_SET_VIDEO_STREAM_CALLBACK_004
 * @param [in] camcorder = Valid Camcorder Handle
 * @param [in] Invalid handle
 * @param [in] Invalid handle
 * @param [out] None
 * @return Error code
 */
void utc_camcorder_set_video_stream_callback_004()
{
	tet_printf("\n ======================utc_camcorder_set_video_stream_callback_004====================== \n");

	gint errorValue = -1;
	gboolean bReturnValue = false;
	MMHandleType camcorder = 0;
	MMCamPreset info;

	info.videodev_type = MM_VIDEO_DEVICE_CAMERA0;

	bReturnValue = CreateCamcorder(&camcorder, &info);
	if (true != bReturnValue) {
		tet_printf("\n Failed in utc_camcorder_set_video_stream_callback_004 while starting the camcorder \n");
		tet_result(TET_UNTESTED);
		return;
	}

	errorValue = mm_camcorder_set_video_stream_callback(camcorder, NULL, NULL);

	bReturnValue = DestroyCamcorder(camcorder);

	dts_check_eq("mm_camcorder_set_video_stream_callback", errorValue, MM_ERROR_NONE, "Failed in utc_camcorder_set_video_stream_callback_004 while setting video stream callback function with error %x \n", errorValue);

	if (true != bReturnValue) {
		tet_printf("\n Failed in utc_camcorder_set_video_stream_callback_004 while destroying the camcorder \n");
		return;
	}

	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief This tests mm_camcorder_set_video_stream_callback API with invalid camcorder.
 * @par ID:
 * UTC_CAMCORDER_SET_VIDEO_STREAM_CALLBACK_005
 * @param [in] Invalid handle
 * @param [in] VideoStreamCallbackFunctionCalledByCamcorder (This itself is a function)
 * @param [in] gCamcorderCallbackParameters
 * @code
	gCamcorderCallbackParameters->camcorder = *camcorder;
	gCamcorderCallbackParameters->mode = IMAGE_MODE;
	gCamcorderCallbackParameters->isMultishot = false;
	gCamcorderCallbackParameters->stillshot_count = 0;
	gCamcorderCallbackParameters->multishot_count = 0;
	gCamcorderCallbackParameters->stillshot_filename = "stillshot_file";;
	gCamcorderCallbackParameters->multishot_filename = "multishot_file";;
 * @endcode
 * @param [out] None
 * @return Error code
 */
void utc_camcorder_set_video_stream_callback_005()
{
	tet_printf("\n ======================utc_camcorder_set_video_stream_callback_005====================== \n");

	gint errorValue = -1;
	gboolean bReturnValue = false;
	MMHandleType camcorder = 0;
	MMCamPreset info;

	info.videodev_type = MM_VIDEO_DEVICE_CAMERA0;

	bReturnValue = CreateCamcorder(&camcorder, &info);
	if (true != bReturnValue) {
		tet_printf("\n Failed in utc_camcorder_set_video_stream_callback_005 while starting the camcorder \n");
		tet_result(TET_UNTESTED);
		return;
	}

	errorValue = mm_camcorder_set_video_stream_callback(0, VideoStreamCallbackFunctionCalledByCamcorder, gCamcorderCallbackParameters);
	
	bReturnValue = DestroyCamcorder(camcorder);

	dts_check_ne("mm_camcorder_set_video_stream_callback", errorValue, MM_ERROR_NONE, "Failed in utc_camcorder_set_video_stream_callback_005 while setting video stream callback function with error %x \n", errorValue);

	if (true != bReturnValue) {
		tet_printf("\n Failed in utc_camcorder_set_video_stream_callback_005 while destroying the camcorder \n");
		return;
	}

	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief This tests mm_camcorder_set_video_stream_callback API with invalid camcorder as well as with NULL user parameter.
 * @par ID:
 * UTC_CAMCORDER_SET_VIDEO_STREAM_CALLBACK_006
 * @param [in] Invalid handle
 * @param [in] VideoStreamCallbackFunctionCalledByCamcorder (This itself is a function)
 * @param [in] Invalid handle
 * @param [out] None
 * @return Error code
 */
void utc_camcorder_set_video_stream_callback_006()
{
	tet_printf("\n ======================utc_camcorder_set_video_stream_callback_006====================== \n");

	gint errorValue = -1;
	gboolean bReturnValue = false;
	MMHandleType camcorder = 0;
	MMCamPreset info;

	info.videodev_type = MM_VIDEO_DEVICE_CAMERA0;

	bReturnValue = CreateCamcorder(&camcorder, &info);
	if (true != bReturnValue) {
		tet_printf("\n Failed in utc_camcorder_set_video_stream_callback_006 while starting the camcorder \n");
		tet_result(TET_UNTESTED);
		return;
	}

	errorValue = mm_camcorder_set_video_stream_callback(0, VideoStreamCallbackFunctionCalledByCamcorder, NULL);

	bReturnValue = DestroyCamcorder(camcorder);

	dts_check_ne("mm_camcorder_set_video_stream_callback", errorValue, MM_ERROR_NONE, "Failed in utc_camcorder_set_video_stream_callback_006 while setting video stream callback function with error %x \n", errorValue);

	if (true != bReturnValue) {
		tet_printf("\n Failed in utc_camcorder_set_video_stream_callback_006 while destroying the camcorder \n");
		return;
	}

	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief This tests mm_camcorder_set_video_stream_callback API with invalid camcorder as well as with NULL callback function.
 * @par ID:
 * UTC_CAMCORDER_SET_VIDEO_STREAM_CALLBACK_007
 * @param [in] Invalid handle
 * @param [in] Invalid handle
 * @param [in] gCamcorderCallbackParameters
 * @code
	gCamcorderCallbackParameters->camcorder = *camcorder;
	gCamcorderCallbackParameters->mode = IMAGE_MODE;
	gCamcorderCallbackParameters->isMultishot = false;
	gCamcorderCallbackParameters->stillshot_count = 0;
	gCamcorderCallbackParameters->multishot_count = 0;
	gCamcorderCallbackParameters->stillshot_filename = "stillshot_file";;
	gCamcorderCallbackParameters->multishot_filename = "multishot_file";;
 * @endcode
 * @param [out] None
 * @return Error code
 */
void utc_camcorder_set_video_stream_callback_007()
{
	tet_printf("\n ======================utc_camcorder_set_video_stream_callback_007====================== \n");

	gint errorValue = -1;
	gboolean bReturnValue = false;
	MMHandleType camcorder = 0;
	MMCamPreset info;

	info.videodev_type = MM_VIDEO_DEVICE_CAMERA0;

	bReturnValue = CreateCamcorder(&camcorder, &info);
	if (true != bReturnValue) {
		tet_printf("\n Failed in utc_camcorder_set_video_stream_callback_007 while starting the camcorder \n");
		tet_result(TET_UNTESTED);
		return;
	}

	errorValue = mm_camcorder_set_video_stream_callback(0, NULL, gCamcorderCallbackParameters);

	bReturnValue = DestroyCamcorder(camcorder);

	dts_check_ne("mm_camcorder_set_video_stream_callback", errorValue, MM_ERROR_NONE, "Failed in utc_camcorder_set_video_stream_callback_007 while setting video stream callback function with error %x \n", errorValue);

	if (true != bReturnValue) {
		tet_printf("\n Failed in utc_camcorder_set_video_stream_callback_007 while destroying the camcorder \n");
		return;
	}

	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief This tests mm_camcorder_set_video_stream_callback API with invalid camcorder as well as with NULL callback function and NULL user parameter.
 * @par ID:
 * UTC_CAMCORDER_SET_VIDEO_STREAM_CALLBACK_008
 * @param [in] Invalid handle
 * @param [in] Invalid handle
 * @param [in] Invalid handle
 * @param [out] None
 * @return Error code
 */
void utc_camcorder_set_video_stream_callback_008()
{
	tet_printf("\n ======================utc_camcorder_set_video_stream_callback_008====================== \n");

	gint errorValue = -1;
	gboolean bReturnValue = false;
	MMHandleType camcorder = 0;
	MMCamPreset info;

	info.videodev_type = MM_VIDEO_DEVICE_CAMERA0;

	bReturnValue = CreateCamcorder(&camcorder, &info);
	if (true != bReturnValue) {
		tet_printf("\n Failed in utc_camcorder_set_video_stream_callback_008 while starting the camcorder \n");
		tet_result(TET_UNTESTED);
		return;
	}

	errorValue = mm_camcorder_set_video_stream_callback(0, NULL, NULL);

	bReturnValue = DestroyCamcorder(camcorder);

	dts_check_ne("mm_camcorder_set_video_stream_callback", errorValue, MM_ERROR_NONE, "Failed in utc_camcorder_set_video_stream_callback_008 while setting video stream callback function with error %x \n", errorValue);

	if (true != bReturnValue) {
		tet_printf("\n Failed in utc_camcorder_set_video_stream_callback_008 while destroying the camcorder \n");
		return;
	}

	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/** @} */
