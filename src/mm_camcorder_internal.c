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

/*=======================================================================================
|  INCLUDE FILES									|
========================================================================================*/
#include <stdio.h>
#include <string.h>
#include <gst/gst.h>
#include <gst/gstutils.h>
#include <gst/gstpad.h>

#include <mm_error.h>
#include "mm_camcorder_internal.h"
#include <mm_types.h>

#include <gst/interfaces/colorbalance.h>
#include <gst/interfaces/cameracontrol.h>
#include <asm/types.h>

#include <mm_session.h>
#include <mm_session_private.h>
#include <audio-session-manager.h>
#include <vconf.h>

/*---------------------------------------------------------------------------------------
|    GLOBAL VARIABLE DEFINITIONS for internal						|
---------------------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------------------
|    LOCAL VARIABLE DEFINITIONS for internal						|
---------------------------------------------------------------------------------------*/
#define __MMCAMCORDER_CMD_ITERATE_MAX           3
#define __MMCAMCORDER_SECURITY_HANDLE_DEFAULT   -1
#define __MMCAMCORDER_SET_GST_STATE_TIMEOUT     5

/*---------------------------------------------------------------------------------------
|    LOCAL FUNCTION PROTOTYPES:								|
---------------------------------------------------------------------------------------*/
/* STATIC INTERNAL FUNCTION */
static gboolean __mmcamcorder_gstreamer_init(camera_conf * conf);

static gboolean __mmcamcorder_handle_gst_error(MMHandleType handle, GstMessage *message, GError *error);
static gint     __mmcamcorder_gst_handle_stream_error(MMHandleType handle, int code, GstMessage *message);
static gint     __mmcamcorder_gst_handle_resource_error(MMHandleType handle, int code, GstMessage *message);
static gint     __mmcamcorder_gst_handle_library_error(MMHandleType handle, int code, GstMessage *message);
static gint     __mmcamcorder_gst_handle_core_error(MMHandleType handle, int code, GstMessage *message);
static gint     __mmcamcorder_gst_handle_resource_warning(MMHandleType handle, GstMessage *message , GError *error);
static gboolean __mmcamcorder_handle_gst_warning(MMHandleType handle, GstMessage *message, GError *error);

static int      __mmcamcorder_asm_get_event_type(int sessionType);
static void     __mmcamcorder_force_stop(mmf_camcorder_t *hcamcorder);
static void     __mmcamcorder_force_resume(mmf_camcorder_t *hcamcorder);
ASM_cb_result_t _mmcamcorder_asm_callback(int handle, ASM_event_sources_t event_src,
                                          ASM_sound_commands_t command,
                                          unsigned int sound_status, void *cb_data);

static gboolean __mmcamcorder_set_attr_to_camsensor_cb(gpointer data);

/*=======================================================================================
|  FUNCTION DEFINITIONS									|
=======================================================================================*/
/*---------------------------------------------------------------------------------------
|    GLOBAL FUNCTION DEFINITIONS:							|
---------------------------------------------------------------------------------------*/

/* Internal command functions {*/
int _mmcamcorder_create(MMHandleType *handle, MMCamPreset *info)
{
	int ret = MM_ERROR_NONE;
	int UseConfCtrl = 0;
	int sessionType = MM_SESSION_TYPE_EXCLUSIVE;
	int errorcode = MM_ERROR_NONE;
	int rcmd_fmt_capture = MM_PIXEL_FORMAT_YUYV;
	int rcmd_fmt_recording = MM_PIXEL_FORMAT_NV12;
	int rcmd_dpy_rotation = MM_DISPLAY_ROTATION_270;
	int play_capture_sound = TRUE;
	char *err_attr_name = NULL;
	char *ConfCtrlFile = NULL;
	mmf_camcorder_t *hcamcorder = NULL;
	ASM_resource_t mm_resource = ASM_RESOURCE_NONE;

	_mmcam_dbg_log("Entered");

	mmf_return_val_if_fail(handle, MM_ERROR_CAMCORDER_INVALID_ARGUMENT);
	mmf_return_val_if_fail(info, MM_ERROR_CAMCORDER_INVALID_ARGUMENT);

	/* Create mmf_camcorder_t handle and initialize every variable */
	hcamcorder = (mmf_camcorder_t *)malloc(sizeof(mmf_camcorder_t));
	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_LOW_MEMORY);
	memset(hcamcorder, 0x00, sizeof(mmf_camcorder_t));

	/* init values */
	hcamcorder->type = 0;
	hcamcorder->state = MM_CAMCORDER_STATE_NONE;
	hcamcorder->sub_context = NULL;
	hcamcorder->target_state = MM_CAMCORDER_STATE_NULL;

	/* thread - for g_mutex_new() */
	if (!g_thread_supported()) {
		g_thread_init(NULL);
	}

	(hcamcorder->mtsafe).lock = g_mutex_new();
	(hcamcorder->mtsafe).cond = g_cond_new();

	(hcamcorder->mtsafe).cmd_lock = g_mutex_new();
	(hcamcorder->mtsafe).state_lock = g_mutex_new();
	(hcamcorder->mtsafe).gst_state_lock = g_mutex_new();
	(hcamcorder->mtsafe).message_cb_lock = g_mutex_new();
	(hcamcorder->mtsafe).vcapture_cb_lock = g_mutex_new();
	(hcamcorder->mtsafe).vstream_cb_lock = g_mutex_new();
	(hcamcorder->mtsafe).astream_cb_lock = g_mutex_new();

	pthread_mutex_init(&(hcamcorder->sound_lock), NULL);
	pthread_cond_init(&(hcamcorder->sound_cond), NULL);

	/* Sound mutex/cond init */
	pthread_mutex_init(&(hcamcorder->snd_info.open_mutex), NULL);
	pthread_cond_init(&(hcamcorder->snd_info.open_cond), NULL);

	if (info->videodev_type < MM_VIDEO_DEVICE_NONE ||
	    info->videodev_type >= MM_VIDEO_DEVICE_NUM) {
		_mmcam_dbg_err("_mmcamcorder_create::video device type is out of range.");
		ret = MM_ERROR_CAMCORDER_INVALID_ARGUMENT;
		goto _ERR_DEFAULT_VALUE_INIT;
	}

	/* Check and register ASM */
	if (MM_ERROR_NONE != _mm_session_util_read_type(-1, &sessionType)) {
		_mmcam_dbg_warn("Read _mm_session_util_read_type failed. use default \"exclusive\" type");
		sessionType = MM_SESSION_TYPE_EXCLUSIVE;
		if (MM_ERROR_NONE != mm_session_init(sessionType)) {
			_mmcam_dbg_err("mm_session_init() failed");
			ret = MM_ERROR_POLICY_INTERNAL;
			goto _ERR_DEFAULT_VALUE_INIT;
		}
	}
	if ((sessionType != MM_SESSION_TYPE_CALL) && (sessionType != MM_SESSION_TYPE_VIDEOCALL)) {
		int asm_session_type = ASM_EVENT_NONE;
		int asm_handle;
		int pid = -1; /* process id of itself */

		asm_session_type = __mmcamcorder_asm_get_event_type( sessionType );
		/* Call will not be interrupted. so does not need callback function */
		if (!ASM_register_sound(pid, &asm_handle, asm_session_type, ASM_STATE_NONE,
		                        (ASM_sound_cb_t)_mmcamcorder_asm_callback,
		                        (void*)hcamcorder, mm_resource, &errorcode)) {
			_mmcam_dbg_err("ASM_register_sound() failed[%x]", errorcode);
			ret = MM_ERROR_POLICY_INTERNAL;
			goto _ERR_DEFAULT_VALUE_INIT;
		}
		hcamcorder->asm_handle = asm_handle;
	}

	/* Get Camera Configure information from Camcorder INI file */
	__ta__( "    _mmcamcorder_conf_get_info main",
	_mmcamcorder_conf_get_info(CONFIGURE_TYPE_MAIN, CONFIGURE_MAIN_FILE, &hcamcorder->conf_main);
	);

	if (!(hcamcorder->conf_main)) {
		_mmcam_dbg_err( "Failed to get configure(main) info." );

		ret = MM_ERROR_CAMCORDER_CREATE_CONFIGURE;
		goto _ERR_AUDIO_BLOCKED;
	}

	__ta__("    _mmcamcorder_alloc_attribute",   
	hcamcorder->attributes= _mmcamcorder_alloc_attribute((MMHandleType)hcamcorder, info);
	);
	if (!(hcamcorder->attributes)) {
		_mmcam_dbg_err("_mmcamcorder_create::alloc attribute error.");
		
		ret = MM_ERROR_CAMCORDER_RESOURCE_CREATION;
		goto _ERR_AUDIO_BLOCKED;
	}

	if (info->videodev_type != MM_VIDEO_DEVICE_NONE) {
		_mmcamcorder_conf_get_value_int(hcamcorder->conf_main,
		                                CONFIGURE_CATEGORY_MAIN_VIDEO_INPUT,
		                                "UseConfCtrl", &UseConfCtrl);

		if (UseConfCtrl) {
			_mmcam_dbg_log( "Enable Configure Control system." );
#if 1
			switch (info->videodev_type) {
			case MM_VIDEO_DEVICE_CAMERA0:
				_mmcamcorder_conf_get_value_string(hcamcorder->conf_main,
				                                   CONFIGURE_CATEGORY_MAIN_VIDEO_INPUT,
				                                   "ConfCtrlFile0", &ConfCtrlFile);
				break;
			case MM_VIDEO_DEVICE_CAMERA1:
				_mmcamcorder_conf_get_value_string(hcamcorder->conf_main,
				                                   CONFIGURE_CATEGORY_MAIN_VIDEO_INPUT,
				                                   "ConfCtrlFile1", &ConfCtrlFile);
				break;
			default:
				_mmcam_dbg_err( "Not supported camera type." );
				ret = MM_ERROR_CAMCORDER_NOT_SUPPORTED;
				goto _ERR_ALLOC_ATTRIBUTE;
			}

			_mmcam_dbg_log("videodev_type : [%d], ConfCtrlPath : [%s]", info->videodev_type, ConfCtrlFile);

			__ta__( "    _mmcamcorder_conf_get_info ctrl",
			_mmcamcorder_conf_get_info(CONFIGURE_TYPE_CTRL, ConfCtrlFile, &hcamcorder->conf_ctrl);
			);
/*
			_mmcamcorder_conf_print_info(&hcamcorder->conf_main);
			_mmcamcorder_conf_print_info(&hcamcorder->conf_ctrl);
*/
#else
			_mmcamcorder_conf_query_info(CONFIGURE_TYPE_CTRL, info->videodev_type, &hcamcorder->conf_ctrl);
#endif
			if (!(hcamcorder->conf_ctrl)) {
				_mmcam_dbg_err( "Failed to get configure(control) info." );
				ret = MM_ERROR_CAMCORDER_CREATE_CONFIGURE;
				goto _ERR_ALLOC_ATTRIBUTE;
			}

			__ta__( "    _mmcamcorder_init_convert_table",
			ret = _mmcamcorder_init_convert_table((MMHandleType)hcamcorder);
			);
			if (ret != MM_ERROR_NONE) {
				_mmcam_dbg_warn("converting table initialize error!!");
			}

			__ta__( "    _mmcamcorder_init_attr_from_configure",
			ret = _mmcamcorder_init_attr_from_configure((MMHandleType)hcamcorder);
			);
			if (ret != MM_ERROR_NONE) {
				_mmcam_dbg_warn("converting table initialize error!!");
			}
		}
		else
		{
			_mmcam_dbg_log( "Disable Configure Control system." );
			hcamcorder->conf_ctrl = NULL;
		}
	}

	__ta__( "    __mmcamcorder_gstreamer_init",
	ret = __mmcamcorder_gstreamer_init(hcamcorder->conf_main);
	);
	if (!ret) {
		_mmcam_dbg_err( "Failed to initialize gstreamer!!" );
		ret = MM_ERROR_CAMCORDER_NOT_INITIALIZED;
		goto _ERR_ALLOC_ATTRIBUTE;
	}

	/* Get recommend preview format and display rotation from INI */
	_mmcamcorder_conf_get_value_int(hcamcorder->conf_ctrl,
	                                CONFIGURE_CATEGORY_CTRL_CAMERA,
	                                "RecommendPreviewFormatCapture",
	                                &rcmd_fmt_capture);

	_mmcamcorder_conf_get_value_int(hcamcorder->conf_ctrl,
	                                CONFIGURE_CATEGORY_CTRL_CAMERA,
	                                "RecommendPreviewFormatRecord",
	                                &rcmd_fmt_recording);

	_mmcamcorder_conf_get_value_int(hcamcorder->conf_ctrl,
	                                CONFIGURE_CATEGORY_CTRL_CAMERA,
	                                "RecommendDisplayRotation",
	                                &rcmd_dpy_rotation);

	_mmcamcorder_conf_get_value_int(hcamcorder->conf_main,
	                                CONFIGURE_CATEGORY_MAIN_CAPTURE,
	                                "PlayCaptureSound",
	                                &play_capture_sound);

	_mmcam_dbg_log("Recommend preview rot[capture:%d,recording:%d], dpy rot[%d], play capture sound[%d]",
			rcmd_fmt_capture, rcmd_fmt_recording, rcmd_dpy_rotation, play_capture_sound);

	ret = mm_camcorder_set_attributes((MMHandleType)hcamcorder, &err_attr_name,
	                                  MMCAM_RECOMMEND_PREVIEW_FORMAT_FOR_CAPTURE, rcmd_fmt_capture,
	                                  MMCAM_RECOMMEND_PREVIEW_FORMAT_FOR_RECORDING, rcmd_fmt_recording,
	                                  MMCAM_RECOMMEND_DISPLAY_ROTATION, rcmd_dpy_rotation,
	                                  "capture-sound-enable", play_capture_sound,
	                                  NULL);
	if (ret != MM_ERROR_NONE) {
		_mmcam_dbg_err( "Set %s FAILED.", err_attr_name );
		if (err_attr_name != NULL) {
			free( err_attr_name );
			err_attr_name = NULL;
		}

		goto _ERR_ALLOC_ATTRIBUTE;
	}

	/* Get UseZeroCopyFormat value from INI */
	_mmcamcorder_conf_get_value_int(hcamcorder->conf_main,
	                                CONFIGURE_CATEGORY_MAIN_VIDEO_INPUT,
	                                "UseZeroCopyFormat",
	                                &(hcamcorder->use_zero_copy_format));
	_mmcam_dbg_log("UseZeroCopyFormat : %d", hcamcorder->use_zero_copy_format);

	/* Make some attributes as read-only type */
	__ta__( "    _mmcamcorder_lock_readonly_attributes",
	_mmcamcorder_lock_readonly_attributes((MMHandleType)hcamcorder);
	);

	/* Disable attributes in each model */
	__ta__( "    _mmcamcorder_set_disabled_attributes",
	_mmcamcorder_set_disabled_attributes((MMHandleType)hcamcorder);
	);

	/* Determine state change as sync or async */
	_mmcamcorder_conf_get_value_int(hcamcorder->conf_main,
	                                CONFIGURE_CATEGORY_MAIN_GENERAL,
	                                "SyncStateChange",
	                                &hcamcorder->sync_state_change );
	if (!(hcamcorder->sync_state_change)) {
		_mmcamcorder_create_command_loop((MMHandleType)hcamcorder);
	}

	/* Set initial state */
	_mmcamcorder_set_state((MMHandleType)hcamcorder, MM_CAMCORDER_STATE_NULL);
	_mmcam_dbg_log("_mmcamcorder_set_state");

	*handle = (MMHandleType)hcamcorder;

	return MM_ERROR_NONE;

_ERR_ALLOC_ATTRIBUTE:
_ERR_AUDIO_BLOCKED:
	/* unregister audio session manager */
	{
		sessionType = MM_SESSION_TYPE_EXCLUSIVE;
		errorcode = MM_ERROR_NONE;
		if (MM_ERROR_NONE != _mm_session_util_read_type(-1, &sessionType)) {
			sessionType = MM_SESSION_TYPE_EXCLUSIVE;
		}

		if((sessionType != MM_SESSION_TYPE_CALL) && (sessionType != MM_SESSION_TYPE_VIDEOCALL)) {
			int asm_session_type = __mmcamcorder_asm_get_event_type( sessionType );
			if (!ASM_unregister_sound(hcamcorder->asm_handle, asm_session_type, &errorcode)) {
				_mmcam_dbg_err("ASM_unregister_sound() failed(hdl:%p, stype:%d, err:%x)",
				               (void*)hcamcorder->asm_handle, sessionType, errorcode);
			}
		}
	}

_ERR_DEFAULT_VALUE_INIT:
	g_mutex_free ((hcamcorder->mtsafe).lock);
	g_cond_free ((hcamcorder->mtsafe).cond);
	g_mutex_free ((hcamcorder->mtsafe).cmd_lock);
	g_mutex_free ((hcamcorder->mtsafe).state_lock);
	g_mutex_free ((hcamcorder->mtsafe).gst_state_lock);	

	if (hcamcorder->conf_ctrl) {
		_mmcamcorder_conf_release_info( &hcamcorder->conf_ctrl );
	}

	if (hcamcorder->conf_main) {
		_mmcamcorder_conf_release_info( &hcamcorder->conf_main );
	}

	return ret;
}


