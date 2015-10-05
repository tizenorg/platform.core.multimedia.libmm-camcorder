/*
 * libmm-camcorder
 *
 * Copyright (c) 2000 - 2015 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Sejong Park <sejong123.park@samsung.com>
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

#ifndef __MM_CAMCORDER_MUSED_H__
#define __MM_CAMCORDER_MUSED_H__

#include "mm_types.h"

#ifdef __cplusplus
	extern "C" {
#endif

/**
 * Camcorder Client's attribute enumeration.
 */
typedef enum
{
	MM_CAM_CLIENT_DISPLAY_SHM_SOCKET_PATH,
	MM_CAM_CLIENT_DISPLAY_HANDLE,
	MM_CAM_CLIENT_DISPLAY_DEVICE,
	MM_CAM_CLIENT_DISPLAY_SURFACE,
	MM_CAM_CLIENT_DISPLAY_RECT_X,
	MM_CAM_CLIENT_DISPLAY_RECT_Y,
	MM_CAM_CLIENT_DISPLAY_RECT_WIDTH,
	MM_CAM_CLIENT_DISPLAY_RECT_HEIGHT,
	MM_CAM_CLIENT_DISPLAY_SOURCE_X,
	MM_CAM_CLIENT_DISPLAY_SOURCE_Y,
	MM_CAM_CLIENT_DISPLAY_SOURCE_WIDTH,
	MM_CAM_CLIENT_DISPLAY_SOURCE_HEIGHT,
	MM_CAM_CLIENT_DISPLAY_ROTATION,
	MM_CAM_CLIENT_DISPLAY_VISIBLE,
	MM_CAM_CLIENT_DISPLAY_SCALE,
	MM_CAM_CLIENT_DISPLAY_GEOMETRY_METHOD,
	MM_CAM_CLIENT_DISPLAY_MODE,
	MM_CAM_CLIENT_DISPLAY_EVAS_SURFACE_SINK,
	MM_CAM_CLIENT_DISPLAY_EVAS_DO_SCALING,
	MM_CAM_CLIENT_DISPLAY_FLIP,
	MM_CAM_CLIENT_ATTRIBUTE_NUM
}MMCamcorderClientAttrsID;

/**
 * Camcorder Client Pipeline's Element name.
 * @note index of element.
 */
typedef enum {
	_MMCAMCORDER_CLIENT_NONE = (-1),

	/* Main Pipeline Element */
	_MMCAMCORDER_CLIENT_MAIN_PIPE = 0x00,

	/* Pipeline element of Video input */
	_MMCAMCORDER_CLIENT_VIDEOSRC_SRC,

	/* Pipeline element of Video Sink Queue */
	_MMCAMCORDER_CLIENT_VIDEOSINK_QUE,

	/* Pipeline element of Video Sink CLS */
	_MMCAMCORDER_CLIENT_VIDEOSINK_CLS,

	/* Pipeline element of Video output */
	_MMCAMCORDER_CLIENT_VIDEOSINK_SINK,

	/* Client pipeline Max number */
	_MMCAMCORDER_CLIENT_PIPELINE_ELEMENT_NUM
} _MMCAMCORDER_PREVIEW_CLIENT_PIPELINE_ELELMENT;

/**
 * This function creates resources at the client process.
 *
 * @param[in]	handle		Handle of camcorder context.
 * @return	This function returns MM_ERROR_NONE on success, or the other values on error.
 */
int mm_camcorder_client_create(MMHandleType *handle);

/**
 * This function destroys resources from the client process.
 *
 * @param[in]	handle		Handle of camcorder context.
 * @return	void
 */
void mm_camcorder_client_destroy(MMHandleType handle);

/**
 * This function prepares for the state running.
 *
 * @param[in]	handle		Handle of camcorder context.
 * @return	This function returns MM_ERROR_NONE on success, or the other values on error.
 * @see		_mmcamcorder_client_realize function.
 */
int mm_camcorder_client_realize(MMHandleType handle, char *caps);

/**
 * This function unprepare for the state null.
 *
 * @param[in]	handle		Handle of camcorder context.
 * @return	This function returns MM_ERROR_NONE on success, or the other values on error.
 * @see		_mmcamcorder_client_unrealize function.
 */
int mm_camcorder_client_unrealize(MMHandleType handle);

/**
 * This function get string of raw video caps.
 * To be used by server.
 *
 * @param	handle  [in] Handle of camera.
 * @param	caps    [out] String of caps. Should be freed after used.
 *
 * @return	This function returns zero on success, or negative value with error
 *			code.
 * @see
 * @since
 */
int mm_camcorder_client_get_video_caps(MMHandleType handle, char **caps);

/**
 * This function set "socket-path" element property of shmsink/src.
 * To be used by both server and client.
 *
 * @param	handle  [in] Handle of camera.
 * @param	path    [in] Local file path.
 *
 * @return	This function returns zero on success, or negative value with error
 *			code.
 * @see
 * @since
 */
int mm_camcorder_client_set_shm_socket_path(MMHandleType handle, const char *path);

#ifdef __cplusplus
	}
#endif

#endif /* __MM_CAMCORDER_MUSED_H__ */
