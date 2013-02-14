/*
 * libmm-camcorder
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

#ifndef __MM_CAMCORDER_VIDEOREC_H__
#define __MM_CAMCORDER_VIDEOREC_H__

/*=======================================================================================
| INCLUDE FILES										|
========================================================================================*/
#include <mm_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=======================================================================================
| GLOBAL DEFINITIONS AND DECLARATIONS FOR CAMCORDER					|
========================================================================================*/

/*=======================================================================================
| MACRO DEFINITIONS									|
========================================================================================*/

/*=======================================================================================
| ENUM DEFINITIONS									|
========================================================================================*/

/*=======================================================================================
| STRUCTURE DEFINITIONS									|
========================================================================================*/
/**
 * MMCamcorder information for video(preview/recording) mode
 */
typedef struct {
	gboolean b_commiting;		/**< Is it commiting now? */
	char *filename;			/**< recorded filename */
	double record_timestamp_ratio;	/**< timestamp ratio of video recording for slow motion recording */
	guint64 video_frame_count;	/**< current video frame */
	guint64 audio_frame_count;	/**< current audio frame */
	guint64 filesize;		/**< current file size */
	guint64 max_size;		/**< max recording size */
	guint64 max_time;		/**< max recording time */
	int fileformat;			/**< recording file format */
/*
	guint checker_id;
	guint checker_count;
*/
} _MMCamcorderVideoInfo;

/*=======================================================================================
| CONSTANT DEFINITIONS									|
========================================================================================*/

/*=======================================================================================
| STATIC VARIABLES									|
========================================================================================*/

/*=======================================================================================
| EXTERN GLOBAL VARIABLE								|
========================================================================================*/

/*=======================================================================================
| GLOBAL FUNCTION PROTOTYPES								|
========================================================================================*/
/**
 * This function add recorder bin to main pipeline.
 * When application creates initial pipeline, there are only bins for preview.
 * If application wants to add recording function, bins for recording should be added.
 *
 * @param[in]	handle		Handle of camcorder context.
 * @return	This function returns MM_ERROR_NONE on success, or the other values on error.
 * @remarks
 * @see		__mmcamcorder_create_preview_pipeline()
 */
int _mmcamcorder_add_recorder_pipeline(MMHandleType handle);

/**
 * This function remove recorder bin from main pipeline.
 *
 * @param[in]	handle		Handle of camcorder context.
 * @return	This function returns MM_ERROR_NONE on success, or the other values on error.
 * @remarks
 * @see		__mmcamcorder_create_preview_pipeline(), __mmcamcorder_add_recorder_pipeline()
 */
int _mmcamcorder_remove_recorder_pipeline(MMHandleType handle);

/**
 * This function destroy video pipeline.
 *
 * @param[in]	handle		Handle of camcorder context.
 * @return	void
 * @remarks
 * @see		_mmcamcorder_destroy_pipeline()
 */
void _mmcamcorder_destroy_video_pipeline(MMHandleType handle);

/**
 * This function operates each command on video mode.
 *
 * @param[in]	handle		Handle of camcorder context.
 * @param[in]	command		command type received from Multimedia Framework.
 * @return	This function returns MM_ERROR_NONE on success, or the other values on error.
 * @remarks
 * @see		_mmcamcorder_set_functions()
 */
int _mmcamcorder_video_command(MMHandleType handle, int command);

/**
 * This function handles EOS(end of stream) when commit video recording.
 *
 * @param[in]	handle		Handle of camcorder context.
 * @return	This function returns TRUE on success, or FALSE on failure.
 * @remarks
 * @see		_mmcamcorder_set_functions()
 */
int _mmcamcorder_video_handle_eos(MMHandleType handle);


#ifdef __cplusplus
}
#endif

#endif /* __MM_CAMCORDER_VIDEOREC_H__ */