int _mmcamcorder_destroy(MMHandleType handle)
{
	int ret = MM_ERROR_NONE;
	int state = MM_CAMCORDER_STATE_NONE;
	int state_FROM = MM_CAMCORDER_STATE_NULL;
	int state_TO = MM_CAMCORDER_STATE_NONE;
	int asm_session_type = ASM_EVENT_EXCLUSIVE_MMCAMCORDER;
	int sessionType = MM_SESSION_TYPE_SHARE;
	int errorcode = MM_ERROR_NONE;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);

	_mmcam_dbg_log("");

	if (!hcamcorder) {
		_mmcam_dbg_err("Not initialized");
		ret = MM_ERROR_CAMCORDER_NOT_INITIALIZED;
		goto _ERR_CAMCORDER_CMD_PRECON;
	}

	if (!_MMCAMCORDER_TRYLOCK_CMD(hcamcorder)) {
		_mmcam_dbg_err("Another command is running.");
		ret = MM_ERROR_CAMCORDER_CMD_IS_RUNNING;
		goto _ERR_CAMCORDER_CMD_PRECON;
	}

	state = _mmcamcorder_get_state(handle);
	if (state != state_FROM) {
		_mmcam_dbg_err("Wrong state(%d)", state);
		ret = MM_ERROR_CAMCORDER_INVALID_STATE;
		goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
	}

	/* Set async state */
	ret = _mmcamcorder_set_async_state(handle, state_TO);
	if (ret < 0) {
		_mmcam_dbg_err("Can't set async state");
		goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
	}

	/* Release sound handle */
	__ta__("_mmcamcorder_sound_finalize",
	ret = _mmcamcorder_sound_finalize(handle);
	);
	_mmcam_dbg_log("sound finalize [%d]", ret);

	/* Release SubContext and pipeline */
	if (hcamcorder->sub_context) {
		if (hcamcorder->sub_context->element) {
			__ta__("    _mmcamcorder_destroy_pipeline",
			_mmcamcorder_destroy_pipeline(handle, hcamcorder->type);
			);
		}

		_mmcamcorder_dealloc_subcontext(hcamcorder->sub_context);
		hcamcorder->sub_context = NULL;
	}

	/* Remove idle function which is not called yet */
	if (hcamcorder->setting_event_id) {
		_mmcam_dbg_log("Remove remaining idle function");
		g_source_remove(hcamcorder->setting_event_id);
		hcamcorder->setting_event_id = 0;
	}

	/* Remove attributes */
	if (hcamcorder->attributes) {
		_mmcamcorder_dealloc_attribute(hcamcorder->attributes);
		hcamcorder->attributes = 0;
	}

	/* Remove exif info */
	if (hcamcorder->exif_info) {
		mm_exif_destory_exif_info(hcamcorder->exif_info);
		hcamcorder->exif_info=NULL;

	}

	/* Remove command loop when async state change mode */
	if (!hcamcorder->sync_state_change) {
		_mmcamcorder_destroy_command_loop(handle);
	}

	/* Release configure info */
	if (hcamcorder->conf_ctrl) {
		_mmcamcorder_conf_release_info( &hcamcorder->conf_ctrl );
	}
	if (hcamcorder->conf_main) {
		_mmcamcorder_conf_release_info( &hcamcorder->conf_main );
	}

	/* Remove messages which are not called yet */
	_mmcamcroder_remove_message_all(handle);

	/* Unregister ASM */
	if (MM_ERROR_NONE != _mm_session_util_read_type(-1, &sessionType)) {
		_mmcam_dbg_err("_mm_session_util_read_type Fail");
	}
	if ((sessionType != MM_SESSION_TYPE_CALL) && (sessionType != MM_SESSION_TYPE_VIDEOCALL)) {
		asm_session_type = __mmcamcorder_asm_get_event_type(sessionType);
		if (!ASM_unregister_sound(hcamcorder->asm_handle, asm_session_type, &errorcode)) {
			_mmcam_dbg_err("ASM_unregister_sound() failed(hdl:%p, stype:%d, err:%x)",
			               (void*)hcamcorder->asm_handle, sessionType, errorcode);
		}
	}

	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

	/* Release lock, cond */
	if ((hcamcorder->mtsafe).lock) {
		g_mutex_free ((hcamcorder->mtsafe).lock);
		(hcamcorder->mtsafe).lock = NULL;
	}
	if ((hcamcorder->mtsafe).cond) {
		g_cond_free ((hcamcorder->mtsafe).cond);
		(hcamcorder->mtsafe).cond = NULL;
	}
	if ((hcamcorder->mtsafe).cmd_lock) {
		g_mutex_free ((hcamcorder->mtsafe).cmd_lock);
		(hcamcorder->mtsafe).cmd_lock = NULL;
	}
	if ((hcamcorder->mtsafe).state_lock) {
		g_mutex_free ((hcamcorder->mtsafe).state_lock);
		(hcamcorder->mtsafe).state_lock = NULL;
	}
	if ((hcamcorder->mtsafe).gst_state_lock) {
		g_mutex_free ((hcamcorder->mtsafe).gst_state_lock);
		(hcamcorder->mtsafe).gst_state_lock = NULL;
	}
	if ((hcamcorder->mtsafe).message_cb_lock) {
		g_mutex_free ((hcamcorder->mtsafe).message_cb_lock);
		(hcamcorder->mtsafe).message_cb_lock = NULL;
	}
	if ((hcamcorder->mtsafe).vcapture_cb_lock) {
		g_mutex_free ((hcamcorder->mtsafe).vcapture_cb_lock);
		(hcamcorder->mtsafe).vcapture_cb_lock = NULL;
	}
	if ((hcamcorder->mtsafe).vstream_cb_lock) {
		g_mutex_free ((hcamcorder->mtsafe).vstream_cb_lock);
		(hcamcorder->mtsafe).vstream_cb_lock = NULL;
	}
	if ((hcamcorder->mtsafe).astream_cb_lock) {
		g_mutex_free ((hcamcorder->mtsafe).astream_cb_lock);
		(hcamcorder->mtsafe).astream_cb_lock = NULL;
	}

	pthread_mutex_destroy(&(hcamcorder->sound_lock));
	pthread_cond_destroy(&(hcamcorder->sound_cond));
	pthread_mutex_destroy(&(hcamcorder->snd_info.open_mutex));
	pthread_cond_destroy(&(hcamcorder->snd_info.open_cond));

	/* Release handle */
	memset(hcamcorder, 0x00, sizeof(mmf_camcorder_t));
	free(hcamcorder);

	return MM_ERROR_NONE;

_ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK:
	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

_ERR_CAMCORDER_CMD_PRECON:
	if (hcamcorder) {
		_mmcam_dbg_err("Destroy fail (type %d, state %d)", hcamcorder->type, state);
	}

	_mmcam_dbg_err("Destroy fail (ret %x)", ret);

	return ret;
}


int _mmcamcorder_realize(MMHandleType handle)
{
	int ret = MM_ERROR_NONE;
	int state = MM_CAMCORDER_STATE_NONE;
	int state_FROM = MM_CAMCORDER_STATE_NULL;
	int state_TO = MM_CAMCORDER_STATE_READY;
	int sessionType = MM_SESSION_TYPE_EXCLUSIVE;
	int errorcode = MM_ERROR_NONE;
	int display_surface_type = MM_DISPLAY_SURFACE_X;
	double motion_rate = _MMCAMCORDER_DEFAULT_RECORDING_MOTION_RATE;
	char *videosink_element_type = NULL;
	char *videosink_name = NULL;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);

	_mmcam_dbg_log("");

	if (!hcamcorder) {
		_mmcam_dbg_err("Not initialized");
		ret = MM_ERROR_CAMCORDER_NOT_INITIALIZED;
		return ret;
	}

	/* Check quick-device-close for emergency */
	if (hcamcorder->quick_device_close) {
		_mmcam_dbg_err("_mmcamcorder_realize can't be called!!!!");
		ret = MM_ERROR_CAMCORDER_DEVICE;
		goto _ERR_CAMCORDER_CMD_PRECON;
	}

	if (!_MMCAMCORDER_TRYLOCK_CMD(hcamcorder)) {
		_mmcam_dbg_err("Another command is running.");
		ret = MM_ERROR_CAMCORDER_CMD_IS_RUNNING;
		goto _ERR_CAMCORDER_CMD_PRECON;
	}

	state = _mmcamcorder_get_state(handle);
	if (state != state_FROM) {
		_mmcam_dbg_err("Wrong state(%d)", state);
		ret = MM_ERROR_CAMCORDER_INVALID_STATE;
		goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
	}

	/* Set async state */
	ret = _mmcamcorder_set_async_state(handle, state_TO);
	if (ret < 0) {
		_mmcam_dbg_err("Can't set async state");
		goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
	}

	mm_camcorder_get_attributes(handle, NULL,
	                            MMCAM_MODE, &hcamcorder->type,
	                            MMCAM_DISPLAY_SURFACE, &display_surface_type,
	                            MMCAM_CAMERA_RECORDING_MOTION_RATE, &motion_rate,
	                            NULL);

	/* Get profile mode */
	_mmcam_dbg_log("Profile mode set is (%d)", hcamcorder->type);

	/* Check and register ASM */
	if (MM_ERROR_NONE != _mm_session_util_read_type(-1, &sessionType)) {
		_mmcam_dbg_warn("Read _mm_session_util_read_type failed. use default \"exclusive\" type");
		sessionType = MM_SESSION_TYPE_EXCLUSIVE;
	}
	if ((sessionType != MM_SESSION_TYPE_CALL) && (sessionType != MM_SESSION_TYPE_VIDEOCALL)) {
		int asm_session_type = ASM_EVENT_NONE;
		ASM_resource_t mm_resource = ASM_RESOURCE_NONE;
		
		asm_session_type = __mmcamcorder_asm_get_event_type(sessionType);
		switch (hcamcorder->type) {
		case MM_CAMCORDER_MODE_VIDEO:
			mm_resource = ASM_RESOURCE_CAMERA | ASM_RESOURCE_VIDEO_OVERLAY | ASM_RESOURCE_HW_ENCODER;
			break;
		case MM_CAMCORDER_MODE_AUDIO:
			mm_resource = ASM_RESOURCE_NONE;
			break;
		case MM_CAMCORDER_MODE_IMAGE:
		default:
			mm_resource = ASM_RESOURCE_CAMERA | ASM_RESOURCE_VIDEO_OVERLAY;
			break;
		}

		if (!ASM_set_sound_state(hcamcorder->asm_handle, asm_session_type,
		                         ASM_STATE_PLAYING, mm_resource, &errorcode)) {
			debug_error("Set state to playing failed 0x%X\n", errorcode);
			ret =  MM_ERROR_POLICY_BLOCKED;
			goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
		}
	}

	/* alloc sub context */
	hcamcorder->sub_context = _mmcamcorder_alloc_subcontext(hcamcorder->type);
	if(!hcamcorder->sub_context) {
		ret = MM_ERROR_CAMCORDER_RESOURCE_CREATION;
		goto _ERR_CAMCORDER_CMD;
	}

	/* Set basic configure information */
	if (motion_rate != _MMCAMCORDER_DEFAULT_RECORDING_MOTION_RATE) {
		hcamcorder->sub_context->is_modified_rate = TRUE;
	}

	switch (display_surface_type) {
	case MM_DISPLAY_SURFACE_X:
		videosink_element_type = strdup("VideosinkElementX");
		break;
	case MM_DISPLAY_SURFACE_EVAS:
		videosink_element_type = strdup("VideosinkElementEvas");
		break;
	case MM_DISPLAY_SURFACE_GL:
		videosink_element_type = strdup("VideosinkElementGL");
		break;
	case MM_DISPLAY_SURFACE_NULL:
		videosink_element_type = strdup("VideosinkElementNull");
		break;
	default:
		videosink_element_type = strdup("VideosinkElementX");
		break;
	}

	/* check string of videosink element */
	if (videosink_element_type) {
		_mmcamcorder_conf_get_element(hcamcorder->conf_main,
		                              CONFIGURE_CATEGORY_MAIN_VIDEO_OUTPUT,
		                              videosink_element_type,
		                              &hcamcorder->sub_context->VideosinkElement );

		free(videosink_element_type);
		videosink_element_type = NULL;
	} else {
		_mmcam_dbg_warn("strdup failed(display_surface_type %d). Use default X type",
		                display_surface_type);

		_mmcamcorder_conf_get_element(hcamcorder->conf_main,
		                              CONFIGURE_CATEGORY_MAIN_VIDEO_OUTPUT,
		                              _MMCAMCORDER_DEFAULT_VIDEOSINK_TYPE,
		                              &hcamcorder->sub_context->VideosinkElement );
	}

	_mmcamcorder_conf_get_value_element_name(hcamcorder->sub_context->VideosinkElement, &videosink_name);
	_mmcam_dbg_log("Videosink name : %s", videosink_name);

	_mmcamcorder_conf_get_value_int(hcamcorder->conf_ctrl,
	                                CONFIGURE_CATEGORY_CTRL_CAPTURE,
	                                "SensorEncodedCapture",
	                                &(hcamcorder->sub_context->SensorEncodedCapture));
	_mmcam_dbg_log("Support sensor encoded capture : %d", hcamcorder->sub_context->SensorEncodedCapture);

	/* create pipeline */
	__ta__("    _mmcamcorder_create_pipeline",
	ret = _mmcamcorder_create_pipeline(handle, hcamcorder->type);
	);
	if (ret != MM_ERROR_NONE) {
		/* check internal error of gstreamer */
		if (hcamcorder->sub_context->error_code != MM_ERROR_NONE) {
			ret = hcamcorder->sub_context->error_code;
			_mmcam_dbg_log("gstreamer error is occurred. return it %x", ret);
		}

		/* release sub context */
		_mmcamcorder_dealloc_subcontext(hcamcorder->sub_context);
		hcamcorder->sub_context = NULL;
		goto _ERR_CAMCORDER_CMD;
	}

	/* set command function */
	ret = _mmcamcorder_set_functions(handle, hcamcorder->type);
	if (ret != MM_ERROR_NONE) {
		_mmcamcorder_destroy_pipeline(handle, hcamcorder->type);
		_mmcamcorder_dealloc_subcontext(hcamcorder->sub_context);
		hcamcorder->sub_context = NULL;
		goto _ERR_CAMCORDER_CMD;
	}

	_mmcamcorder_set_state(handle, state_TO);

	/* set camera state to vconf key */
	if (hcamcorder->type != MM_CAMCORDER_MODE_AUDIO) {
		int vconf_camera_state = 0;

		/* get current camera state of vconf key */
		vconf_get_int(VCONFKEY_CAMERA_STATE, &vconf_camera_state);
		vconf_set_int(VCONFKEY_CAMERA_STATE, VCONFKEY_CAMERA_STATE_OPEN);

		_mmcam_dbg_log("VCONFKEY_CAMERA_STATE prev %d -> cur %d",
		               vconf_camera_state, VCONFKEY_CAMERA_STATE_OPEN);
	}

	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

	return MM_ERROR_NONE;

_ERR_CAMCORDER_CMD:
	/* set async state and (set async cancel or set state) are pair. */
	_mmcamcorder_set_async_cancel(handle);

_ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK:
	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);
	
_ERR_CAMCORDER_CMD_PRECON:
	_mmcam_dbg_err("Realize fail (type %d, state %d, ret %x)",
	               hcamcorder->type, state, ret);

	return ret;
}


