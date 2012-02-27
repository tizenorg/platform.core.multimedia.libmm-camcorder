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
* @addtogroup 	UTS_MM_CAMCORDER_GET_STATE Uts_Mm_Camcorder_Get_State
* @{
*/

/**
* @file		uts_mm_camcorder_get_state.c
* @brief	This is a suite of unit test cases to test mm_camcorder_get_state API.
* @author	Kishor Reddy (p.kishor@samsung.com)
* @version	Initial Creation V0.1
* @date		2008.11.27
*/

#include "uts_mm_camcorder_get_state.h"
#include "uts_mm_camcorder_utils.h"

struct tet_testlist tet_testlist[] = {
	{utc_camcorder_get_state_001,1},
	{utc_camcorder_get_state_002,2},
	{utc_camcorder_get_state_003,3},
	{NULL,0}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief This tests mm_camcorder_get_state API in normal conditions.
 * @par ID:
 * UTC_CAMCORDER_GET_STATE_001
 * @par Description: In order to get each and every state we called mm_camcorder_create,mm_camcorder_realize,mm_camcorder_start,mm_camcorder_capture_start,mm_camcorder_capture_stop APIs.
 * @param [in] camcorder = Valid Camcorder Handle
 * @code
 errorValue = mm_camcorder_create(&camcorder);
 errorValue = mm_camcorder_get_state(camcorder);

 errorValue = mm_camcorder_realize(camcorder);
 errorValue = mm_camcorder_get_state(camcorder);

 errorValue = mm_camcorder_start(camcorder);
 errorValue = mm_camcorder_get_state(camcorder);

 errorValue = mm_camcorder_capture_start(camcorder);
 errorValue = mm_camcorder_get_state(camcorder);

 errorValue = mm_camcorder_capture_stop(camcorder);
 errorValue = mm_camcorder_get_state(camcorder);
 * @param [out] None
 * @param [out] None
 * @return State of the camcorder
 */
void utc_camcorder_get_state_001()
{
	tet_printf("\n ======================utc_camcorder_get_state_001====================== \n");

	gint errorValue = -1; 
	gboolean bReturnValue = false;
	MMCamcorderStateType state = MM_CAMCORDER_STATE_NUM;
	int mode = IMAGE_MODE;
	MMHandleType camcorder = 0;
	MMCamPreset info;

	info.videodev_type = MM_VIDEO_DEVICE_CAMERA0;

	errorValue = mm_camcorder_create(&camcorder, &info);
	if (MM_ERROR_NONE != errorValue)  {
		tet_printf("\n Failed in utc_camcorder_get_state_001 while creating the camcorder with error %x but mmcam_init API was passed \n", errorValue);
		tet_result(TET_UNTESTED);
		return;
	}

	bReturnValue = SetDefaultAttributes(camcorder, mode);
	if (true != bReturnValue) {
		tet_printf("\n Failed in utc_camcorder_get_state_001 while setting default attributes to the camcorder \n");
		tet_result(TET_UNTESTED);       		
		return;
	}

	errorValue = mm_camcorder_get_state(camcorder, &state);
	if (MM_CAMCORDER_STATE_NULL != state) {
		tet_printf("\n Failed in utc_camcorder_get_state_001 while getting the NULL state from the camcorder with error %x \n", errorValue);
		tet_result(TET_FAIL);
		return;
	}

	errorValue = mm_camcorder_realize(camcorder);
	if (MM_ERROR_NONE != errorValue) {
		tet_printf("\n Failed in utc_camcorder_get_state_001 while realizing the camcorder with error %x but mm_camcorder_create API was passed \n", errorValue);
		tet_result(TET_UNTESTED);
		return;
	}

	errorValue = mm_camcorder_get_state(camcorder, &state);
	if (MM_CAMCORDER_STATE_READY != state) {
		tet_printf("\n Failed in utc_camcorder_get_state_001 while getting the READY state from the camcorder with error %x \n", errorValue);
		tet_result(TET_FAIL);
		return;
	}

	errorValue = mm_camcorder_start(camcorder);
	if (MM_ERROR_NONE != errorValue) {
		tet_printf("\n Failed in utc_camcorder_get_state_001 while starting the camcorder with error %x but mm_camcorder_realize API was passed \n", errorValue);
		tet_result(TET_UNTESTED);
		return;
	}

	errorValue = mm_camcorder_get_state(camcorder, &state);
	if (MM_CAMCORDER_STATE_PREPARE != state) {
		tet_printf("\n Failed in utc_camcorder_get_state_001 while getting the PREPARED state from the camcorder with error %x \n", errorValue);
		tet_result(TET_FAIL);
		return;
	}

	if(IMAGE_MODE == mode) {
		errorValue = mm_camcorder_capture_start(camcorder);
		if (MM_ERROR_NONE != errorValue) {
			tet_printf("\n Failed in utc_camcorder_get_state_001 while starting the capture mode of the camcorder with error %x but mm_camcorder_start API was passed \n", errorValue);
			tet_result(TET_UNTESTED);
			return;
		}

		errorValue = mm_camcorder_get_state(camcorder, &state);
		if (MM_CAMCORDER_STATE_CAPTURING != state) {
			tet_printf("\n Failed in utc_camcorder_get_state_001 while getting the CAPTURING state from the camcorder with error %x \n", errorValue);
			tet_result(TET_FAIL);
			return;
		}
/*
		errorValue = mm_camcorder_pause(camcorder);
		if (MM_ERROR_NONE != errorValue) {
			tet_printf("\n Failed in utc_camcorder_get_state_001 while pausing the capture mode of the camcorder with error %x but mm_camcorder_start API was passed \n", errorValue);
			tet_result(TET_UNTESTED);
			return;
		}

		errorValue = mm_camcorder_get_state(camcorder);
		if (MM_CAMCORDER_STATE_PAUSED != errorValue) {
			tet_printf("\n Failed in utc_camcorder_get_state_001 while getting the PAUSE state from the camcorder with error %x \n", errorValue);
			tet_result(TET_FAIL);
			return;
		}
*/	
		sleep(5);

		errorValue = mm_camcorder_capture_stop(camcorder);
		if (MM_ERROR_NONE != errorValue) {
			tet_printf("\n Failed in utc_camcorder_get_state_001 while stoping the capture mode of the camcorder with error %x but mm_camcorder_start API was passed \n", errorValue);
			tet_result(TET_UNTESTED);
			return;
		}
	}
	else if (VIDEO_MODE == mode) {
		errorValue = mm_camcorder_record(camcorder);
		if (MM_ERROR_NONE != errorValue) {
			tet_printf("\n Failed in utc_camcorder_get_state_001 while recording in the camcorder with error %x but mm_camcorder_start API was passed \n", errorValue);
			tet_result(TET_UNTESTED);
			return;
		}

		errorValue = mm_camcorder_get_state(camcorder, &state);
		if (MM_CAMCORDER_STATE_RECORDING != state) {
			tet_printf("\n Failed in utc_camcorder_get_state_001 while getting the RECORDING state from the camcorder with error %x \n", errorValue);
			tet_result(TET_FAIL);
			return;
		}

		errorValue = mm_camcorder_pause(camcorder);
		if (MM_ERROR_NONE != errorValue) {
			tet_printf("\n Failed in utc_camcorder_get_state_001 while pausing the recording mode of the camcorder with error %x but mm_camcorder_capture_start API was passed \n", errorValue);
			tet_result(TET_UNTESTED);
			return;
		}

		errorValue = mm_camcorder_get_state(camcorder, &state);
		if (MM_CAMCORDER_STATE_PAUSED != state) {
			tet_printf("\n Failed in utc_camcorder_get_state_001 while getting the PAUSE state from the camcorder with error %x \n", errorValue);
			tet_result(TET_FAIL);
			return;
		}

		errorValue = mm_camcorder_cancel(camcorder);
		if (MM_ERROR_NONE != errorValue) {
			tet_printf("\n Failed in utc_camcorder_get_state_001 while stoping the recording mode of the camcorder with error %x but mm_camcorder_record API was passed \n", errorValue);
			return;
		}
	}

	tet_result(TET_PASS);

	errorValue = mm_camcorder_stop(camcorder);
	if (MM_ERROR_NONE != errorValue) {
		tet_printf("\n Failed in utc_camcorder_get_state_001 while stoping the camcorder with error %x but mm_camcorder_start API was passed \n", errorValue);
		return;
	}

	errorValue = mm_camcorder_unrealize(camcorder);
	if (MM_ERROR_NONE != errorValue) {
		tet_printf("\n Failed in utc_camcorder_get_state_001 while unrealizing the camcorder with error %x but mm_camcorder_realize API was passed \n", errorValue);
		return;
	}

	errorValue = mm_camcorder_destroy(camcorder);
	if (MM_ERROR_NONE != errorValue) {
		tet_printf("\n Failed in utc_camcorder_get_state_001 while destroying the camcorder with error %x but mm_camcorder_unrealize API was passed \n", errorValue);
		return;
	}

	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief This tests mm_camcorder_get_state API with invalid camcorder.
 * @par ID:
 * UTC_CAMCORDER_GET_STATE_002
 * @param [in] Invalid handle
 * @param [out] None
 * @return Error code
 */
void utc_camcorder_get_state_002()
{
	tet_printf("\n ======================utc_camcorder_stop_002====================== \n");

	gint errorValue = -1; 
	MMCamcorderStateType state = MM_CAMCORDER_STATE_NUM;

	errorValue = mm_camcorder_get_state(0, &state);
	
	dts_check_ne( "mm_camcorder_get_state", errorValue, MM_ERROR_NONE,
			"Failed in utc_camcorder_get_state_002 while getting the camcorder with error %x", errorValue );

	return;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief This tests mm_camcorder_get_state API with invalid camcorder.
 * @par ID:
 * UTC_CAMCORDER_GET_STATE_003
 * @param [in] Invalid handle
 * @param [out] None
 * @return Error code
 */
void utc_camcorder_get_state_003()
{
	tet_printf("\n ======================utc_camcorder_stop_002====================== \n");

	gint errorValue = -1; 
	MMCamcorderStateType *state = NULL;
	MMHandleType camcorder = 0;
	MMCamPreset info;

	info.videodev_type = MM_VIDEO_DEVICE_NUM;

	errorValue = mm_camcorder_create(&camcorder, &info);
	if (MM_ERROR_NONE == errorValue)  {
		tet_printf("\n Failed in utc_camcorder_get_state_003 while creating the camcorder with error %x but mmcam_init API was passed \n", errorValue);
		tet_result(TET_UNTESTED);
		return;
	}

	errorValue = mm_camcorder_get_state(camcorder, state);

	dts_check_ne( "mm_camcorder_get_state", errorValue, MM_ERROR_NONE,
			"Failed in utc_camcorder_get_state_003 while getting the camcorder with error %x", errorValue );

 	return;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
/** @} */
