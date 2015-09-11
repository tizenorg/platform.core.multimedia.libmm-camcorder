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

/*===========================================================================================
|																							|
|  INCLUDE FILES																			|
|  																							|
========================================================================================== */
#include "mm_types.h"

/*===========================================================================================
|																							|
|  GLOBAL DEFINITIONS AND DECLARATIONS FOR MODULE											|
|  																							|
========================================================================================== */

/*---------------------------------------------------------------------------
|    GLOBAL #defines:														|
---------------------------------------------------------------------------*/

#ifdef __cplusplus
	extern "C" {
#endif

/**
 * Camcorder Pipeline's Element name.
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

int mm_camcorder_client_create(MMHandleType *handle);
void mm_camcorder_client_destroy(MMHandleType handle);
int mm_camcorder_client_realize(MMHandleType handle, char *caps);
int mm_camcorder_client_unrealize(MMHandleType handle);
int mm_camcorder_client_pre_unrealize(MMHandleType handle);
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


/**
 * Determines the shm stream path
 */
#define MMCAM_DISPLAY_SHM_SOCKET_PATH		"display-shm-socket-path"

/**
 * Surface of display.
 * @see		MMDisplaySurfaceType (in mm_types.h)
 */
#define MMCAM_DISPLAY_SURFACE                    "display-surface"

/**
 * Pointer of display buffer or ID of xwindow.
 */
#define MMCAM_DISPLAY_HANDLE                    "display-handle"

#define SOCKET_PATH_LENGTH 32
#define SOCKET_PATH_BASE "/tmp/mused_gst.%d"

#ifdef __cplusplus
	}
#endif

#endif /* __MM_CAMCORDER_MUSED_H__ */