int _mmcamcorder_unrealize(MMHandleType handle)
{
	int ret = MM_ERROR_NONE;
	int state = MM_CAMCORDER_STATE_NONE;
	int state_FROM = MM_CAMCORDER_STATE_READY;
	int state_TO = MM_CAMCORDER_STATE_NULL;
	int sessionType = MM_SESSION_TYPE_SHARE;
	int asm_session_type = ASM_EVENT_NONE;
	ASM_resource_t mm_resource = ASM_RESOURCE_NONE;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);

	_mmcam_dbg_log("");

	if (!hcamcorder) {
		_mmcam_dbg_err("Not initialized");
		ret = MM_ERROR_CAMCORDER_NOT_INITIALIZED;
		return ret;
	}

	if (!_MMCAMCORDER_TRYLOCK_CMD(hcamcorder)) {
		_mmcam_dbg_err("Another command is running.");
		ret = MM_ERROR_CAMCORDER_CMD_IS_RUNNING;
		goto _ERR_CAMCORDER_CMD_PRECON;
	}

	state = _mmcamcorder_get_state(handle);
	if (state != state_FROM) {
		_mmcam_dbg_err("Wrong state(%d)", state);
		ret = MM_ERROR_CAMCORDER_INVALID_STATE;
		goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
	}

	/* set async state */
	ret = _mmcamcorder_set_async_state(handle, state_TO);
	if (ret != MM_ERROR_NONE) {
		_mmcam_dbg_err("Can't set async state");
		goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
	}

	/* Release sound handle */
	__ta__("_mmcamcorder_sound_finalize",
	ret = _mmcamcorder_sound_finalize(handle);
	);
	_mmcam_dbg_log("sound finalize [%d]", ret);

	/* Release SubContext */
	if (hcamcorder->sub_context) {
		/* destroy pipeline */
		_mmcamcorder_destroy_pipeline(handle, hcamcorder->type);
		/* Deallocate SubContext */
		_mmcamcorder_dealloc_subcontext(hcamcorder->sub_context);
		hcamcorder->sub_context = NULL;
	}

	/* Deinitialize main context member */

	hcamcorder->command = NULL;

	/* check who calls unrealize. it's no need to set ASM state if caller is ASM */
	if (hcamcorder->state_change_by_system != _MMCAMCORDER_STATE_CHANGE_BY_ASM) {
		if (MM_ERROR_NONE != _mm_session_util_read_type(-1, &sessionType)) {
			_mmcam_dbg_err("_mm_session_util_read_type Fail\n");
		}

		if ((sessionType != MM_SESSION_TYPE_CALL) && (sessionType != MM_SESSION_TYPE_VIDEOCALL)) {
			asm_session_type = ASM_EVENT_NONE;
			mm_resource = ASM_RESOURCE_NONE;
			asm_session_type = __mmcamcorder_asm_get_event_type(sessionType);

			switch (hcamcorder->type) {
			case MM_CAMCORDER_MODE_VIDEO:
				mm_resource = ASM_RESOURCE_CAMERA | ASM_RESOURCE_VIDEO_OVERLAY | ASM_RESOURCE_HW_ENCODER;
				break;
			case MM_CAMCORDER_MODE_AUDIO:
				mm_resource = ASM_RESOURCE_NONE;
				break;
			case MM_CAMCORDER_MODE_IMAGE:
			default:
				mm_resource = ASM_RESOURCE_CAMERA | ASM_RESOURCE_VIDEO_OVERLAY;
				break;
			}

			/* Call session is not ended here */
			if (!ASM_set_sound_state(hcamcorder->asm_handle, asm_session_type,
						ASM_STATE_STOP, mm_resource, &ret)) {
				_mmcam_dbg_err("Set state to playing failed 0x%X\n", ret);
			}
		}
	}

	_mmcamcorder_set_state(handle, state_TO);

	/* set camera state to vconf key */
	if (hcamcorder->type != MM_CAMCORDER_MODE_AUDIO) {
		int vconf_camera_state = 0;

		/* get current camera state of vconf key */
		vconf_get_int(VCONFKEY_CAMERA_STATE, &vconf_camera_state);
		vconf_set_int(VCONFKEY_CAMERA_STATE, VCONFKEY_CAMERA_STATE_NULL);

		_mmcam_dbg_log("VCONFKEY_CAMERA_STATE prev %d -> cur %d",
		               vconf_camera_state, VCONFKEY_CAMERA_STATE_NULL);
	}

	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

	return MM_ERROR_NONE;

_ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK:
	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

_ERR_CAMCORDER_CMD_PRECON:
	/* send message */
	_mmcam_dbg_err("Unrealize fail (type %d, state %d, ret %x)",
	               hcamcorder->type, state, ret);

	return ret;
}

int _mmcamcorder_start(MMHandleType handle)
{
	int ret = MM_ERROR_NONE;
	int state = MM_CAMCORDER_STATE_NONE;
	int state_FROM = MM_CAMCORDER_STATE_READY;
	int state_TO =MM_CAMCORDER_STATE_PREPARE;

	_MMCamcorderSubContext *sc = NULL;
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);

	_mmcam_dbg_log("");

	if (!hcamcorder) {
		_mmcam_dbg_err("Not initialized");
		ret = MM_ERROR_CAMCORDER_NOT_INITIALIZED;
		return ret;
	}

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	mmf_return_val_if_fail(sc, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	/* check quick-device-close for emergency */
	if (hcamcorder->quick_device_close) {
		_mmcam_dbg_err("_mmcamcorder_start can't be called!!!!");
		ret = MM_ERROR_CAMCORDER_DEVICE;
		goto _ERR_CAMCORDER_CMD_PRECON;
	}

	if (!_MMCAMCORDER_TRYLOCK_CMD(hcamcorder)) {
		_mmcam_dbg_err("Another command is running.");
		ret = MM_ERROR_CAMCORDER_CMD_IS_RUNNING;
		goto _ERR_CAMCORDER_CMD_PRECON;
	}

	state = _mmcamcorder_get_state(handle);
	if (state != state_FROM) {
		_mmcam_dbg_err("Wrong state(%d)", state);
		ret = MM_ERROR_CAMCORDER_INVALID_STATE;
		goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
	}

	/* initialize error code */
	hcamcorder->sub_context->error_code = MM_ERROR_NONE;

	/* set async state */
	ret = _mmcamcorder_set_async_state(handle, state_TO);
	if (ret != MM_ERROR_NONE) {
		_mmcam_dbg_err("Can't set async state");
		goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
	}

	ret = hcamcorder->command((MMHandleType)hcamcorder, _MMCamcorder_CMD_PREVIEW_START);
	if (ret != MM_ERROR_NONE) {
		goto _ERR_CAMCORDER_CMD;
	}

	_mmcamcorder_set_state(handle, state_TO);

	if (hcamcorder->type != MM_CAMCORDER_MODE_AUDIO) {
		int vconf_camera_state = 0;

		_mmcamcorder_set_attribute_to_camsensor(handle);

		/* check camera state of vconf key */
		vconf_get_int(VCONFKEY_CAMERA_STATE, &vconf_camera_state);

		/* set camera state to vconf key */
		vconf_set_int(VCONFKEY_CAMERA_STATE, VCONFKEY_CAMERA_STATE_PREVIEW);

		_mmcam_dbg_log("VCONFKEY_CAMERA_STATE prev %d -> cur %d",
		               vconf_camera_state, VCONFKEY_CAMERA_STATE_PREVIEW);
	}

	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

	return MM_ERROR_NONE;

_ERR_CAMCORDER_CMD:
	/* set async state and (set async cancel or set state) are pair. */
	_mmcamcorder_set_async_cancel(handle);
	
_ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK:
	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

_ERR_CAMCORDER_CMD_PRECON:
	/* check internal error of gstreamer */
	if (hcamcorder->sub_context->error_code != MM_ERROR_NONE) {
		ret = hcamcorder->sub_context->error_code;
		hcamcorder->sub_context->error_code = MM_ERROR_NONE;

		_mmcam_dbg_log("gstreamer error is occurred. return it %x", ret);
	}

	_mmcam_dbg_err("Start fail (type %d, state %d, ret %x)",
	               hcamcorder->type, state, ret);

	return ret;
}

int _mmcamcorder_stop(MMHandleType handle)
{
	int ret = MM_ERROR_NONE;
	int state = MM_CAMCORDER_STATE_NONE;
	int state_FROM = MM_CAMCORDER_STATE_PREPARE;
	int state_TO = MM_CAMCORDER_STATE_READY;
	int frame_rate = 0;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);

	_mmcam_dbg_log("");

	if (!hcamcorder) {
		_mmcam_dbg_err("Not initialized");
		ret = MM_ERROR_CAMCORDER_NOT_INITIALIZED;
		return ret;
	}

	if (!_MMCAMCORDER_TRYLOCK_CMD(hcamcorder)) {
		_mmcam_dbg_err("Another command is running.");
		ret = MM_ERROR_CAMCORDER_CMD_IS_RUNNING;
		goto _ERR_CAMCORDER_CMD_PRECON;
	}

	state = _mmcamcorder_get_state(handle);
	if (state != state_FROM) {
		_mmcam_dbg_err("Wrong state(%d)", state);
		ret = MM_ERROR_CAMCORDER_INVALID_STATE;
		goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
	}

	/* set async state */
	ret = _mmcamcorder_set_async_state(handle, state_TO);
	if (ret != MM_ERROR_NONE) {
		_mmcam_dbg_err("Can't set async state");
		goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
	}

	ret = hcamcorder->command((MMHandleType)hcamcorder, _MMCamcorder_CMD_PREVIEW_STOP);
	if (ret != MM_ERROR_NONE) {
		goto _ERR_CAMCORDER_CMD;
	}

	/* KPI : frame rate */
	frame_rate=_mmcamcorder_video_average_framerate(handle);
	__ta__(__tafmt__("MM_CAM_006:: Frame per sec : %d", frame_rate), ;);

	_mmcamcorder_set_state(handle, state_TO);

	if (hcamcorder->type != MM_CAMCORDER_MODE_AUDIO) {
		int vconf_camera_state = 0;

		/* check camera state of vconf key */
		vconf_get_int(VCONFKEY_CAMERA_STATE, &vconf_camera_state);

		/* set camera state to vconf key */
		vconf_set_int(VCONFKEY_CAMERA_STATE, VCONFKEY_CAMERA_STATE_OPEN);

		_mmcam_dbg_log("VCONFKEY_CAMERA_STATE prev %d -> cur %d",
		               vconf_camera_state, VCONFKEY_CAMERA_STATE_OPEN);
	}

	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

	return MM_ERROR_NONE;

_ERR_CAMCORDER_CMD:
	/* set async state and (set async cancel or set state) are pair. */
	_mmcamcorder_set_async_cancel(handle);

_ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK:
	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

_ERR_CAMCORDER_CMD_PRECON:
	/* send message */
	_mmcam_dbg_err("Stop fail (type %d, state %d, ret %x)",
	               hcamcorder->type, state, ret);

	return ret;
}


int _mmcamcorder_capture_start(MMHandleType handle)
{
	int ret = MM_ERROR_NONE;
	int state = MM_CAMCORDER_STATE_NONE;
	int state_FROM = MM_CAMCORDER_STATE_PREPARE;
	int state_FROM2 = MM_CAMCORDER_STATE_RECORDING;
	int state_TO = MM_CAMCORDER_STATE_CAPTURING;
	char *err_name = NULL;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);

	_mmcam_dbg_log("");

	if (!hcamcorder) {
		_mmcam_dbg_err("Not initialized");
		ret = MM_ERROR_CAMCORDER_NOT_INITIALIZED;
		return ret;
	}

	if (!_MMCAMCORDER_TRYLOCK_CMD(hcamcorder)) {
		_mmcam_dbg_err("Another command is running.");
		ret = MM_ERROR_CAMCORDER_CMD_IS_RUNNING;
		goto _ERR_CAMCORDER_CMD_PRECON;
	}

	state = _mmcamcorder_get_state(handle);
	if (state != state_FROM && state != state_FROM2) {
		_mmcam_dbg_err("Wrong state(%d)", state);
		ret = MM_ERROR_CAMCORDER_INVALID_STATE;
		goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
	}

	/* set async state */
	ret = _mmcamcorder_set_async_state(handle, state_TO);
	if (ret != MM_ERROR_NONE) {
		_mmcam_dbg_err("Can't set async state");
		goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
	}

	ret = hcamcorder->command((MMHandleType)hcamcorder, _MMCamcorder_CMD_CAPTURE);
	if (ret != MM_ERROR_NONE) {
		goto _ERR_CAMCORDER_CMD;
	}

	/* Do not change state when recording snapshot capture */
	if (state == state_FROM) {
		_mmcamcorder_set_state(handle, state_TO);
	}

	/* Init break continuous shot attr */
	mm_camcorder_set_attributes(handle, &err_name, "capture-break-cont-shot", 0, NULL);

	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

	return MM_ERROR_NONE;

_ERR_CAMCORDER_CMD:
	/* set async state and (set async cancel or set state) are pair. */
	_mmcamcorder_set_async_cancel(handle);

_ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK:
	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

_ERR_CAMCORDER_CMD_PRECON:
	/* send message */
	_mmcam_dbg_err("Capture start fail (type %d, state %d, ret %x)",
	               hcamcorder->type, state, ret);

	return ret;
}

int _mmcamcorder_capture_stop(MMHandleType handle)
{
	int ret = MM_ERROR_NONE;
	int state = MM_CAMCORDER_STATE_NONE;
	int state_FROM = MM_CAMCORDER_STATE_CAPTURING;
	int state_TO = MM_CAMCORDER_STATE_PREPARE;
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);

	_mmcam_dbg_log("");

	if (!hcamcorder) {
		_mmcam_dbg_err("Not initialized");
		ret = MM_ERROR_CAMCORDER_NOT_INITIALIZED;
		return ret;
	}

	if (!_MMCAMCORDER_TRYLOCK_CMD(hcamcorder)) {
		_mmcam_dbg_err("Another command is running.");
		ret = MM_ERROR_CAMCORDER_CMD_IS_RUNNING;
		goto _ERR_CAMCORDER_CMD_PRECON;
	}

	state = _mmcamcorder_get_state(handle);
	if (state != state_FROM) {
		_mmcam_dbg_err("Wrong state(%d)", state);
		ret = MM_ERROR_CAMCORDER_INVALID_STATE;
		goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
	}

	/* set async state */
	ret = _mmcamcorder_set_async_state(handle, state_TO);
	if (ret != MM_ERROR_NONE) {
		_mmcam_dbg_err("Can't set async state");
		goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
	}

	ret = hcamcorder->command((MMHandleType)hcamcorder, _MMCamcorder_CMD_PREVIEW_START);
	if (ret != MM_ERROR_NONE) {
		goto _ERR_CAMCORDER_CMD;
	}

	_mmcamcorder_set_state(handle, state_TO);

	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

	MMTA_ACUM_ITEM_END("Real First Capture Start", FALSE);

	return MM_ERROR_NONE;

_ERR_CAMCORDER_CMD:
	/* set async state and (set async cancel or set state) are pair. */
	_mmcamcorder_set_async_cancel(handle);

_ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK:
	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

_ERR_CAMCORDER_CMD_PRECON:
	/* send message */
	_mmcam_dbg_err("Capture stop fail (type %d, state %d, ret %x)",
	               hcamcorder->type, state, ret);

	return ret;
}

int _mmcamcorder_record(MMHandleType handle)
{
	int ret = MM_ERROR_NONE;
	int state = MM_CAMCORDER_STATE_NONE;
	int state_FROM1 = MM_CAMCORDER_STATE_PREPARE;
	int state_FROM2 = MM_CAMCORDER_STATE_PAUSED;
	int state_TO = MM_CAMCORDER_STATE_RECORDING;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);

	_mmcam_dbg_log("");

	if (!hcamcorder) {
		_mmcam_dbg_err("Not initialized");
		ret = MM_ERROR_CAMCORDER_NOT_INITIALIZED;
		return ret;
	}

	if (!_MMCAMCORDER_TRYLOCK_CMD(hcamcorder)) {
		_mmcam_dbg_err("Another command is running.");
		ret = MM_ERROR_CAMCORDER_CMD_IS_RUNNING;
		goto _ERR_CAMCORDER_CMD_PRECON;
	}

	state = _mmcamcorder_get_state(handle);
	if (state != state_FROM1 && state != state_FROM2) {
		_mmcam_dbg_err("Wrong state(%d)", state);
		ret = MM_ERROR_CAMCORDER_INVALID_STATE;
		goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
	}

	/* initialize error code */
	hcamcorder->sub_context->error_code = MM_ERROR_NONE;

	/* set async state */
	ret = _mmcamcorder_set_async_state(handle, state_TO);
	if (ret != MM_ERROR_NONE) {
		_mmcam_dbg_err("Can't set async state");
		goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
	}

	ret = hcamcorder->command((MMHandleType)hcamcorder, _MMCamcorder_CMD_RECORD);
	if (ret != MM_ERROR_NONE) {
		goto _ERR_CAMCORDER_CMD;
	}

	_mmcamcorder_set_state(handle, state_TO);

	/* set camera state to vconf key */
	if (hcamcorder->type != MM_CAMCORDER_MODE_AUDIO) {
		int vconf_camera_state = 0;

		/* get current camera state of vconf key */
		vconf_get_int(VCONFKEY_CAMERA_STATE, &vconf_camera_state);
		vconf_set_int(VCONFKEY_CAMERA_STATE, VCONFKEY_CAMERA_STATE_RECORDING);

		_mmcam_dbg_log("VCONFKEY_CAMERA_STATE prev %d -> cur %d",
		               vconf_camera_state, VCONFKEY_CAMERA_STATE_RECORDING);
	}

	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

	return MM_ERROR_NONE;

_ERR_CAMCORDER_CMD:
	/* set async state and (set async cancel or set state) are pair. */
	_mmcamcorder_set_async_cancel(handle);

_ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK:
	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

_ERR_CAMCORDER_CMD_PRECON:
	/* check internal error of gstreamer */
	if (hcamcorder->sub_context->error_code != MM_ERROR_NONE) {
		ret = hcamcorder->sub_context->error_code;
		hcamcorder->sub_context->error_code = MM_ERROR_NONE;

		_mmcam_dbg_log("gstreamer error is occurred. return it %x", ret);
	}

	_mmcam_dbg_err("Record fail (type %d, state %d, ret %x)",
	               hcamcorder->type, state, ret);

	return ret;
}


int _mmcamcorder_pause(MMHandleType handle)
{
	int ret = MM_ERROR_NONE;
	int state = MM_CAMCORDER_STATE_NONE;
	int state_FROM = MM_CAMCORDER_STATE_RECORDING;
	int state_TO = MM_CAMCORDER_STATE_PAUSED;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);

	_mmcam_dbg_log("");

	if (!hcamcorder) {
		_mmcam_dbg_err("Not initialized");
		ret = MM_ERROR_CAMCORDER_NOT_INITIALIZED;
		return ret;
	}

	if (!_MMCAMCORDER_TRYLOCK_CMD(hcamcorder)) {
		_mmcam_dbg_err("Another command is running.");
		ret = MM_ERROR_CAMCORDER_CMD_IS_RUNNING;
		goto _ERR_CAMCORDER_CMD_PRECON;
	}

	state = _mmcamcorder_get_state(handle);
	if (state != state_FROM) {
		_mmcam_dbg_err("Wrong state(%d)", state);
		ret = MM_ERROR_CAMCORDER_INVALID_STATE;
		goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
	}

	/* set async state */
	ret = _mmcamcorder_set_async_state(handle, state_TO);
	if (ret != MM_ERROR_NONE) {
		_mmcam_dbg_err("Can't set async state");
		goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
	}

	ret = hcamcorder->command((MMHandleType)hcamcorder, _MMCamcorder_CMD_PAUSE);
	if (ret != MM_ERROR_NONE) {
		goto _ERR_CAMCORDER_CMD;
	}

	_mmcamcorder_set_state(handle, state_TO);

	/* set camera state to vconf key */
	if (hcamcorder->type != MM_CAMCORDER_MODE_AUDIO) {
		int vconf_camera_state = 0;

		/* get current camera state of vconf key */
		vconf_get_int(VCONFKEY_CAMERA_STATE, &vconf_camera_state);
		vconf_set_int(VCONFKEY_CAMERA_STATE, VCONFKEY_CAMERA_STATE_RECORDING_PAUSE);

		_mmcam_dbg_log("VCONFKEY_CAMERA_STATE prev %d -> cur %d",
		               vconf_camera_state, VCONFKEY_CAMERA_STATE_RECORDING_PAUSE);
	}

	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

	return MM_ERROR_NONE;

_ERR_CAMCORDER_CMD:
	/* set async state and (set async cancel or set state) are pair. */
	_mmcamcorder_set_async_cancel(handle);

_ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK:
	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

_ERR_CAMCORDER_CMD_PRECON:
	/* send message */
	_mmcam_dbg_err("Pause fail (type %d, state %d, ret %x)",
	               hcamcorder->type, state, ret);

	return ret;
}


int _mmcamcorder_commit(MMHandleType handle)
{
	int ret = MM_ERROR_NONE;
	int state = MM_CAMCORDER_STATE_NONE;
	int state_FROM1 = MM_CAMCORDER_STATE_RECORDING;
	int state_FROM2 = MM_CAMCORDER_STATE_PAUSED;
	int state_TO = MM_CAMCORDER_STATE_PREPARE;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	
	_mmcam_dbg_log("");

	if (!hcamcorder) {
		_mmcam_dbg_err("Not initialized");
		ret = MM_ERROR_CAMCORDER_NOT_INITIALIZED;
		return ret;
	}

	if (!_MMCAMCORDER_TRYLOCK_CMD(hcamcorder)) {
		_mmcam_dbg_err("Another command is running.");
		ret = MM_ERROR_CAMCORDER_CMD_IS_RUNNING;
		goto _ERR_CAMCORDER_CMD_PRECON;
	}

	state = _mmcamcorder_get_state(handle);
	if (state != state_FROM1 && state != state_FROM2) {
		_mmcam_dbg_err("Wrong state(%d)", state);
		ret = MM_ERROR_CAMCORDER_INVALID_STATE;
		goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
	}

	/* set async state */
	ret = _mmcamcorder_set_async_state(handle, state_TO);
	if (ret != MM_ERROR_NONE) {
		_mmcam_dbg_err("Can't set async state");
		goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
	}

	ret = hcamcorder->command((MMHandleType)hcamcorder, _MMCamcorder_CMD_COMMIT);
	if (ret != MM_ERROR_NONE) {
		goto _ERR_CAMCORDER_CMD;
	}

	_mmcamcorder_set_state(handle,state_TO);

	/* set camera state to vconf key */
	if (hcamcorder->type != MM_CAMCORDER_MODE_AUDIO) {
		int vconf_camera_state = 0;

		/* get current camera state of vconf key */
		vconf_get_int(VCONFKEY_CAMERA_STATE, &vconf_camera_state);
		vconf_set_int(VCONFKEY_CAMERA_STATE, VCONFKEY_CAMERA_STATE_PREVIEW);

		_mmcam_dbg_log("VCONFKEY_CAMERA_STATE prev %d -> cur %d",
		               vconf_camera_state, VCONFKEY_CAMERA_STATE_PREVIEW);
	}

	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

	return MM_ERROR_NONE;

_ERR_CAMCORDER_CMD:
	/* set async state and (set async cancel or set state) are pair. */
	_mmcamcorder_set_async_cancel(handle);

_ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK:
	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

_ERR_CAMCORDER_CMD_PRECON:
	/* send message */
	_mmcam_dbg_err("Commit fail (type %d, state %d, ret %x)",
	               hcamcorder->type, state, ret);

	return ret;
}


int _mmcamcorder_cancel(MMHandleType handle)
{
	int ret = MM_ERROR_NONE;
	int state = MM_CAMCORDER_STATE_NONE;
	int state_FROM1 = MM_CAMCORDER_STATE_RECORDING;
	int state_FROM2 = MM_CAMCORDER_STATE_PAUSED;
	int state_TO = MM_CAMCORDER_STATE_PREPARE;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	
	_mmcam_dbg_log("");

	if (!hcamcorder) {
		_mmcam_dbg_err("Not initialized");
		ret = MM_ERROR_CAMCORDER_NOT_INITIALIZED;
		return ret;
	}

	if (!_MMCAMCORDER_TRYLOCK_CMD(hcamcorder)) {
		_mmcam_dbg_err("Another command is running.");
		ret = MM_ERROR_CAMCORDER_CMD_IS_RUNNING;
		goto _ERR_CAMCORDER_CMD_PRECON;
	}

	state = _mmcamcorder_get_state(handle);
	if (state != state_FROM1 && state != state_FROM2) {
		_mmcam_dbg_err("Wrong state(%d)", state);
		ret = MM_ERROR_CAMCORDER_INVALID_STATE;
		goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
	}

	/* set async state */
	ret = _mmcamcorder_set_async_state(handle, state_TO);
	if (ret != MM_ERROR_NONE) {
		_mmcam_dbg_err("Can't set async state");
		goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
	}

	ret = hcamcorder->command((MMHandleType)hcamcorder, _MMCamcorder_CMD_CANCEL);
	if (ret != MM_ERROR_NONE) {
		goto _ERR_CAMCORDER_CMD;
	}

	_mmcamcorder_set_state(handle, state_TO);

	/* set camera state to vconf key */
	if (hcamcorder->type != MM_CAMCORDER_MODE_AUDIO) {
		int vconf_camera_state = 0;

		/* get current camera state of vconf key */
		vconf_get_int(VCONFKEY_CAMERA_STATE, &vconf_camera_state);
		vconf_set_int(VCONFKEY_CAMERA_STATE, VCONFKEY_CAMERA_STATE_PREVIEW);

		_mmcam_dbg_log("VCONFKEY_CAMERA_STATE prev %d -> cur %d",
		               vconf_camera_state, VCONFKEY_CAMERA_STATE_PREVIEW);
	}

	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

	return MM_ERROR_NONE;

_ERR_CAMCORDER_CMD:
	/* set async state and (set async cancel or set state) are pair. */
	_mmcamcorder_set_async_cancel(handle);

_ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK:
	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

_ERR_CAMCORDER_CMD_PRECON:
	/* send message */
	_mmcam_dbg_err("Cancel fail (type %d, state %d, ret %x)",
	               hcamcorder->type, state, ret);

	return ret;
}
/* } Internal command functions */


int _mmcamcorder_commit_async_end(MMHandleType handle)
{
	_mmcam_dbg_log("");

	_mmcam_dbg_warn("_mmcamcorder_commit_async_end : MM_CAMCORDER_STATE_PREPARE");
	_mmcamcorder_set_state(handle, MM_CAMCORDER_STATE_PREPARE);

	return MM_ERROR_NONE;
}


int _mmcamcorder_set_message_callback(MMHandleType handle, MMMessageCallback callback, void *user_data)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);

	_mmcam_dbg_log("%p", hcamcorder);

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	if (callback == NULL) {
		_mmcam_dbg_warn("Message Callback is disabled, because application sets it to NULL");
	}

	if (!_MMCAMCORDER_TRYLOCK_MESSAGE_CALLBACK(hcamcorder)) {
		_mmcam_dbg_warn("Application's message callback is running now");
		return MM_ERROR_CAMCORDER_INVALID_CONDITION;
	}

	/* set message callback to message handle */
	hcamcorder->msg_cb = callback;
	hcamcorder->msg_cb_param = user_data;

	_MMCAMCORDER_UNLOCK_MESSAGE_CALLBACK(hcamcorder);

	return MM_ERROR_NONE;
}


int _mmcamcorder_set_video_stream_callback(MMHandleType handle, mm_camcorder_video_stream_callback callback, void *user_data)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);

	_mmcam_dbg_log("");

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	if (callback == NULL) {
		_mmcam_dbg_warn("Video Stream Callback is disabled, because application sets it to NULL");
	}

	if (!_MMCAMCORDER_TRYLOCK_VSTREAM_CALLBACK(hcamcorder)) {
		_mmcam_dbg_warn("Application's video stream callback is running now");
		return MM_ERROR_CAMCORDER_INVALID_CONDITION;
	}

	hcamcorder->vstream_cb = callback;
	hcamcorder->vstream_cb_param = user_data;

	_MMCAMCORDER_UNLOCK_VSTREAM_CALLBACK(hcamcorder);

	return MM_ERROR_NONE;
}


int _mmcamcorder_set_audio_stream_callback(MMHandleType handle, mm_camcorder_audio_stream_callback callback, void *user_data)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);

	_mmcam_dbg_log("");

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	if (callback == NULL) {
		_mmcam_dbg_warn("Audio Stream Callback is disabled, because application sets it to NULL");
	}

	if (!_MMCAMCORDER_TRYLOCK_ASTREAM_CALLBACK(hcamcorder)) {
		_mmcam_dbg_warn("Application's audio stream callback is running now");
		return MM_ERROR_CAMCORDER_INVALID_CONDITION;
	}

	hcamcorder->astream_cb = callback;
	hcamcorder->astream_cb_param = user_data;

	_MMCAMCORDER_UNLOCK_ASTREAM_CALLBACK(hcamcorder);

	return MM_ERROR_NONE;
}


int _mmcamcorder_set_video_capture_callback(MMHandleType handle, mm_camcorder_video_capture_callback callback, void *user_data)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);

	_mmcam_dbg_log("");

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	if (callback == NULL) {
		_mmcam_dbg_warn("Video Capture Callback is disabled, because application sets it to NULLL");
	}

	if (!_MMCAMCORDER_TRYLOCK_VCAPTURE_CALLBACK(hcamcorder)) {
		_mmcam_dbg_warn("Application's video capture callback is running now");
		return MM_ERROR_CAMCORDER_INVALID_CONDITION;
	}

	hcamcorder->vcapture_cb = callback;
	hcamcorder->vcapture_cb_param = user_data;

	_MMCAMCORDER_UNLOCK_VCAPTURE_CALLBACK(hcamcorder);

	return MM_ERROR_NONE;
}

int _mmcamcorder_get_current_state(MMHandleType handle)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);

	_mmcam_dbg_log("");

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	return _mmcamcorder_get_state(handle);
}

int _mmcamcorder_init_focusing(MMHandleType handle)
{
	int ret = TRUE;
	int state = MM_CAMCORDER_STATE_NONE;
	int focus_mode = MM_CAMCORDER_FOCUS_MODE_NONE;
	int af_range = MM_CAMCORDER_AUTO_FOCUS_NORMAL;
	int sensor_focus_mode = 0;
	int sensor_af_range = 0;
	int current_focus_mode = 0;
	int current_af_range = 0;
	mmf_camcorder_t *hcamcorder = NULL;
	_MMCamcorderSubContext *sc = NULL;
	mmf_attrs_t *attr = NULL;
	GstCameraControl *control = NULL;

	_mmcam_dbg_log("");

	hcamcorder = MMF_CAMCORDER(handle);
	mmf_return_val_if_fail(handle, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	sc = MMF_CAMCORDER_SUBCONTEXT(hcamcorder);
	mmf_return_val_if_fail(sc, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	if (!_MMCAMCORDER_TRYLOCK_CMD(hcamcorder)) {
		_mmcam_dbg_err("Another command is running.");
		return MM_ERROR_CAMCORDER_CMD_IS_RUNNING;
	}

	state = _mmcamcorder_get_state(handle);

	if (state == MM_CAMCORDER_STATE_CAPTURING ||
	    state < MM_CAMCORDER_STATE_PREPARE) {
		_mmcam_dbg_err( "Not proper state. state[%d]", state );
		_MMCAMCORDER_UNLOCK_CMD(hcamcorder);
		return MM_ERROR_CAMCORDER_INVALID_STATE;
	}

	if (!GST_IS_CAMERA_CONTROL(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst)) {
		_mmcam_dbg_log("Can't cast Video source into camera control.");
		_MMCAMCORDER_UNLOCK_CMD(hcamcorder);
		return MM_ERROR_NONE;
	}
	control = GST_CAMERA_CONTROL (sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst);

	ret = gst_camera_control_stop_auto_focus(control);
	if (!ret) {
		_mmcam_dbg_err("Auto focusing stop fail.");
		_MMCAMCORDER_UNLOCK_CMD(hcamcorder);
		return MM_ERROR_CAMCORDER_DEVICE_IO;
	}

	/* Initialize lens position */
	attr = (mmf_attrs_t *)MMF_CAMCORDER_ATTRS(handle);
	mm_camcorder_get_attributes(handle, NULL,
	                            MMCAM_CAMERA_FOCUS_MODE, &focus_mode,
	                            MMCAM_CAMERA_AF_SCAN_RANGE, &af_range,
	                            NULL);
	sensor_af_range = _mmcamcorder_convert_msl_to_sensor(handle, MM_CAM_CAMERA_AF_SCAN_RANGE, af_range);
	sensor_focus_mode = _mmcamcorder_convert_msl_to_sensor(handle, MM_CAM_CAMERA_FOCUS_MODE, focus_mode);

	gst_camera_control_get_focus(control, &current_focus_mode, &current_af_range);

	if (current_focus_mode != sensor_focus_mode ||
	    current_af_range != sensor_af_range) {
		ret = gst_camera_control_set_focus(control, sensor_focus_mode, sensor_af_range);
	} else {
		_mmcam_dbg_log("No need to init FOCUS [mode:%d, range:%d]", focus_mode, af_range );
		ret = TRUE;
	}

	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

	if (ret) {
		_mmcam_dbg_log("Lens init success.");
		return MM_ERROR_NONE;
	} else {
		_mmcam_dbg_err("Lens init fail.");
		return MM_ERROR_CAMCORDER_DEVICE_IO;
	}
}

int _mmcamcorder_adjust_focus(MMHandleType handle, int direction)
{
	int state;
	int focus_mode = MM_CAMCORDER_FOCUS_MODE_NONE;
	int ret = MM_ERROR_UNKNOWN;
	char *err_attr_name = NULL;

	mmf_return_val_if_fail(handle, MM_ERROR_CAMCORDER_NOT_INITIALIZED);
	mmf_return_val_if_fail(direction, MM_ERROR_CAMCORDER_INVALID_ARGUMENT);

	_mmcam_dbg_log("");

	state = _mmcamcorder_get_state(handle);
	if (state == MM_CAMCORDER_STATE_CAPTURING ||
	    state < MM_CAMCORDER_STATE_PREPARE) {
		_mmcam_dbg_err("Not proper state. state[%d]", state);
		return MM_ERROR_CAMCORDER_INVALID_STATE;
	}

	/* TODO : call a auto or manual focus function */
	ret = mm_camcorder_get_attributes(handle, &err_attr_name,
	                                  MMCAM_CAMERA_FOCUS_MODE, &focus_mode,
	                                  NULL);
	if (ret != MM_ERROR_NONE) {
		_mmcam_dbg_warn("Get focus-mode fail. (%s:%x)", err_attr_name, ret);
		SAFE_FREE (err_attr_name);
		return ret;
	}

	if (focus_mode == MM_CAMCORDER_FOCUS_MODE_MANUAL) {
		return _mmcamcorder_adjust_manual_focus(handle, direction);
	} else if (focus_mode == MM_CAMCORDER_FOCUS_MODE_AUTO ||
		   focus_mode == MM_CAMCORDER_FOCUS_MODE_TOUCH_AUTO ||
		   focus_mode == MM_CAMCORDER_FOCUS_MODE_CONTINUOUS) {
		return _mmcamcorder_adjust_auto_focus(handle);
	} else {
		_mmcam_dbg_err("It doesn't adjust focus. Focusing mode(%d)", focus_mode);
		return MM_ERROR_CAMCORDER_NOT_SUPPORTED;
	}
}

int _mmcamcorder_adjust_manual_focus(MMHandleType handle, int direction)
{
	int max_level = 0;
	int min_level = 0;
	int cur_level = 0;
	int focus_level = 0;
	float unit_level = 0;
	GstCameraControl *control = NULL;
	_MMCamcorderSubContext *sc = NULL;
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);

	_mmcam_dbg_log("");

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);
	mmf_return_val_if_fail(_MMFCAMCORDER_FOCUS_TOTAL_LEVEL != 1, MM_ERROR_CAMCORDER_NOT_SUPPORTED);
	mmf_return_val_if_fail((direction >= MM_CAMCORDER_MF_LENS_DIR_FORWARD) &&
	                       (direction <= MM_CAMCORDER_MF_LENS_DIR_BACKWARD),
	                       MM_ERROR_CAMCORDER_INVALID_ARGUMENT );

	sc = MMF_CAMCORDER_SUBCONTEXT(hcamcorder);
	mmf_return_val_if_fail(sc, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	if (!GST_IS_CAMERA_CONTROL(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst)) {
		_mmcam_dbg_log("Can't cast Video source into camera control.");
		return MM_ERROR_NONE;
	}
	control = GST_CAMERA_CONTROL (sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst);

	/* TODO : get max, min level */
	if (max_level - min_level + 1 < _MMFCAMCORDER_FOCUS_TOTAL_LEVEL) {
		_mmcam_dbg_warn("Total level  of manual focus of MMF is greater than that of the camera driver.");
	}

	unit_level = ((float)max_level - (float)min_level)/(float)(_MMFCAMCORDER_FOCUS_TOTAL_LEVEL - 1);

	if (!gst_camera_control_get_focus_level(control, &cur_level)) {
		_mmcam_dbg_err("Can't get current level of manual focus.");
		return MM_ERROR_CAMCORDER_DEVICE_IO;
	}

	//TODO : adjust unit level value
	if (direction == MM_CAMCORDER_MF_LENS_DIR_FORWARD) {
		focus_level = cur_level + unit_level;
	} else if (direction == MM_CAMCORDER_MF_LENS_DIR_BACKWARD) {
		focus_level = cur_level - unit_level;
	}

	if (focus_level > max_level) {
		focus_level = max_level;
	} else if (focus_level < min_level) {
		focus_level = min_level;
	}

	if (!gst_camera_control_set_focus_level(control, focus_level)) {
		_mmcam_dbg_err("Manual focusing fail.");
		return MM_ERROR_CAMCORDER_DEVICE_IO;
	}

	return MM_ERROR_NONE;
}


int _mmcamcorder_adjust_auto_focus(MMHandleType handle)
{
	gboolean ret;
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	GstCameraControl *control = NULL;
	_MMCamcorderSubContext *sc = NULL;

	mmf_return_val_if_fail(handle, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	_mmcam_dbg_log("");

	sc = MMF_CAMCORDER_SUBCONTEXT(hcamcorder);

	if (!GST_IS_CAMERA_CONTROL(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst)) {
		_mmcam_dbg_log("Can't cast Video source into camera control.");
		return MM_ERROR_NONE;
	}
	control = GST_CAMERA_CONTROL (sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst);

	/* Start AF */
	ret = gst_camera_control_start_auto_focus(control);
	if (ret) {
		_mmcam_dbg_log("Auto focusing start success.");
		return MM_ERROR_NONE;
	} else {
		_mmcam_dbg_err("Auto focusing start fail.");
		return MM_ERROR_CAMCORDER_DEVICE_IO;
	}
}

int _mmcamcorder_stop_focusing(MMHandleType handle)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;

	int ret, state;
	GstCameraControl *control = NULL;

	mmf_return_val_if_fail(handle, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	_mmcam_dbg_log("");

	sc = MMF_CAMCORDER_SUBCONTEXT(hcamcorder);

	if (!_MMCAMCORDER_TRYLOCK_CMD(hcamcorder)) {
		_mmcam_dbg_err("Another command is running.");
		return MM_ERROR_CAMCORDER_CMD_IS_RUNNING;
	}

	state = _mmcamcorder_get_state(handle);
	if (state == MM_CAMCORDER_STATE_CAPTURING ||
	    state < MM_CAMCORDER_STATE_PREPARE) {
		_mmcam_dbg_err( "Not proper state. state[%d]", state );
		_MMCAMCORDER_UNLOCK_CMD(hcamcorder);
		return MM_ERROR_CAMCORDER_INVALID_STATE;
	}

	if (!GST_IS_CAMERA_CONTROL(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst)) {
		_mmcam_dbg_log("Can't cast Video source into camera control.");
		_MMCAMCORDER_UNLOCK_CMD(hcamcorder);
		return MM_ERROR_NONE;
	}
	control = GST_CAMERA_CONTROL(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst);

	ret = gst_camera_control_stop_auto_focus(control);

	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

	if (ret) {
		_mmcam_dbg_log("Auto focusing stop success.");
		return MM_ERROR_NONE;
	} else {
		_mmcam_dbg_err("Auto focusing stop fail.");
		return MM_ERROR_CAMCORDER_DEVICE_IO;
	}
}


/*-----------------------------------------------
|	  CAMCORDER INTERNAL LOCAL		|
-----------------------------------------------*/
static gboolean
__mmcamcorder_gstreamer_init(camera_conf * conf)
{
	static const int max_argc = 10;
	int i = 0;
	int cnt_str = 0;
	gint *argc = NULL;
	gchar **argv = NULL;
	GError *err = NULL;
	gboolean ret = FALSE;
	type_string_array *GSTInitOption = NULL;

	mmf_return_val_if_fail(conf, FALSE);

	_mmcam_dbg_log("");

	/* alloc */
	argc = malloc(sizeof(int));
	argv = malloc(sizeof(gchar *) * max_argc);

	if (!argc || !argv) {
		goto ERROR;
	}

	memset(argv, 0, sizeof(gchar *) * max_argc);

	/* add initial */
	*argc = 1;
	argv[0] = g_strdup("mmcamcorder");

	/* add gst_param */
	_mmcamcorder_conf_get_value_string_array(conf,
	                                         CONFIGURE_CATEGORY_MAIN_GENERAL,
	                                         "GSTInitOption",
	                                         &GSTInitOption);
	if (GSTInitOption != NULL && GSTInitOption->value) {
		cnt_str = GSTInitOption->count;
		for( ; *argc < max_argc && *argc <= cnt_str ; (*argc)++ )
		{
			argv[*argc] = g_strdup(GSTInitOption->value[(*argc)-1]);
		}
	}

	_mmcam_dbg_log("initializing gstreamer with following parameter[argc:%d]", *argc);

	for (i = 0; i < *argc; i++) {
		_mmcam_dbg_log("argv[%d] : %s", i, argv[i]);
	}

	/* initializing gstreamer */
	__ta__("        gst_init_check",
	ret = gst_init_check (argc, &argv, &err);
	);

	if (!ret) {
		_mmcam_dbg_err("Could not initialize GStreamer: %s\n",
		               err ? err->message : "unknown error occurred");
		if (err) {
			g_error_free (err);
		}
	}

	/* release */
	for (i = 0; i < *argc; i++) {
		if (argv[i]) {
			free(argv[i]);
			argv[i] = NULL;
		}
	}

	if (argv) {
		free(argv);
		argv = NULL;
	}

	if (argc) {
		free(argc);
		argc = NULL;
	}

	return ret;

ERROR:
	_mmcam_dbg_err("failed to initialize gstreamer");

	if (argv) {
		free(argv);
		argv = NULL;
	}

	if (argc) {
		free(argc);
		argc = NULL;
	}

	return FALSE;
}


int _mmcamcorder_get_state(MMHandleType handle)
{
	int state;
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);

	mmf_return_val_if_fail(hcamcorder, -1);

	_MMCAMCORDER_LOCK_STATE(handle);

	state = hcamcorder->state;
	_mmcam_dbg_log("state=%d",state);

	_MMCAMCORDER_UNLOCK_STATE(handle);

	return state;
}


void _mmcamcorder_set_state(MMHandleType handle, int state)
{
	int old_state;
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderMsgItem msg;

	mmf_return_if_fail(hcamcorder);

	_mmcam_dbg_log("");

	_MMCAMCORDER_LOCK_STATE(handle);

	old_state = hcamcorder->state;
	if(old_state != state) {
		hcamcorder->state = state;
		hcamcorder->target_state = state;

		_mmcam_dbg_log("set state[%d] and send state-changed message", state);

		/* To discern who changes the state */
		switch (hcamcorder->state_change_by_system) {
		case _MMCAMCORDER_STATE_CHANGE_BY_ASM:
			msg.id = MM_MESSAGE_CAMCORDER_STATE_CHANGED_BY_ASM;
			msg.param.state.code = hcamcorder->asm_event_code;
			break;
		case _MMCAMCORDER_STATE_CHANGE_NORMAL:
		default:
			msg.id = MM_MESSAGE_CAMCORDER_STATE_CHANGED;
			msg.param.state.code = MM_ERROR_NONE;
			break;
		}

		msg.param.state.previous = old_state;
		msg.param.state.current = state;

		_mmcam_dbg_log("_mmcamcroder_send_message : msg : %p, id:%x", &msg, msg.id);
		_mmcamcroder_send_message(handle, &msg);
	}

	_MMCAMCORDER_UNLOCK_STATE(handle);

	return;
}


int _mmcamcorder_get_async_state(MMHandleType handle)
{
	int state;
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);

	_MMCAMCORDER_LOCK_STATE(handle);
	state = hcamcorder->target_state;

	_MMCAMCORDER_UNLOCK_STATE(handle);

	return state;
}

int _mmcamcorder_set_async_state(MMHandleType handle, int target_state)
{
	int error = MM_ERROR_NONE;
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_INVALID_CONDITION);
		
	_mmcam_dbg_log("");

	if (hcamcorder->sync_state_change) {
		return error;
	}

	_MMCAMCORDER_LOCK_STATE(handle);

	/* check changing state */
	if (hcamcorder->state != hcamcorder->target_state) {
		_mmcam_dbg_warn("State is changing now.");
		error = MM_ERROR_CAMCORDER_CMD_IS_RUNNING;
		goto _SET_ASYNC_END;
	}

	/* check already set */
	if (hcamcorder->state == target_state) {
		_mmcam_dbg_log("Target state already set.");
		error = MM_ERROR_CAMCORDER_INVALID_STATE;
		goto _SET_ASYNC_END;
	}

	hcamcorder->target_state = target_state;

	_mmcam_dbg_log("Done");

_SET_ASYNC_END:
	_MMCAMCORDER_UNLOCK_STATE(handle);
	return error;
}


gboolean _mmcamcorder_set_async_cancel(MMHandleType handle)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	
	mmf_return_val_if_fail(hcamcorder, FALSE);
	
	_mmcam_dbg_log("");

	if (hcamcorder->sync_state_change) {
		return TRUE;
	}

	_MMCAMCORDER_LOCK_STATE(handle);

	if (hcamcorder->target_state != hcamcorder->state) {
		hcamcorder->target_state = hcamcorder->state;
		_mmcam_dbg_log("Async state change is canceled.");
		_MMCAMCORDER_UNLOCK_STATE(handle);
		return TRUE;
	} else {
		_mmcam_dbg_log("Nothing to be cancel or Already processed.");
	}

	_MMCAMCORDER_UNLOCK_STATE(handle);

	return FALSE;
}


gboolean _mmcamcorder_is_state_changing(MMHandleType handle)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);

	mmf_return_val_if_fail(hcamcorder, FALSE);
	
	_mmcam_dbg_log("(%d)", (hcamcorder->target_state != hcamcorder->state));

	if (hcamcorder->sync_state_change) {
		return FALSE;
	}

	_MMCAMCORDER_LOCK_STATE(handle);

	if (hcamcorder->target_state != hcamcorder->state) {
		_MMCAMCORDER_UNLOCK_STATE(handle);
		return TRUE;
	}

	_MMCAMCORDER_UNLOCK_STATE(handle);

	return FALSE;
}


_MMCamcorderSubContext *_mmcamcorder_alloc_subcontext(int type)
{
	int i;
	_MMCamcorderSubContext *sc = NULL;

	_mmcam_dbg_log("");

	/* alloc container */
	sc = (_MMCamcorderSubContext *)malloc(sizeof(_MMCamcorderSubContext));
	mmf_return_val_if_fail(sc != NULL, NULL);

	/* init members */
	memset(sc, 0x00, sizeof(_MMCamcorderSubContext));

	sc->element_num = _MMCamcorder_PIPELINE_ELEMENT_NUM;

	/* alloc info for each mode */
	switch (type) {
	case MM_CAMCORDER_MODE_IMAGE:
		sc->info = malloc( sizeof(_MMCamcorderImageInfo));
		if(sc->info == NULL) {
			_mmcam_dbg_err("Failed to alloc info structure");
			free(sc);
			return NULL;
		}
		memset(sc->info, 0x00, sizeof(_MMCamcorderImageInfo));
		break;
	case MM_CAMCORDER_MODE_AUDIO:
		sc->info = malloc( sizeof(_MMCamcorderAudioInfo));
		if(sc->info == NULL) {
			_mmcam_dbg_err("Failed to alloc info structure");
			free(sc);
			return NULL;
		}
		memset(sc->info, 0x00, sizeof(_MMCamcorderAudioInfo));
		break;
	case MM_CAMCORDER_MODE_VIDEO:
		sc->info = malloc( sizeof(_MMCamcorderVideoInfo));
		if(sc->info == NULL) {
			_mmcam_dbg_err("Failed to alloc info structure");
			free(sc);
			return NULL;
		}
		memset(sc->info, 0x00, sizeof(_MMCamcorderVideoInfo));
		break;
	default:
		_mmcam_dbg_err("unknown type[%d]", type);
		free(sc);
		return NULL;
	}

	/* alloc element array */
	sc->element = (_MMCamcorderGstElement *)malloc(sizeof(_MMCamcorderGstElement) * sc->element_num);
	if(!sc->element) {
		_mmcam_dbg_err("Failed to alloc element structure");
		free(sc->info);
		free(sc);
		return NULL;
	}

	for (i = 0; i< sc->element_num; i++) {
		sc->element[i].id = _MMCAMCORDER_NONE;
		sc->element[i].gst = NULL;
	}

	sc->fourcc = 0x80000000;
	sc->cam_stability_count = 0;
	sc->drop_vframe = 0;
	sc->pass_first_vframe = 0;
	sc->is_modified_rate = FALSE;

	return sc;
}


void _mmcamcorder_dealloc_subcontext(_MMCamcorderSubContext *sc)
{
	_mmcam_dbg_log("");

	if (sc) {
		if (sc->element) {
			free(sc->element);
			sc->element = NULL;
		}

		if (sc->info) {
			free(sc->info);
			sc->info = NULL;
		}

		free(sc);
		sc = NULL;
	}

	return;
}


int _mmcamcorder_set_functions(MMHandleType handle, int type)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);

	_mmcam_dbg_log("");

	switch (type) {
	case MM_CAMCORDER_MODE_VIDEO:
		hcamcorder->command = _mmcamcorder_video_command;
		break;
	case MM_CAMCORDER_MODE_AUDIO:
		hcamcorder->command = _mmcamcorder_audio_command;
		break;
	case MM_CAMCORDER_MODE_IMAGE:
		hcamcorder->command = _mmcamcorder_image_command;
		break;
	default:
		return MM_ERROR_CAMCORDER_INTERNAL;
		break;
	}

	return MM_ERROR_NONE;
}


gboolean _mmcamcorder_pipeline_cb_message(GstBus *bus, GstMessage *message, gpointer data)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(data);
	_MMCamcorderMsgItem msg;
	_MMCamcorderSubContext *sc = NULL;

	mmf_return_val_if_fail(hcamcorder, FALSE);
	mmf_return_val_if_fail(message, FALSE);
	//_mmcam_dbg_log("message type=(%d)", GST_MESSAGE_TYPE(message));

	switch (GST_MESSAGE_TYPE(message)) {
	case GST_MESSAGE_UNKNOWN:
		_mmcam_dbg_log("GST_MESSAGE_UNKNOWN");
		break;
	case GST_MESSAGE_EOS:
	{
		_mmcam_dbg_log ("Got EOS from element \"%s\".",
		                GST_STR_NULL(GST_ELEMENT_NAME(GST_MESSAGE_SRC(message))));

		sc = MMF_CAMCORDER_SUBCONTEXT(hcamcorder);
		mmf_return_val_if_fail(sc, TRUE);
		mmf_return_val_if_fail(sc->info, TRUE);

		if (hcamcorder->type == MM_CAMCORDER_MODE_VIDEO) {
			_MMCamcorderVideoInfo *info = sc->info;
			if (info->b_commiting) {
				_mmcamcorder_video_handle_eos((MMHandleType)hcamcorder);
			}
		} else if (hcamcorder->type == MM_CAMCORDER_MODE_AUDIO) {
			_MMCamcorderAudioInfo *info = sc->info;
			if (info->b_commiting) {
				_mmcamcorder_audio_handle_eos((MMHandleType)hcamcorder);
			}
		}

		sc->bget_eos = TRUE;

		break;
	}
	case GST_MESSAGE_ERROR:
	{
		GError *err;
		gchar *debug;
		gst_message_parse_error(message, &err, &debug);

		_mmcam_dbg_err ("GSTERR: %s", err->message);
		_mmcam_dbg_err ("Error Debug: %s", debug);

		__mmcamcorder_handle_gst_error((MMHandleType)hcamcorder, message, err);

		g_error_free (err);
		g_free (debug);
		break;
	}
	case GST_MESSAGE_WARNING:
	{
		GError *err;
		gchar *debug;
		gst_message_parse_warning (message, &err, &debug);

		_mmcam_dbg_warn("GSTWARN: %s", err->message);

		__mmcamcorder_handle_gst_warning((MMHandleType)hcamcorder, message, err);

		g_error_free (err);
		g_free (debug);
		break;
	}
	case GST_MESSAGE_INFO:
		_mmcam_dbg_log("GST_MESSAGE_INFO");
		break;
	case GST_MESSAGE_TAG:
		_mmcam_dbg_log("GST_MESSAGE_TAG");
		break;
	case GST_MESSAGE_BUFFERING:
		_mmcam_dbg_log("GST_MESSAGE_BUFFERING");
		break;
	case GST_MESSAGE_STATE_CHANGED:
	{
		GValue *vnewstate;
		GstState newstate;
		GstElement *pipeline = NULL;

		sc = MMF_CAMCORDER_SUBCONTEXT(hcamcorder);
		if ((sc) && (sc->element)) {
			if (sc->element[_MMCAMCORDER_MAIN_PIPE].gst) {
				pipeline = sc->element[_MMCAMCORDER_MAIN_PIPE].gst;
				if (message->src == (GstObject*)pipeline) {
					vnewstate = (GValue*)gst_structure_get_value(message->structure, "new-state");
					newstate = (GstState)vnewstate->data[0].v_int;
					_mmcam_dbg_log("GST_MESSAGE_STATE_CHANGED[%s]",gst_element_state_get_name(newstate));
				}
			}
		}
		break;
	}
	case GST_MESSAGE_STATE_DIRTY:
		_mmcam_dbg_log("GST_MESSAGE_STATE_DIRTY");
		break;
	case GST_MESSAGE_STEP_DONE:
		_mmcam_dbg_log("GST_MESSAGE_STEP_DONE");
		break;
	case GST_MESSAGE_CLOCK_PROVIDE:
		_mmcam_dbg_log("GST_MESSAGE_CLOCK_PROVIDE");
		break;
	case GST_MESSAGE_CLOCK_LOST:
		_mmcam_dbg_log("GST_MESSAGE_CLOCK_LOST");
		break;
	case GST_MESSAGE_NEW_CLOCK:
	{
		GstClock *clock;
		gst_message_parse_new_clock(message, &clock);
		_mmcam_dbg_log("GST_MESSAGE_NEW_CLOCK : %s", (clock ? GST_OBJECT_NAME (clock) : "NULL"));
		break;
	}
	case GST_MESSAGE_STRUCTURE_CHANGE:
		_mmcam_dbg_log("GST_MESSAGE_STRUCTURE_CHANGE");
		break;
	case GST_MESSAGE_STREAM_STATUS:
		_mmcam_dbg_log("GST_MESSAGE_STREAM_STATUS");
		break;
	case GST_MESSAGE_APPLICATION:
		_mmcam_dbg_log("GST_MESSAGE_APPLICATION");
		break;
	case GST_MESSAGE_ELEMENT:
		_mmcam_dbg_log("GST_MESSAGE_ELEMENT");

		if (gst_structure_has_name(message->structure, "avsysvideosrc-AF") ||
		    gst_structure_has_name(message->structure, "camerasrc-AF")) {
			int focus_state = 0;

			gst_structure_get_int(message->structure, "focus-state", &focus_state);
			_mmcam_dbg_log("Focus State:%d", focus_state);

			msg.id = MM_MESSAGE_CAMCORDER_FOCUS_CHANGED;
			msg.param.code = focus_state;
			_mmcamcroder_send_message((MMHandleType)hcamcorder, &msg);
		} else if (gst_structure_has_name(message->structure, "camerasrc-HDR")) {
			int progress = 0;
			int status = 0;

			gst_structure_get_int(message->structure, "progress", &progress);
			gst_structure_get_int(message->structure, "status", &status);
			_mmcam_dbg_log("HDR progress %d percent, status %d", progress, status);

			msg.id = MM_MESSAGE_CAMCORDER_HDR_PROGRESS;
			msg.param.code = progress;
			_mmcamcroder_send_message((MMHandleType)hcamcorder, &msg);
		} else if (gst_structure_has_name(message->structure, "camerasrc-FD")) {
			int i = 0;
			GValue *g_value = NULL;
			GstCameraControlFaceDetectInfo *fd_info = NULL;
			MMCamFaceDetectInfo *cam_fd_info = NULL;

			g_value = gst_structure_get_value(message->structure, "face-info");
			if (g_value) {
				fd_info = (GstCameraControlFaceDetectInfo *)g_value_get_pointer(g_value);
			}

			if (fd_info == NULL) {
				_mmcam_dbg_warn("fd_info is NULL");
				return TRUE;
			}

			cam_fd_info = (MMCamFaceDetectInfo *)malloc(sizeof(MMCamFaceDetectInfo));
			if (cam_fd_info == NULL) {
				_mmcam_dbg_warn("cam_fd_info alloc failed");

				free(fd_info);
				fd_info = NULL;

				return TRUE;
			}

			/* set total face count */
			cam_fd_info->num_of_faces = fd_info->num_of_faces;

			if (cam_fd_info->num_of_faces > 0) {
				cam_fd_info->face_info = (MMCamFaceInfo *)malloc(sizeof(MMCamFaceInfo) * cam_fd_info->num_of_faces);
				if (cam_fd_info->face_info) {
					/* set information of each face */
					for (i = 0 ; i < fd_info->num_of_faces ; i++) {
						cam_fd_info->face_info[i].id = fd_info->face_info[i].id;
						cam_fd_info->face_info[i].score = fd_info->face_info[i].score;
						cam_fd_info->face_info[i].rect.x = fd_info->face_info[i].rect.x;
						cam_fd_info->face_info[i].rect.y = fd_info->face_info[i].rect.y;
						cam_fd_info->face_info[i].rect.width = fd_info->face_info[i].rect.width;
						cam_fd_info->face_info[i].rect.height = fd_info->face_info[i].rect.height;
						/*
						_mmcam_dbg_log("id %d, score %d, [%d,%d,%dx%d]",
						               fd_info->face_info[i].id,
						               fd_info->face_info[i].score,
						               fd_info->face_info[i].rect.x,
						               fd_info->face_info[i].rect.y,
						               fd_info->face_info[i].rect.width,
						               fd_info->face_info[i].rect.height);
						*/
					}
				} else {
					_mmcam_dbg_warn("MMCamFaceInfo alloc failed");

					/* free allocated memory that is not sent */
					free(cam_fd_info);
					cam_fd_info = NULL;
				}
			} else {
				cam_fd_info->face_info = NULL;
			}

			if (cam_fd_info) {
				/* send message  - cam_fd_info should be freed by application */
				msg.id = MM_MESSAGE_CAMCORDER_FACE_DETECT_INFO;
				msg.param.data = cam_fd_info;
				msg.param.size = sizeof(MMCamFaceDetectInfo);
				msg.param.code = 0;

				_mmcamcroder_send_message((MMHandleType)hcamcorder, &msg);
			}

			/* free fd_info allocated by plugin */
			free(fd_info);
			fd_info = NULL;
		}
		break;
	case GST_MESSAGE_SEGMENT_START:
		_mmcam_dbg_log("GST_MESSAGE_SEGMENT_START");
		break;
	case GST_MESSAGE_SEGMENT_DONE:
		_mmcam_dbg_log("GST_MESSAGE_SEGMENT_DONE");
		break;
	case GST_MESSAGE_DURATION:
		_mmcam_dbg_log("GST_MESSAGE_DURATION");
		break;
	case GST_MESSAGE_LATENCY:
		_mmcam_dbg_log("GST_MESSAGE_LATENCY");
		break;
	case GST_MESSAGE_ASYNC_START:
		_mmcam_dbg_log("GST_MESSAGE_ASYNC_START");
		break;
	case GST_MESSAGE_ASYNC_DONE:
		_mmcam_dbg_log("GST_MESSAGE_ASYNC_DONE");
		break;
	case GST_MESSAGE_ANY:
		_mmcam_dbg_log("GST_MESSAGE_ANY");
		break;
	default:
		_mmcam_dbg_log("not handled message type=(%d)", GST_MESSAGE_TYPE(message));
		break;
	}

	return TRUE;
}


GstBusSyncReply _mmcamcorder_pipeline_bus_sync_callback(GstBus *bus, GstMessage *message, gpointer data)
{
	GstElement *element = NULL;
	GError *err = NULL;
	gchar *debug_info = NULL;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(data);
	_MMCamcorderSubContext *sc = NULL;

	mmf_return_val_if_fail(hcamcorder, GST_BUS_PASS);
	mmf_return_val_if_fail(message, GST_BUS_PASS);

	sc = MMF_CAMCORDER_SUBCONTEXT(hcamcorder);
	mmf_return_val_if_fail(sc, GST_BUS_PASS);

	sc->error_code = MM_ERROR_NONE;

	if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR) {
		/* parse error message */
		gst_message_parse_error(message, &err, &debug_info);

		if (!err) {
			_mmcam_dbg_warn("failed to parse error message");
			return GST_BUS_PASS;
		}

		if (debug_info) {
			_mmcam_dbg_err("GST ERROR : %s", debug_info);
			g_free(debug_info);
			debug_info = NULL;
		}

		/* set videosrc element to compare */
		element = GST_ELEMENT_CAST(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst);

		/* check domain[RESOURCE] and element[VIDEOSRC] */
		if (err->domain == GST_RESOURCE_ERROR &&
		    GST_ELEMENT_CAST(message->src) == element) {
			switch (err->code) {
			case GST_RESOURCE_ERROR_BUSY:
				_mmcam_dbg_err("Camera device [busy]");
				sc->error_code = MM_ERROR_CAMCORDER_DEVICE_BUSY;
				break;
			case GST_RESOURCE_ERROR_OPEN_READ_WRITE:
			case GST_RESOURCE_ERROR_OPEN_WRITE:
				_mmcam_dbg_err("Camera device [open failed]");
				sc->error_code = MM_ERROR_CAMCORDER_DEVICE_OPEN;
				break;
			case GST_RESOURCE_ERROR_OPEN_READ:
				_mmcam_dbg_err("Camera device [register trouble]");
				sc->error_code = MM_ERROR_CAMCORDER_DEVICE_REG_TROUBLE;
				break;
			case GST_RESOURCE_ERROR_NOT_FOUND:
				_mmcam_dbg_err("Camera device [device not found]");
				sc->error_code = MM_ERROR_CAMCORDER_DEVICE_NOT_FOUND;
				break;
			case GST_RESOURCE_ERROR_TOO_LAZY:
				_mmcam_dbg_err("Camera device [timeout]");
				sc->error_code = MM_ERROR_CAMCORDER_DEVICE_TIMEOUT;
				break;
			case GST_RESOURCE_ERROR_SETTINGS:
				_mmcam_dbg_err("Camera device [not supported]");
				sc->error_code = MM_ERROR_CAMCORDER_NOT_SUPPORTED;
				break;
			case GST_RESOURCE_ERROR_FAILED:
				_mmcam_dbg_err("Camera device [working failed].");
				sc->error_code = MM_ERROR_CAMCORDER_DEVICE_IO;
				break;
			default:
				_mmcam_dbg_err("Camera device [General(%d)]", err->code);
				sc->error_code = MM_ERROR_CAMCORDER_DEVICE;
				break;
			}

			sc->error_occurs = TRUE;
		}

		g_error_free(err);

		/* store error code and drop this message if cmd is running */
		if (sc->error_code != MM_ERROR_NONE) {
			if (!_MMCAMCORDER_TRYLOCK_CMD(hcamcorder)) {
				_mmcam_dbg_err("cmd is running and will be returned with this error %x",
				               sc->error_code);
				return GST_BUS_DROP;
			}

			_MMCAMCORDER_UNLOCK_CMD(hcamcorder);
		}
	}

	return GST_BUS_PASS;
}


static int __mmcamcorder_asm_get_event_type(int sessionType)
{
	switch (sessionType) {
	case MM_SESSION_TYPE_SHARE:
		return ASM_EVENT_SHARE_MMCAMCORDER;
	case MM_SESSION_TYPE_EXCLUSIVE:
		return ASM_EVENT_EXCLUSIVE_MMCAMCORDER;
	case MM_SESSION_TYPE_NOTIFY:
		return ASM_EVENT_NOTIFY;
	case MM_SESSION_TYPE_CALL:
		return ASM_EVENT_CALL;
	case MM_SESSION_TYPE_VIDEOCALL:
		return ASM_EVENT_VIDEOCALL;
	case MM_SESSION_TYPE_ALARM:
		return ASM_EVENT_ALARM;
	default:
		return ASM_EVENT_SHARE_MMCAMCORDER;
	}
}

ASM_cb_result_t _mmcamcorder_asm_callback(int handle, ASM_event_sources_t event_src,
                                          ASM_sound_commands_t command,
                                          unsigned int sound_status, void* cb_data)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(cb_data);
	int current_state = MM_CAMCORDER_STATE_NONE;
	ASM_cb_result_t cb_res = ASM_CB_RES_NONE;

	mmf_return_val_if_fail((MMHandleType)hcamcorder, ASM_CB_RES_NONE);

	current_state = _mmcamcorder_get_state((MMHandleType)hcamcorder);
	if (current_state <= MM_CAMCORDER_STATE_NONE ||
	    current_state >= MM_CAMCORDER_STATE_NUM) {
		_mmcam_dbg_err("Abnormal state. Or null handle. (%p, %d, %d)", hcamcorder, command, current_state);
	}

	/* set value to inform a status is changed by asm */
	hcamcorder->state_change_by_system = _MMCAMCORDER_STATE_CHANGE_BY_ASM;
	/* set ASM event code for sending it to application */
	hcamcorder->asm_event_code = event_src;

	switch (command) {
	case ASM_COMMAND_STOP:
	case ASM_COMMAND_PAUSE:
		_mmcam_dbg_log("Got msg from asm to Stop or Pause(%d, %d)", command, current_state);

		__mmcamcorder_force_stop(hcamcorder);
		cb_res = ASM_CB_RES_STOP;

		_mmcam_dbg_log("Finish opeartion. Camera is released.(%d)", cb_res);
		break;
	case ASM_COMMAND_PLAY:
		_mmcam_dbg_log("Got msg from asm to Play(%d, %d)", command, current_state);

		if (current_state >= MM_CAMCORDER_STATE_PREPARE) {
			_mmcam_dbg_log("Already start previewing");
			return ASM_CB_RES_PLAYING;
		}

		__mmcamcorder_force_resume(hcamcorder);
		cb_res = ASM_CB_RES_PLAYING;

		_mmcam_dbg_log("Finish opeartion. Preview is started.(%d)", cb_res);
		break;
	case ASM_COMMAND_RESUME:
	{
		_MMCamcorderMsgItem msg;

		_mmcam_dbg_log("Got msg from asm to Resume(%d, %d)", command, current_state);

		msg.id = MM_MESSAGE_READY_TO_RESUME;
		_mmcamcroder_send_message((MMHandleType)hcamcorder, &msg);
		cb_res = ASM_CB_RES_PLAYING;

		_mmcam_dbg_log("Finish opeartion.(%d)", cb_res);
		break;
	}
	default: /* should not be reached here */
		cb_res = ASM_CB_RES_PLAYING;
		_mmcam_dbg_err("Command err.");
		break;
	}

	/* restore value */
	hcamcorder->state_change_by_system = _MMCAMCORDER_STATE_CHANGE_NORMAL;

	return cb_res;
}


int _mmcamcorder_create_pipeline(MMHandleType handle, int type)
{
	int ret = MM_ERROR_NONE;
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;
	GstElement *pipeline = NULL;

	_mmcam_dbg_log("handle : %x, type : %d", handle, type);

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	mmf_return_val_if_fail(sc, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	switch (type) {
	case MM_CAMCORDER_MODE_IMAGE:
		__ta__("        _mmcamcorder_create_preview_pipeline",
		ret = _mmcamcorder_create_preview_pipeline(handle);
		);
		if (ret != MM_ERROR_NONE) {
			return ret;
		}

		__ta__("        _mmcamcorder_add_stillshot_pipeline",
		ret = _mmcamcorder_add_stillshot_pipeline(handle);
		);
		if (ret != MM_ERROR_NONE) {
			return ret;
		}
		break;
	case MM_CAMCORDER_MODE_AUDIO:
		__ta__("        _mmcamcorder_create_audio_pipeline",
		ret = _mmcamcorder_create_audio_pipeline(handle);
		);
		if (ret != MM_ERROR_NONE) {
			return ret;
		}
		break;
	case MM_CAMCORDER_MODE_VIDEO:
		__ta__("        _mmcamcorder_create_preview_pipeline",
		ret = _mmcamcorder_create_preview_pipeline(handle);
		);
		if (ret != MM_ERROR_NONE) {
			return ret;
		}
		break;
	default:
		return MM_ERROR_CAMCORDER_INTERNAL;
	}

	pipeline = sc->element[_MMCAMCORDER_MAIN_PIPE].gst;
	if (type != MM_CAMCORDER_MODE_AUDIO) {
		if (hcamcorder->sync_state_change) {
			ret = _mmcamcorder_gst_set_state(handle, pipeline, GST_STATE_READY);
		} else {
			ret = _mmcamcorder_gst_set_state_async(handle, pipeline, GST_STATE_READY);
			if (ret == GST_STATE_CHANGE_FAILURE) {
				return MM_ERROR_CAMCORDER_GST_STATECHANGE;
			}
		}
	}
#ifdef _MMCAMCORDER_GET_DEVICE_INFO
	if (!_mmcamcorder_get_device_info(handle)) {
		_mmcam_dbg_err("Getting device information error!!");
	}
#endif
	_mmcam_dbg_log("ret[%x]", ret);
	return ret;
}


void _mmcamcorder_destroy_pipeline(MMHandleType handle, int type)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;	
	gint i = 0;
	GstBus *bus = NULL;

	mmf_return_if_fail(hcamcorder);

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	mmf_return_if_fail(sc);

	_mmcam_dbg_log("");

	bus = gst_pipeline_get_bus(GST_PIPELINE(sc->element[_MMCAMCORDER_MAIN_PIPE].gst));

	/* Inside each pipeline destroy function, Set GST_STATE_NULL to Main pipeline */
	switch (type) {
	case MM_CAMCORDER_MODE_IMAGE:
		_mmcamcorder_destroy_image_pipeline(handle);
		break;
	case MM_CAMCORDER_MODE_AUDIO:
		_mmcamcorder_destroy_audio_pipeline(handle);
		break;
	case MM_CAMCORDER_MODE_VIDEO:
		_mmcamcorder_destroy_video_pipeline(handle);
		break;
	default:
		break;
	}

	_mmcam_dbg_log("Pipeline clear!!");

	/* Remove pipeline message callback */
	g_source_remove(hcamcorder->pipeline_cb_event_id);

	/* Remove remained message in bus */
	if (bus != NULL) {
		GstMessage *gst_msg = NULL;
		while ((gst_msg = gst_bus_pop(bus)) != NULL) {
			_mmcamcorder_pipeline_cb_message(bus, gst_msg, (gpointer)hcamcorder);
			gst_message_unref( gst_msg );
			gst_msg = NULL;
		}
		gst_object_unref( bus );
		bus = NULL;
	}

	/* checking unreleased element */
	for (i = 0 ; i < _MMCamcorder_PIPELINE_ELEMENT_NUM ; i++ ) {
		if (sc->element[i].gst) {
			if (GST_IS_ELEMENT(sc->element[i].gst)) {
				_mmcam_dbg_warn("Still alive element - ID[%d], name [%s], ref count[%d], status[%s]",
				                sc->element[i].id,
				                GST_OBJECT_NAME(sc->element[i].gst),
				                GST_OBJECT_REFCOUNT_VALUE(sc->element[i].gst),
				                gst_element_state_get_name (GST_STATE (sc->element[i].gst)));
				g_object_weak_unref(G_OBJECT(sc->element[i].gst), (GWeakNotify)_mmcamcorder_element_release_noti, sc);
			} else {
				_mmcam_dbg_warn("The element[%d] is still aliving, check it", sc->element[i].id);
			}

			sc->element[i].id = _MMCAMCORDER_NONE;
			sc->element[i].gst = NULL;
		}
	}
}


int _mmcamcorder_gst_set_state_async(MMHandleType handle, GstElement *pipeline, GstState target_state)
{
	GstStateChangeReturn setChangeReturn = GST_STATE_CHANGE_FAILURE;

	_MMCAMCORDER_LOCK_GST_STATE(handle);
	setChangeReturn = gst_element_set_state(pipeline, target_state);
	_MMCAMCORDER_UNLOCK_GST_STATE(handle);

	return setChangeReturn;
}


static gboolean __mmcamcorder_set_attr_to_camsensor_cb(gpointer data)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(data);

	mmf_return_val_if_fail(hcamcorder, FALSE);

	_mmcam_dbg_log("");

	_mmcamcorder_set_attribute_to_camsensor((MMHandleType)hcamcorder);

	/* initialize */
	hcamcorder->setting_event_id = 0;

	_mmcam_dbg_log("Done");

	/* once */
	return FALSE;
}


int _mmcamcorder_gst_set_state (MMHandleType handle, GstElement *pipeline, GstState target_state)
{
	unsigned int k = 0;
	GstState pipeline_state = GST_STATE_VOID_PENDING;
	GstStateChangeReturn setChangeReturn = GST_STATE_CHANGE_FAILURE;
	GstStateChangeReturn getChangeReturn = GST_STATE_CHANGE_FAILURE;
	GstClockTime get_timeout = __MMCAMCORDER_SET_GST_STATE_TIMEOUT * GST_SECOND;

	mmf_return_val_if_fail(handle, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	_mmcam_dbg_log("Set state to %d", target_state);

	_MMCAMCORDER_LOCK_GST_STATE(handle);

	for (k = 0; k < _MMCAMCORDER_STATE_SET_COUNT; k++) {
		setChangeReturn = gst_element_set_state(pipeline, target_state);
		_mmcam_dbg_log("gst_element_set_state[%d] return %d",
		               target_state, setChangeReturn);
		if (setChangeReturn != GST_STATE_CHANGE_FAILURE) {
			getChangeReturn = gst_element_get_state(pipeline, &pipeline_state, NULL, get_timeout);
			switch (getChangeReturn) {
			case GST_STATE_CHANGE_NO_PREROLL:
				_mmcam_dbg_log("status=GST_STATE_CHANGE_NO_PREROLL.");
			case GST_STATE_CHANGE_SUCCESS:
				/* if we reached the final target state, exit */
				if (pipeline_state == target_state) {
					_MMCAMCORDER_UNLOCK_GST_STATE(handle);
					return MM_ERROR_NONE;
				}
				break;
			case GST_STATE_CHANGE_ASYNC:
				_mmcam_dbg_log("status=GST_STATE_CHANGE_ASYNC.");
				break;
			default :
				_MMCAMCORDER_UNLOCK_GST_STATE(handle);
				_mmcam_dbg_log("status=GST_STATE_CHANGE_FAILURE.");
				return MM_ERROR_CAMCORDER_GST_STATECHANGE;
			}

			_MMCAMCORDER_UNLOCK_GST_STATE(handle);
			_mmcam_dbg_err("timeout of gst_element_get_state()!!");
			return MM_ERROR_CAMCORDER_RESPONSE_TIMEOUT;
		}
		usleep(_MMCAMCORDER_STATE_CHECK_INTERVAL);
	}

	_MMCAMCORDER_UNLOCK_GST_STATE(handle);

	_mmcam_dbg_err("Failure. gst_element_set_state timeout!!");

	return MM_ERROR_CAMCORDER_RESPONSE_TIMEOUT;	
}


/* For performance check */
int _mmcamcorder_video_current_framerate(MMHandleType handle)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;

	mmf_return_val_if_fail(hcamcorder, -1);

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	mmf_return_val_if_fail(sc, -1);
	
	return sc->kpi.current_fps;
}


int _mmcamcorder_video_average_framerate(MMHandleType handle)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;

	mmf_return_val_if_fail(hcamcorder, -1);

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	mmf_return_val_if_fail(sc, -1);

	return sc->kpi.average_fps;
}


void _mmcamcorder_video_current_framerate_init(MMHandleType handle)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;

	mmf_return_if_fail(hcamcorder);

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	mmf_return_if_fail(sc);

	memset(&(sc->kpi), 0x00, sizeof(_MMCamcorderKPIMeasure));

	return;
}

/* Async state change */
void _mmcamcorder_delete_command_info(__MMCamcorderCmdInfo *cmdinfo)
{
	if (cmdinfo) {
		g_free(cmdinfo);
	}
}

static void __mmcamcorder_force_stop(mmf_camcorder_t *hcamcorder)
{
	int i = 0;
	int loop = 0;
	int itr_cnt = 0;
	int result = MM_ERROR_NONE;
	int current_state = MM_CAMCORDER_STATE_NONE;

	mmf_return_if_fail(hcamcorder);

	current_state = _mmcamcorder_get_state((MMHandleType)hcamcorder);

	_mmcam_dbg_log( "Force STOP MMFW Camcorder" );

	for (loop = 0 ; current_state > MM_CAMCORDER_STATE_NULL && loop < __MMCAMCORDER_CMD_ITERATE_MAX * 3 ; loop++) {
		itr_cnt = __MMCAMCORDER_CMD_ITERATE_MAX;
		switch (current_state) {
		case MM_CAMCORDER_STATE_CAPTURING:
		{
			_MMCamcorderSubContext *sc = MMF_CAMCORDER_SUBCONTEXT(hcamcorder);
			_MMCamcorderImageInfo *info = NULL;

			mmf_return_if_fail(sc);
			mmf_return_if_fail((info = sc->info));

			_mmcam_dbg_log("Stop capturing.");

			/* if caturing isn't finished, waiting for 2 sec to finish real capture operation. check 'info->capturing'. */
			mm_camcorder_set_attributes((MMHandleType)hcamcorder, NULL,
			                            MMCAM_CAPTURE_BREAK_CONTINUOUS_SHOT, TRUE,
			                            NULL);

			for (i = 0; i < 20 && info->capturing; i++) {
				usleep(100000);
			}

			if (i == 20) {
				_mmcam_dbg_err("Timeout. Can't check stop capturing.");
			}

			while ((itr_cnt--) && ((result = _mmcamcorder_capture_stop((MMHandleType)hcamcorder)) != MM_ERROR_NONE)) {
					_mmcam_dbg_warn("Can't stop capturing.(%x)", result);
			}

			break;
		}
		case MM_CAMCORDER_STATE_RECORDING:
		case MM_CAMCORDER_STATE_PAUSED:
		{
			_mmcam_dbg_log("Stop recording.");

			while ((itr_cnt--) && ((result = _mmcamcorder_commit((MMHandleType)hcamcorder)) != MM_ERROR_NONE)) {
				_mmcam_dbg_warn("Can't commit.(%x)", result);
			}
			break;
		}
		case MM_CAMCORDER_STATE_PREPARE:
		{
			_mmcam_dbg_log("Stop preview.");

			while ((itr_cnt--) && ((result = _mmcamcorder_stop((MMHandleType)hcamcorder)) != MM_ERROR_NONE)) {
				_mmcam_dbg_warn("Can't stop preview.(%x)", result);
			}
			break;
		}
		case MM_CAMCORDER_STATE_READY:
		{
			_mmcam_dbg_log("unrealize");

			if ((result = _mmcamcorder_unrealize((MMHandleType)hcamcorder)) != MM_ERROR_NONE) {
				_mmcam_dbg_warn("Can't unrealize.(%x)", result);
			}
			break;
		}
		case MM_CAMCORDER_STATE_NULL:
		default:
			_mmcam_dbg_log("Already stopped.");
			break;
		}

		current_state = _mmcamcorder_get_state((MMHandleType)hcamcorder);
	}

	_mmcam_dbg_log( "Done." );

	return;
}


static void __mmcamcorder_force_resume(mmf_camcorder_t *hcamcorder)
{
	int loop = 0;
	int result = MM_ERROR_NONE;
	int itr_cnt = 0;
	int current_state = MM_CAMCORDER_STATE_NONE;

	mmf_return_if_fail(hcamcorder);

	_mmcam_dbg_log("Force RESUME MMFW Camcorder");

	current_state = _mmcamcorder_get_state((MMHandleType)hcamcorder);

	for (loop = 0 ; current_state < MM_CAMCORDER_STATE_PREPARE && loop < __MMCAMCORDER_CMD_ITERATE_MAX * 3 ; loop++) {
		itr_cnt = __MMCAMCORDER_CMD_ITERATE_MAX;

		switch (current_state) {
		case MM_CAMCORDER_STATE_NULL:
			_mmcam_dbg_log("Realize");
			while ((itr_cnt--) && ((result = _mmcamcorder_realize((MMHandleType)hcamcorder)) != MM_ERROR_NONE)) {
				_mmcam_dbg_warn("Can't realize.(%x)", result);
			}
			break;
		case MM_CAMCORDER_STATE_READY:
			_mmcam_dbg_log("Start previewing");

			while ((itr_cnt--) && ((result = _mmcamcorder_start((MMHandleType)hcamcorder)) != MM_ERROR_NONE)) {
				_mmcam_dbg_warn("Can't start previewing.(%x)", result);
			}
			break;
		default:
			_mmcam_dbg_log("Abnormal state.");
			break;
		}

		current_state = _mmcamcorder_get_state((MMHandleType)hcamcorder);
	}

	_mmcam_dbg_log( "Done." );

	return;
}

int _mmcamcorder_create_command_loop(MMHandleType handle)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderCommand *cmd;

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	_mmcam_dbg_log("");

	cmd = (_MMCamcorderCommand *)&(hcamcorder->cmd);
	cmd->cmd_queue = g_queue_new();
	mmf_return_val_if_fail(cmd->cmd_queue, MM_ERROR_CAMCORDER_INVALID_CONDITION);

	sem_init(&cmd->sema, 0, 0);

	if (pthread_create(&cmd->pCommandThread, NULL, _mmcamcorder_command_loop_thread, hcamcorder)) {
		perror("Make Command Thread Fail");
		return MM_ERROR_COMMON_UNKNOWN;
	}

	return MM_ERROR_NONE;
}


int _mmcamcorder_destroy_command_loop(MMHandleType handle)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderCommand *cmd;

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);
	_mmcam_dbg_log("");

	cmd = (_MMCamcorderCommand *)&(hcamcorder->cmd);
	mmf_return_val_if_fail(cmd->cmd_queue, MM_ERROR_CAMCORDER_INVALID_CONDITION);

	_mmcamcorder_append_simple_command(handle, _MMCAMCORDER_CMD_QUIT);

	sem_post(&cmd->sema); /* why is this needed? */

	_mmcam_dbg_log("wait for pthread join");

	pthread_join(cmd->pCommandThread, NULL);

	_mmcam_dbg_log("pthread join!!");
	
	sem_destroy(&cmd->sema);

	while (!g_queue_is_empty(cmd->cmd_queue)) {
		__MMCamcorderCmdInfo *info = NULL;
		info = g_queue_pop_head(cmd->cmd_queue);
		_mmcamcorder_delete_command_info(info);
	}
	g_queue_free(cmd->cmd_queue);

	_mmcam_dbg_log("Command loop clear.");	

	return MM_ERROR_NONE;
}


int _mmcamcorder_append_command(MMHandleType handle, __MMCamcorderCmdInfo *info)
{
	int value = 0;
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderCommand *cmd;

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	_mmcam_dbg_log("");

	cmd = (_MMCamcorderCommand *)&(hcamcorder->cmd);
	mmf_return_val_if_fail(cmd->cmd_queue, MM_ERROR_CAMCORDER_INVALID_CONDITION);

	g_queue_push_tail (cmd->cmd_queue, (gpointer)info);

	sem_getvalue(&cmd->sema, &value);

	if (value == 0) {
		sem_post(&cmd->sema);
	} else {
		/* Don't need to post. */
	}

	return MM_ERROR_NONE;
}


int _mmcamcorder_append_simple_command(MMHandleType handle, _MMCamcorderCommandType type)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	__MMCamcorderCmdInfo *info = NULL;

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	_mmcam_dbg_log("Command Type=%d", type);

	info = (__MMCamcorderCmdInfo*)malloc(sizeof(__MMCamcorderCmdInfo));

	info->handle = handle;
	info->type = type;

	_mmcamcorder_append_command(handle, info);	

	return MM_ERROR_NONE;
}


void *_mmcamcorder_command_loop_thread(void *arg)
{
	gboolean bExit_loop = FALSE;
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(arg);
	_MMCamcorderCommand *cmd = NULL;
	__MMCamcorderCmdInfo *cmdinfo = NULL;

	mmf_return_val_if_fail(hcamcorder, NULL);

	cmd = (_MMCamcorderCommand *)&(hcamcorder->cmd);
	mmf_return_val_if_fail(cmd->cmd_queue, NULL);

	_mmcam_dbg_log("");

	while (!bExit_loop) {
		sem_wait(&cmd->sema);

		/* send command */
		while (!g_queue_is_empty (cmd->cmd_queue)) {
			int bRet = MM_ERROR_NONE;

			cmdinfo = g_queue_pop_head(cmd->cmd_queue);
			if (cmdinfo->handle == (MMHandleType)NULL) {
				_mmcam_dbg_log("Handle in cmdinfo is Null.");
				bRet = MM_ERROR_CAMCORDER_NOT_INITIALIZED;
			} else {
				switch (cmdinfo->type) {
				case _MMCAMCORDER_CMD_CREATE:
				case _MMCAMCORDER_CMD_DESTROY:
				case _MMCAMCORDER_CMD_CAPTURESTART:
				case _MMCAMCORDER_CMD_CAPTURESTOP:
				case _MMCAMCORDER_CMD_RECORD:
				case _MMCAMCORDER_CMD_PAUSE:
				case _MMCAMCORDER_CMD_COMMIT:
					__ta__("_mmcamcorder_commit",
					bRet = _mmcamcorder_commit(cmdinfo->handle);
					);
					break;
				case _MMCAMCORDER_CMD_CANCEL:
					//Not used yet.
					break;
				case _MMCAMCORDER_CMD_REALIZE:
					__ta__("_mmcamcorder_realize",
					bRet = _mmcamcorder_realize(cmdinfo->handle);
					);
					break;
				case _MMCAMCORDER_CMD_UNREALIZE:
					__ta__("_mmcamcorder_unrealize",
					bRet = _mmcamcorder_unrealize(cmdinfo->handle);
					);
					break;
				case _MMCAMCORDER_CMD_START:
					__ta__("_mmcamcorder_start",
					bRet = _mmcamcorder_start(cmdinfo->handle);
					);
					break;
				case _MMCAMCORDER_CMD_STOP:
					__ta__("_mmcamcorder_stop",
					bRet = _mmcamcorder_stop(cmdinfo->handle);
					);
					break;
				case _MMCAMCORDER_CMD_QUIT:
					_mmcam_dbg_log("Exit command loop!!");
					bExit_loop = TRUE;
					break;
				default:
					_mmcam_dbg_log("Wrong command type!!!");
					break;
				}
			}

			if (bRet != MM_ERROR_NONE) {
				_mmcam_dbg_log("Error on command process!(%x)", bRet);
				/* Do something? */
			}

			_mmcamcorder_delete_command_info(cmdinfo);

			if (bExit_loop) {
				break;
			}
			usleep(1);
		}
	}

	return NULL;
}


static gboolean __mmcamcorder_handle_gst_error(MMHandleType handle, GstMessage *message, GError *error)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderMsgItem msg;
	gchar *msg_src_element;
	_MMCamcorderSubContext *sc = NULL;
	
	return_val_if_fail(hcamcorder, FALSE);
	return_val_if_fail(error, FALSE);
	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	mmf_return_val_if_fail(sc, FALSE);

	_mmcam_dbg_log("");

	/* filtering filesink related errors */
	if (hcamcorder->state == MM_CAMCORDER_STATE_RECORDING &&
	    (error->code ==  GST_RESOURCE_ERROR_WRITE || error->code ==  GST_RESOURCE_ERROR_SEEK)) {
		if (sc->ferror_count == 2 && sc->ferror_send == FALSE) {
			sc->ferror_send = TRUE;
			msg.param.code = __mmcamcorder_gst_handle_resource_error(handle, error->code, message);
		} else {
			sc->ferror_count++;
			_mmcam_dbg_warn("Skip error");
			return TRUE;
		}
	}

	if (error->domain == GST_CORE_ERROR) {
		msg.param.code = __mmcamcorder_gst_handle_core_error(handle, error->code, message);
	} else if (error->domain == GST_LIBRARY_ERROR) {
		msg.param.code = __mmcamcorder_gst_handle_library_error(handle, error->code, message);
	} else if (error->domain == GST_RESOURCE_ERROR) {
		msg.param.code = __mmcamcorder_gst_handle_resource_error(handle, error->code, message);
	} else if (error->domain == GST_STREAM_ERROR) {
		msg.param.code = __mmcamcorder_gst_handle_stream_error(handle, error->code, message);
	} else {
		debug_warning("This error domain is not defined.\n");

		/* we treat system error as an internal error */
		msg.param.code = MM_ERROR_CAMCORDER_INTERNAL;
	}

	if (message->src) {
		msg_src_element = GST_ELEMENT_NAME(GST_ELEMENT_CAST(message->src));
		debug_error("-Msg src : [%s]	Domain : [%s]   Error : [%s]  Code : [%d] is tranlated to error code : [0x%x]\n",
		            msg_src_element, g_quark_to_string (error->domain), error->message, error->code, msg.param.code);
	} else {
		debug_error("Domain : [%s]   Error : [%s]  Code : [%d] is tranlated to error code : [0x%x]\n",
		            g_quark_to_string (error->domain), error->message, error->code, msg.param.code);
	}

#ifdef _MMCAMCORDER_SKIP_GST_FLOW_ERROR
	/* Check whether send this error to application */
	if (msg.param.code == MM_ERROR_CAMCORDER_GST_FLOW_ERROR) {
		_mmcam_dbg_log("We got the error. But skip it.");
		return TRUE;
	}
#endif /* _MMCAMCORDER_SKIP_GST_FLOW_ERROR */

	/* post error to application */
	sc->error_occurs = TRUE;
	msg.id = MM_MESSAGE_CAMCORDER_ERROR;
	_mmcamcroder_send_message(handle, &msg);

	return TRUE;
}


static gint __mmcamcorder_gst_handle_core_error(MMHandleType handle, int code, GstMessage *message)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;
	GstElement *element = NULL;

	_mmcam_dbg_log("");

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	mmf_return_val_if_fail(sc, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	/* Specific plugin - video encoder plugin */
	element = GST_ELEMENT_CAST(sc->element[_MMCAMCORDER_ENCSINK_VENC].gst);

	if (GST_ELEMENT_CAST(message->src) == element) {
		if (code == GST_CORE_ERROR_NEGOTIATION) {
			return MM_ERROR_CAMCORDER_GST_NEGOTIATION;
		} else {
			return MM_ERROR_CAMCORDER_ENCODER;
		}
	}

	/* General */
	switch (code)
	{
		case GST_CORE_ERROR_STATE_CHANGE:
			return MM_ERROR_CAMCORDER_GST_STATECHANGE;
		case GST_CORE_ERROR_NEGOTIATION:
			return MM_ERROR_CAMCORDER_GST_NEGOTIATION;
		case GST_CORE_ERROR_MISSING_PLUGIN:
		case GST_CORE_ERROR_SEEK:
		case GST_CORE_ERROR_NOT_IMPLEMENTED:
		case GST_CORE_ERROR_FAILED:
		case GST_CORE_ERROR_TOO_LAZY:	
		case GST_CORE_ERROR_PAD:
		case GST_CORE_ERROR_THREAD:
		case GST_CORE_ERROR_EVENT:
		case GST_CORE_ERROR_CAPS:
		case GST_CORE_ERROR_TAG:
		case GST_CORE_ERROR_CLOCK:
		case GST_CORE_ERROR_DISABLED:
		default:
			return MM_ERROR_CAMCORDER_GST_CORE;
		break;
	}
}

static gint __mmcamcorder_gst_handle_library_error(MMHandleType handle, int code, GstMessage *message)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	_mmcam_dbg_log("");

	/* Specific plugin - NONE */

	/* General */
	switch (code) {
	case GST_LIBRARY_ERROR_FAILED:
	case GST_LIBRARY_ERROR_TOO_LAZY:
	case GST_LIBRARY_ERROR_INIT:
	case GST_LIBRARY_ERROR_SHUTDOWN:
	case GST_LIBRARY_ERROR_SETTINGS:
	case GST_LIBRARY_ERROR_ENCODE:
	default:
		_mmcam_dbg_err("Library error(%d)", code);
		return MM_ERROR_CAMCORDER_GST_LIBRARY;
	}
}


static gint __mmcamcorder_gst_handle_resource_error(MMHandleType handle, int code, GstMessage *message)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;
	GstElement *element = NULL;

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	mmf_return_val_if_fail(sc, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	_mmcam_dbg_log("");

	/* Specific plugin */
	/* video sink */
	element = GST_ELEMENT_CAST(sc->element[_MMCAMCORDER_VIDEOSINK_SINK].gst);
	if (GST_ELEMENT_CAST(message->src) == element) {
		if (code == GST_RESOURCE_ERROR_WRITE) {
			_mmcam_dbg_err("Display device [Off]");
			return MM_ERROR_CAMCORDER_DISPLAY_DEVICE_OFF;
		} else {
			_mmcam_dbg_err("Display device [General(%d)]", code);
		}
	}

	/* encodebin */
	element = GST_ELEMENT_CAST(sc->element[_MMCAMCORDER_ENCSINK_VENC].gst);
	if (GST_ELEMENT_CAST(message->src) == element) {
		if (code == GST_RESOURCE_ERROR_FAILED) {
			_mmcam_dbg_err("Encoder [Resource error]");
			return MM_ERROR_CAMCORDER_ENCODER_BUFFER;
		} else {
			_mmcam_dbg_err("Encoder [General(%d)]", code);
			return MM_ERROR_CAMCORDER_ENCODER;
		}
	}

	/* General */
	switch (code) {
	case GST_RESOURCE_ERROR_WRITE:
		_mmcam_dbg_err("File write error");
		return MM_ERROR_FILE_WRITE;
	case GST_RESOURCE_ERROR_NO_SPACE_LEFT:
		_mmcam_dbg_err("No left space");
		return MM_MESSAGE_CAMCORDER_NO_FREE_SPACE;
	case GST_RESOURCE_ERROR_OPEN_WRITE:
		_mmcam_dbg_err("Out of storage");
		return MM_ERROR_OUT_OF_STORAGE;
	case GST_RESOURCE_ERROR_SEEK:
		_mmcam_dbg_err("File read(seek)");
		return MM_ERROR_FILE_READ;
	case GST_RESOURCE_ERROR_NOT_FOUND:
	case GST_RESOURCE_ERROR_FAILED:
	case GST_RESOURCE_ERROR_TOO_LAZY:
	case GST_RESOURCE_ERROR_BUSY:
	case GST_RESOURCE_ERROR_OPEN_READ:
	case GST_RESOURCE_ERROR_OPEN_READ_WRITE:
	case GST_RESOURCE_ERROR_CLOSE:
	case GST_RESOURCE_ERROR_READ:
	case GST_RESOURCE_ERROR_SYNC:
	case GST_RESOURCE_ERROR_SETTINGS:
	default:
		_mmcam_dbg_err("Resource error(%d)", code);
		return MM_ERROR_CAMCORDER_GST_RESOURCE;
	}
}


static gint __mmcamcorder_gst_handle_stream_error(MMHandleType handle, int code, GstMessage *message)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;
	GstElement *element =NULL;

	mmf_return_val_if_fail( hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED );

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	mmf_return_val_if_fail(sc, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	_mmcam_dbg_log("");

	/* Specific plugin */
	/* video encoder */
	element = GST_ELEMENT_CAST(sc->element[_MMCAMCORDER_ENCSINK_VENC].gst);
	if (GST_ELEMENT_CAST(message->src) == element) {
		switch (code) {
		case GST_STREAM_ERROR_WRONG_TYPE:
			_mmcam_dbg_err("Video encoder [wrong stream type]");
			return MM_ERROR_CAMCORDER_ENCODER_WRONG_TYPE;
		case GST_STREAM_ERROR_ENCODE:
			_mmcam_dbg_err("Video encoder [encode error]");
			return MM_ERROR_CAMCORDER_ENCODER_WORKING;
		case GST_STREAM_ERROR_FAILED:
			_mmcam_dbg_err("Video encoder [stream failed]");
			return MM_ERROR_CAMCORDER_ENCODER_WORKING;
		default:
			_mmcam_dbg_err("Video encoder [General(%d)]", code);
			return MM_ERROR_CAMCORDER_ENCODER;
		}
	}

	/* General plugin */
	switch (code) {
	case GST_STREAM_ERROR_FORMAT:
		_mmcam_dbg_err("General [negotiation error(%d)]", code);
		return MM_ERROR_CAMCORDER_GST_NEGOTIATION;
	case GST_STREAM_ERROR_FAILED:
		_mmcam_dbg_err("General [flow error(%d)]", code);
		return MM_ERROR_CAMCORDER_GST_FLOW_ERROR;
	case GST_STREAM_ERROR_TYPE_NOT_FOUND:
	case GST_STREAM_ERROR_DECODE:
	case GST_STREAM_ERROR_CODEC_NOT_FOUND:
	case GST_STREAM_ERROR_NOT_IMPLEMENTED:
	case GST_STREAM_ERROR_TOO_LAZY:
	case GST_STREAM_ERROR_ENCODE:
	case GST_STREAM_ERROR_DEMUX:
	case GST_STREAM_ERROR_MUX:
	case GST_STREAM_ERROR_DECRYPT:
	case GST_STREAM_ERROR_DECRYPT_NOKEY:
	case GST_STREAM_ERROR_WRONG_TYPE:
	default:
		_mmcam_dbg_err("General [error(%d)]", code);
		return MM_ERROR_CAMCORDER_GST_STREAM;
	}
}


static gboolean __mmcamcorder_handle_gst_warning (MMHandleType handle, GstMessage *message, GError *error)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	gchar *debug = NULL;
	GError *err = NULL;

	return_val_if_fail(hcamcorder, FALSE);
	return_val_if_fail(error, FALSE);

	_mmcam_dbg_log("");

	gst_message_parse_warning(message, &err, &debug);

	if (error->domain == GST_CORE_ERROR) {
		_mmcam_dbg_warn("GST warning: GST_CORE domain");
	} else if (error->domain == GST_LIBRARY_ERROR) {
		_mmcam_dbg_warn("GST warning: GST_LIBRARY domain");
	} else if (error->domain == GST_RESOURCE_ERROR) {
		_mmcam_dbg_warn("GST warning: GST_RESOURCE domain");
		__mmcamcorder_gst_handle_resource_warning(handle, message, error);
	} else if (error->domain == GST_STREAM_ERROR ) {
		_mmcam_dbg_warn("GST warning: GST_STREAM domain");
	} else {
		_mmcam_dbg_warn("This error domain(%d) is not defined.", error->domain);
	}

	if (err != NULL) {
		g_error_free(err);
		err = NULL;
	}
	if (debug != NULL) {
		_mmcam_dbg_err ("Debug: %s", debug);
		g_free(debug);
		debug = NULL;
	}

	return TRUE;
}


static gint __mmcamcorder_gst_handle_resource_warning(MMHandleType handle, GstMessage *message , GError *error)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;
	GstElement *element =NULL;
	gchar *msg_src_element;

	mmf_return_val_if_fail( hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED );

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	mmf_return_val_if_fail(sc, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	_mmcam_dbg_log("");

	/* Special message handling */
	/* video source */
	element = GST_ELEMENT_CAST(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst);
	if (GST_ELEMENT_CAST(message->src) == element) {
		if (error->code == GST_RESOURCE_ERROR_FAILED) {
			msg_src_element = GST_ELEMENT_NAME(GST_ELEMENT_CAST(message->src));
			_mmcam_dbg_warn("-Msg src:[%s] Domain:[%s] Error:[%s]",
			               msg_src_element, g_quark_to_string(error->domain), error->message);
			return MM_ERROR_NONE;
		}
	}

	/* General plugin */
	switch (error->code) {
	case GST_RESOURCE_ERROR_WRITE:
	case GST_RESOURCE_ERROR_NO_SPACE_LEFT:
	case GST_RESOURCE_ERROR_SEEK:
	case GST_RESOURCE_ERROR_NOT_FOUND:
	case GST_RESOURCE_ERROR_FAILED:
	case GST_RESOURCE_ERROR_TOO_LAZY:
	case GST_RESOURCE_ERROR_BUSY:
	case GST_RESOURCE_ERROR_OPEN_READ:
	case GST_RESOURCE_ERROR_OPEN_WRITE:
	case GST_RESOURCE_ERROR_OPEN_READ_WRITE:
	case GST_RESOURCE_ERROR_CLOSE:
	case GST_RESOURCE_ERROR_READ:
	case GST_RESOURCE_ERROR_SYNC:
	case GST_RESOURCE_ERROR_SETTINGS:
	default:
		_mmcam_dbg_warn("General GST warning(%d)", error->code);
		break;
	}

	return MM_ERROR_NONE;
}
