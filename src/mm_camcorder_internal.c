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

#include <gst/video/colorbalance.h>
#include <gst/video/cameracontrol.h>

#include <system_info.h>

/*---------------------------------------------------------------------------------------
|    GLOBAL VARIABLE DEFINITIONS for internal						|
---------------------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------------------
|    LOCAL VARIABLE DEFINITIONS for internal						|
---------------------------------------------------------------------------------------*/
#define __MMCAMCORDER_CMD_ITERATE_MAX           3
#define __MMCAMCORDER_SET_GST_STATE_TIMEOUT     3
#define __MMCAMCORDER_SOUND_WAIT_TIMEOUT        3

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


#ifdef _MMCAMCORDER_USE_SET_ATTR_CB
static gboolean __mmcamcorder_set_attr_to_camsensor_cb(gpointer data);
#endif /* _MMCAMCORDER_USE_SET_ATTR_CB */

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
	int rcmd_fmt_capture = MM_PIXEL_FORMAT_YUYV;
	int rcmd_fmt_recording = MM_PIXEL_FORMAT_NV12;
	int rcmd_dpy_rotation = MM_DISPLAY_ROTATION_270;
	int play_capture_sound = TRUE;
	int camera_device_count = MM_VIDEO_DEVICE_NUM;
	int camera_facing_direction = MM_CAMCORDER_CAMERA_FACING_DIRECTION_REAR;
	char *err_attr_name = NULL;
	const char *ConfCtrlFile = NULL;
	mmf_camcorder_t *hcamcorder = NULL;
	type_element *EvasSurfaceElement = NULL;

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
	hcamcorder->capture_in_recording = FALSE;

	pthread_mutex_init(&((hcamcorder->mtsafe).lock), NULL);
	pthread_cond_init(&((hcamcorder->mtsafe).cond), NULL);

	pthread_mutex_init(&((hcamcorder->mtsafe).cmd_lock), NULL);
	pthread_mutex_init(&((hcamcorder->mtsafe).asm_lock), NULL);
	pthread_mutex_init(&((hcamcorder->mtsafe).state_lock), NULL);
	pthread_mutex_init(&((hcamcorder->mtsafe).gst_state_lock), NULL);
	pthread_mutex_init(&((hcamcorder->mtsafe).gst_encode_state_lock), NULL);
	pthread_mutex_init(&((hcamcorder->mtsafe).message_cb_lock), NULL);
	pthread_mutex_init(&((hcamcorder->mtsafe).vcapture_cb_lock), NULL);
	pthread_mutex_init(&((hcamcorder->mtsafe).vstream_cb_lock), NULL);
	pthread_mutex_init(&((hcamcorder->mtsafe).astream_cb_lock), NULL);

	pthread_mutex_init(&(hcamcorder->sound_lock), NULL);
	pthread_cond_init(&(hcamcorder->sound_cond), NULL);
	pthread_mutex_init(&(hcamcorder->restart_preview_lock), NULL);

	/* Sound mutex/cond init */
	pthread_mutex_init(&(hcamcorder->snd_info.open_mutex), NULL);
	pthread_cond_init(&(hcamcorder->snd_info.open_cond), NULL);

	/* init for sound thread */
	pthread_mutex_init(&(hcamcorder->task_thread_lock), NULL);
	pthread_cond_init(&(hcamcorder->task_thread_cond), NULL);
	hcamcorder->task_thread_state = _MMCAMCORDER_SOUND_STATE_NONE;

	/* create task thread */
	pthread_create(&(hcamcorder->task_thread), NULL, _mmcamcorder_util_task_thread_func, (void *)hcamcorder);

	if (info->videodev_type < MM_VIDEO_DEVICE_NONE ||
	    info->videodev_type >= MM_VIDEO_DEVICE_NUM) {
		_mmcam_dbg_err("_mmcamcorder_create::video device type is out of range.");
		ret = MM_ERROR_CAMCORDER_INVALID_ARGUMENT;
		goto _ERR_DEFAULT_VALUE_INIT;
	}

	/* set device type */
	hcamcorder->device_type = info->videodev_type;
	_mmcam_dbg_warn("Device Type : %d", hcamcorder->device_type);

	/* Get Camera Configure information from Camcorder INI file */
	_mmcamcorder_conf_get_info((MMHandleType)hcamcorder, CONFIGURE_TYPE_MAIN, CONFIGURE_MAIN_FILE, &hcamcorder->conf_main);

	if (!(hcamcorder->conf_main)) {
		_mmcam_dbg_err( "Failed to get configure(main) info." );

		ret = MM_ERROR_CAMCORDER_CREATE_CONFIGURE;
		goto _ERR_AFTER_ASM_REGISTER;
	}

	hcamcorder->attributes = _mmcamcorder_alloc_attribute((MMHandleType)hcamcorder, info);
	if (!(hcamcorder->attributes)) {
		_mmcam_dbg_err("_mmcamcorder_create::alloc attribute error.");

		ret = MM_ERROR_CAMCORDER_RESOURCE_CREATION;
		goto _ERR_AFTER_ASM_REGISTER;
	}

	if (info->videodev_type != MM_VIDEO_DEVICE_NONE) {
		_mmcamcorder_conf_get_value_int(handle, hcamcorder->conf_main,
		                                CONFIGURE_CATEGORY_MAIN_VIDEO_INPUT,
		                                "UseConfCtrl", &UseConfCtrl);

		if (UseConfCtrl) {
			_mmcam_dbg_log( "Enable Configure Control system." );

			switch (info->videodev_type) {
			case MM_VIDEO_DEVICE_CAMERA0:
				_mmcamcorder_conf_get_value_string((MMHandleType)hcamcorder, hcamcorder->conf_main,
				                                   CONFIGURE_CATEGORY_MAIN_VIDEO_INPUT,
				                                   "ConfCtrlFile0", &ConfCtrlFile);
				break;
			case MM_VIDEO_DEVICE_CAMERA1:
				_mmcamcorder_conf_get_value_string((MMHandleType)hcamcorder, hcamcorder->conf_main,
				                                   CONFIGURE_CATEGORY_MAIN_VIDEO_INPUT,
				                                   "ConfCtrlFile1", &ConfCtrlFile);
				break;
			default:
				_mmcam_dbg_err( "Not supported camera type." );
				ret = MM_ERROR_CAMCORDER_NOT_SUPPORTED;
				goto _ERR_AFTER_ASM_REGISTER;
			}

			_mmcam_dbg_log("videodev_type : [%d], ConfCtrlPath : [%s]", info->videodev_type, ConfCtrlFile);

			_mmcamcorder_conf_get_info((MMHandleType)hcamcorder, CONFIGURE_TYPE_CTRL, ConfCtrlFile, &hcamcorder->conf_ctrl);
/*
			_mmcamcorder_conf_print_info(&hcamcorder->conf_main);
			_mmcamcorder_conf_print_info(&hcamcorder->conf_ctrl);
*/
			if (!(hcamcorder->conf_ctrl)) {
				_mmcam_dbg_err( "Failed to get configure(control) info." );
				ret = MM_ERROR_CAMCORDER_CREATE_CONFIGURE;
				goto _ERR_AFTER_ASM_REGISTER;
			}

			ret = _mmcamcorder_init_convert_table((MMHandleType)hcamcorder);
			if (ret != MM_ERROR_NONE) {
				_mmcam_dbg_warn("converting table initialize error!!");
			}

			ret = _mmcamcorder_init_attr_from_configure((MMHandleType)hcamcorder);
			if (ret != MM_ERROR_NONE) {
				_mmcam_dbg_warn("converting table initialize error!!");
			}

			/* Get device info, recommend preview fmt and display rotation from INI */
			_mmcamcorder_conf_get_value_int(handle, hcamcorder->conf_ctrl,
			                                CONFIGURE_CATEGORY_CTRL_CAMERA,
			                                "RecommendPreviewFormatCapture",
			                                &rcmd_fmt_capture);

			_mmcamcorder_conf_get_value_int(handle, hcamcorder->conf_ctrl,
			                                CONFIGURE_CATEGORY_CTRL_CAMERA,
			                                "RecommendPreviewFormatRecord",
			                                &rcmd_fmt_recording);

			_mmcamcorder_conf_get_value_int(handle, hcamcorder->conf_ctrl,
			                                CONFIGURE_CATEGORY_CTRL_CAMERA,
			                                "RecommendDisplayRotation",
			                                &rcmd_dpy_rotation);

			_mmcamcorder_conf_get_value_int(handle, hcamcorder->conf_main,
			                                CONFIGURE_CATEGORY_MAIN_CAPTURE,
			                                "PlayCaptureSound",
			                                &play_capture_sound);

			_mmcamcorder_conf_get_value_int(handle, hcamcorder->conf_main,
			                                CONFIGURE_CATEGORY_MAIN_VIDEO_INPUT,
			                                "DeviceCount",
			                                &camera_device_count);

			_mmcamcorder_conf_get_value_int(handle, hcamcorder->conf_ctrl,
			                                CONFIGURE_CATEGORY_CTRL_CAMERA,
			                                "FacingDirection",
			                                &camera_facing_direction);

			_mmcamcorder_conf_get_value_int(handle, hcamcorder->conf_ctrl,
			                                CONFIGURE_CATEGORY_CTRL_EFFECT,
			                                "BrightnessStepDenominator",
			                                &hcamcorder->brightness_step_denominator);

			_mmcamcorder_conf_get_value_int(handle, hcamcorder->conf_ctrl,
			                                CONFIGURE_CATEGORY_CTRL_CAPTURE,
			                                "SupportZSL",
			                                &hcamcorder->support_zsl_capture);

			_mmcam_dbg_log("Recommend fmt[cap:%d,rec:%d], dpy rot %d, cap snd %d, dev cnt %d, cam facing dir %d, step denom %d, support zsl %d",
			               rcmd_fmt_capture, rcmd_fmt_recording, rcmd_dpy_rotation,
			               play_capture_sound, camera_device_count, camera_facing_direction,
			               hcamcorder->brightness_step_denominator, hcamcorder->support_zsl_capture);

			/* Get UseZeroCopyFormat value from INI */
			_mmcamcorder_conf_get_value_int(handle, hcamcorder->conf_main,
			                                CONFIGURE_CATEGORY_MAIN_VIDEO_INPUT,
			                                "UseZeroCopyFormat",
			                                &(hcamcorder->use_zero_copy_format));

			/* Get SupportMediaPacketPreviewCb value from INI */
			_mmcamcorder_conf_get_value_int(handle, hcamcorder->conf_main,
			                                CONFIGURE_CATEGORY_MAIN_VIDEO_INPUT,
			                                "SupportMediaPacketPreviewCb",
			                                &(hcamcorder->support_media_packet_preview_cb));

			_mmcam_dbg_log("UseZeroCopyFormat : %d", hcamcorder->use_zero_copy_format);
			_mmcam_dbg_log("SupportMediaPacketPreviewCb : %d", hcamcorder->support_media_packet_preview_cb);

			mm_camcorder_set_attributes((MMHandleType)hcamcorder, &err_attr_name,
			                            MMCAM_CAMERA_DEVICE_COUNT, camera_device_count,
			                            MMCAM_CAMERA_FACING_DIRECTION, camera_facing_direction,
			                            MMCAM_RECOMMEND_PREVIEW_FORMAT_FOR_CAPTURE, rcmd_fmt_capture,
			                            MMCAM_RECOMMEND_PREVIEW_FORMAT_FOR_RECORDING, rcmd_fmt_recording,
			                            MMCAM_RECOMMEND_DISPLAY_ROTATION, rcmd_dpy_rotation,
			                            MMCAM_SUPPORT_ZSL_CAPTURE, hcamcorder->support_zsl_capture,
			                            MMCAM_SUPPORT_ZERO_COPY_FORMAT, hcamcorder->use_zero_copy_format,
			                            MMCAM_SUPPORT_MEDIA_PACKET_PREVIEW_CB, hcamcorder->support_media_packet_preview_cb,
			                            "capture-sound-enable", play_capture_sound,
			                            NULL);
			if (err_attr_name) {
				_mmcam_dbg_err("Set %s FAILED.", err_attr_name);
				free(err_attr_name);
				err_attr_name = NULL;
			}

			/* Get default value of brightness */
			mm_camcorder_get_attributes((MMHandleType)hcamcorder, &err_attr_name,
			                            MMCAM_FILTER_BRIGHTNESS, &hcamcorder->brightness_default,
			                            NULL);
			if (err_attr_name) {
				_mmcam_dbg_err("Get brightness FAILED.");
				free(err_attr_name);
				err_attr_name = NULL;
			}
			_mmcam_dbg_log("Default brightness : %d", hcamcorder->brightness_default);
		} else {
			_mmcam_dbg_log( "Disable Configure Control system." );
			hcamcorder->conf_ctrl = NULL;
		}
	}

	ret = __mmcamcorder_gstreamer_init(hcamcorder->conf_main);
	if (!ret) {
		_mmcam_dbg_err( "Failed to initialize gstreamer!!" );
		ret = MM_ERROR_CAMCORDER_NOT_INITIALIZED;
		goto _ERR_AFTER_ASM_REGISTER;
	}

	/* Make some attributes as read-only type */
	_mmcamcorder_lock_readonly_attributes((MMHandleType)hcamcorder);

	/* Disable attributes in each model */
	_mmcamcorder_set_disabled_attributes((MMHandleType)hcamcorder);

	/* Get videosink name for evas surface */
	_mmcamcorder_conf_get_element((MMHandleType)hcamcorder, hcamcorder->conf_main,
	                              CONFIGURE_CATEGORY_MAIN_VIDEO_OUTPUT,
	                              "VideosinkElementEvas",
	                              &EvasSurfaceElement);
	if (EvasSurfaceElement) {
		int attr_index = 0;
		const char *evassink_name = NULL;
		mmf_attribute_t *item_evassink_name = NULL;
		mmf_attrs_t *attrs = (mmf_attrs_t *)MMF_CAMCORDER_ATTRS(hcamcorder);

		_mmcamcorder_conf_get_value_element_name(EvasSurfaceElement, &evassink_name);
		mm_attrs_get_index((MMHandleType)attrs, MMCAM_DISPLAY_EVAS_SURFACE_SINK, &attr_index);
		item_evassink_name = &attrs->items[attr_index];
		mmf_attribute_set_string(item_evassink_name, evassink_name, strlen(evassink_name));
		mmf_attribute_commit(item_evassink_name);

		_mmcam_dbg_log("Evassink name : %s", evassink_name);
	}

	/* get shutter sound policy */
	vconf_get_int(VCONFKEY_CAMERA_SHUTTER_SOUND_POLICY, &hcamcorder->shutter_sound_policy);
	_mmcam_dbg_log("current shutter sound policy : %d", hcamcorder->shutter_sound_policy);

	/* get model name */
	system_info_get_value_string(SYSTEM_INFO_KEY_MODEL, &hcamcorder->model_name);

	/* get software version */
	system_info_get_value_string(SYSTEM_INFO_KEY_BUILD_STRING, &hcamcorder->software_version);

	/* Set initial state */
	_mmcamcorder_set_state((MMHandleType)hcamcorder, MM_CAMCORDER_STATE_NULL);

	_mmcam_dbg_log("created handle %p", hcamcorder);

	*handle = (MMHandleType)hcamcorder;

	return MM_ERROR_NONE;

_ERR_AFTER_ASM_REGISTER:

_ERR_DEFAULT_VALUE_INIT:
	/* Release lock, cond */
	pthread_mutex_destroy(&((hcamcorder->mtsafe).lock));
	pthread_cond_destroy(&((hcamcorder->mtsafe).cond));
	pthread_mutex_destroy(&((hcamcorder->mtsafe).cmd_lock));
	pthread_mutex_destroy(&((hcamcorder->mtsafe).asm_lock));
	pthread_mutex_destroy(&((hcamcorder->mtsafe).state_lock));
	pthread_mutex_destroy(&((hcamcorder->mtsafe).gst_state_lock));
	pthread_mutex_destroy(&((hcamcorder->mtsafe).gst_encode_state_lock));
	pthread_mutex_destroy(&((hcamcorder->mtsafe).message_cb_lock));
	pthread_mutex_destroy(&((hcamcorder->mtsafe).vcapture_cb_lock));
	pthread_mutex_destroy(&((hcamcorder->mtsafe).vstream_cb_lock));
	pthread_mutex_destroy(&((hcamcorder->mtsafe).astream_cb_lock));

	pthread_mutex_destroy(&(hcamcorder->sound_lock));
	pthread_cond_destroy(&(hcamcorder->sound_cond));
	pthread_mutex_destroy(&(hcamcorder->snd_info.open_mutex));
	pthread_cond_destroy(&(hcamcorder->snd_info.open_cond));
	pthread_mutex_destroy(&(hcamcorder->restart_preview_lock));

	if (hcamcorder->conf_ctrl) {
		_mmcamcorder_conf_release_info(handle, &hcamcorder->conf_ctrl);
	}

	if (hcamcorder->conf_main) {
		_mmcamcorder_conf_release_info(handle, &hcamcorder->conf_main);
	}

	if (hcamcorder->model_name) {
		free(hcamcorder->model_name);
		hcamcorder->model_name = NULL;
	}

	if (hcamcorder->software_version) {
		free(hcamcorder->software_version);
		hcamcorder->software_version = NULL;
	}

	/* Release handle */
	memset(hcamcorder, 0x00, sizeof(mmf_camcorder_t));
	free(hcamcorder);

	return ret;
}


int _mmcamcorder_destroy(MMHandleType handle)
{
	int ret = MM_ERROR_NONE;
	int state = MM_CAMCORDER_STATE_NONE;
	int state_FROM = MM_CAMCORDER_STATE_NULL;

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

	/* set exit state for sound thread */
	pthread_mutex_lock(&(hcamcorder->task_thread_lock));
	_mmcam_dbg_log("send signal for task thread exit");
	hcamcorder->task_thread_state = _MMCAMCORDER_TASK_THREAD_STATE_EXIT;
	pthread_cond_signal(&(hcamcorder->task_thread_cond));
	pthread_mutex_unlock(&(hcamcorder->task_thread_lock));

	pthread_mutex_lock(&(hcamcorder->sound_lock));
	while (hcamcorder->capture_sound_count > 0) {
		struct timespec timeout;
		struct timeval tv;

		gettimeofday(&tv, NULL);
		timeout.tv_sec = tv.tv_sec + __MMCAMCORDER_SOUND_WAIT_TIMEOUT;
		timeout.tv_nsec = tv.tv_usec * 1000;

		_mmcam_dbg_warn("capture_sound_count[%d] is not zero. wait signal...",
		                hcamcorder->capture_sound_count);

		if (!pthread_cond_timedwait(&(hcamcorder->sound_cond), &(hcamcorder->sound_lock), &timeout)) {
			_mmcam_dbg_warn("signal received. check again...");
		} else {
			hcamcorder->capture_sound_count = 0;
			_mmcam_dbg_err("capture sound completion wait time out");
			break;
		}
	}
	pthread_mutex_unlock(&(hcamcorder->sound_lock));

	/* Release SubContext and pipeline */
	if (hcamcorder->sub_context) {
		if (hcamcorder->sub_context->element) {
			_mmcamcorder_destroy_pipeline(handle, hcamcorder->type);
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
		_mmcamcorder_dealloc_attribute(handle, hcamcorder->attributes);
		hcamcorder->attributes = 0;
	}

	/* Remove exif info */
	if (hcamcorder->exif_info) {
		mm_exif_destory_exif_info(hcamcorder->exif_info);
		hcamcorder->exif_info=NULL;

	}

	/* Release configure info */
	if (hcamcorder->conf_ctrl) {
		_mmcamcorder_conf_release_info(handle, &hcamcorder->conf_ctrl);
	}
	if (hcamcorder->conf_main) {
		_mmcamcorder_conf_release_info(handle, &hcamcorder->conf_main);
	}

	/* Remove messages which are not called yet */
	_mmcamcroder_remove_message_all(handle);

	/* release model_name */
	if (hcamcorder->model_name) {
		free(hcamcorder->model_name);
		hcamcorder->model_name = NULL;
	}

	if (hcamcorder->software_version) {
		free(hcamcorder->software_version);
		hcamcorder->software_version = NULL;
	}

	/* join task thread */
	_mmcam_dbg_log("task thread join");
	pthread_join(hcamcorder->task_thread, NULL);

	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

	/* Release lock, cond */
	pthread_mutex_destroy(&((hcamcorder->mtsafe).lock));
	pthread_cond_destroy(&((hcamcorder->mtsafe).cond));
	pthread_mutex_destroy(&((hcamcorder->mtsafe).cmd_lock));
	pthread_mutex_destroy(&((hcamcorder->mtsafe).asm_lock));
	pthread_mutex_destroy(&((hcamcorder->mtsafe).state_lock));
	pthread_mutex_destroy(&((hcamcorder->mtsafe).gst_state_lock));
	pthread_mutex_destroy(&((hcamcorder->mtsafe).gst_encode_state_lock));
	pthread_mutex_destroy(&((hcamcorder->mtsafe).message_cb_lock));
	pthread_mutex_destroy(&((hcamcorder->mtsafe).vcapture_cb_lock));
	pthread_mutex_destroy(&((hcamcorder->mtsafe).vstream_cb_lock));
	pthread_mutex_destroy(&((hcamcorder->mtsafe).astream_cb_lock));

	pthread_mutex_destroy(&(hcamcorder->sound_lock));
	pthread_cond_destroy(&(hcamcorder->sound_cond));
	pthread_mutex_destroy(&(hcamcorder->snd_info.open_mutex));
	pthread_cond_destroy(&(hcamcorder->snd_info.open_cond));
	pthread_mutex_destroy(&(hcamcorder->restart_preview_lock));
	pthread_mutex_destroy(&(hcamcorder->task_thread_lock));
	pthread_cond_destroy(&(hcamcorder->task_thread_cond));

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
	int display_surface_type = MM_DISPLAY_SURFACE_X;
	double motion_rate = _MMCAMCORDER_DEFAULT_RECORDING_MOTION_RATE;
	char *videosink_element_type = NULL;
	const char *videosink_name = NULL;

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

	mm_camcorder_get_attributes(handle, NULL,
	                            MMCAM_MODE, &hcamcorder->type,
	                            NULL);

	/* Get profile mode */
	_mmcam_dbg_log("Profile mode [%d]", hcamcorder->type);

	/* set camera state to vconf key */
	if (hcamcorder->type != MM_CAMCORDER_MODE_AUDIO) {
		int vconf_camera_state = 0;

		/* get current camera state of vconf key */
		vconf_get_int(VCONFKEY_CAMERA_STATE, &vconf_camera_state);
		vconf_set_int(VCONFKEY_CAMERA_STATE, VCONFKEY_CAMERA_STATE_OPEN);

		_mmcam_dbg_log("VCONFKEY_CAMERA_STATE prev %d -> cur %d",
		               vconf_camera_state, VCONFKEY_CAMERA_STATE_OPEN);
	}

	mm_camcorder_get_attributes(handle, NULL,
	                            MMCAM_DISPLAY_SURFACE, &display_surface_type,
	                            MMCAM_CAMERA_RECORDING_MOTION_RATE, &motion_rate,
	                            NULL);

	/* alloc sub context */
	hcamcorder->sub_context = _mmcamcorder_alloc_subcontext(hcamcorder->type);
	if(!hcamcorder->sub_context) {
		ret = MM_ERROR_CAMCORDER_RESOURCE_CREATION;
		goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
	}

	/* Set basic configure information */
	if (motion_rate != _MMCAMCORDER_DEFAULT_RECORDING_MOTION_RATE) {
		hcamcorder->sub_context->is_modified_rate = TRUE;
	}

	_mmcamcorder_conf_get_value_int(handle, hcamcorder->conf_main,
	                                CONFIGURE_CATEGORY_MAIN_CAPTURE,
	                                "UseEncodebin",
	                                &(hcamcorder->sub_context->bencbin_capture));
	_mmcam_dbg_warn("UseEncodebin [%d]", hcamcorder->sub_context->bencbin_capture);

	if (hcamcorder->type != MM_CAMCORDER_MODE_AUDIO) {
		/* get dual stream support info */
		_mmcamcorder_conf_get_value_int(handle, hcamcorder->conf_main,
		                                CONFIGURE_CATEGORY_MAIN_RECORD,
		                                "SupportDualStream",
		                                &(hcamcorder->sub_context->info_video->support_dual_stream));
		_mmcam_dbg_warn("SupportDualStream [%d]", hcamcorder->sub_context->info_video->support_dual_stream);
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
		_mmcamcorder_conf_get_element(handle, hcamcorder->conf_main,
		                              CONFIGURE_CATEGORY_MAIN_VIDEO_OUTPUT,
		                              videosink_element_type,
		                              &hcamcorder->sub_context->VideosinkElement);
		free(videosink_element_type);
		videosink_element_type = NULL;
	} else {
		_mmcam_dbg_warn("strdup failed(display_surface_type %d). Use default X type",
		                display_surface_type);

		_mmcamcorder_conf_get_element(handle, hcamcorder->conf_main,
		                              CONFIGURE_CATEGORY_MAIN_VIDEO_OUTPUT,
		                              _MMCAMCORDER_DEFAULT_VIDEOSINK_TYPE,
		                              &hcamcorder->sub_context->VideosinkElement);
	}

	_mmcamcorder_conf_get_value_element_name(hcamcorder->sub_context->VideosinkElement, &videosink_name);
	_mmcam_dbg_log("Videosink name : %s", videosink_name);

	/* get videoconvert element */
	_mmcamcorder_conf_get_element(handle, hcamcorder->conf_main,
	                              CONFIGURE_CATEGORY_MAIN_VIDEO_OUTPUT,
	                              "VideoconvertElement",
	                              &hcamcorder->sub_context->VideoconvertElement);

	_mmcamcorder_conf_get_value_int(handle, hcamcorder->conf_ctrl,
	                                CONFIGURE_CATEGORY_CTRL_CAPTURE,
	                                "SensorEncodedCapture",
	                                &(hcamcorder->sub_context->SensorEncodedCapture));
	_mmcam_dbg_log("Support sensor encoded capture : %d", hcamcorder->sub_context->SensorEncodedCapture);

	/* create pipeline */
	ret = _mmcamcorder_create_pipeline(handle, hcamcorder->type);
	if (ret != MM_ERROR_NONE) {
		/* check internal error of gstreamer */
		if (hcamcorder->sub_context->error_code != MM_ERROR_NONE) {
			ret = hcamcorder->sub_context->error_code;
			_mmcam_dbg_log("gstreamer error is occurred. return it %x", ret);
		}

		/* release sub context */
		_mmcamcorder_dealloc_subcontext(hcamcorder->sub_context);
		hcamcorder->sub_context = NULL;
		goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
	}

	/* set command function */
	ret = _mmcamcorder_set_functions(handle, hcamcorder->type);
	if (ret != MM_ERROR_NONE) {
		_mmcamcorder_destroy_pipeline(handle, hcamcorder->type);
		_mmcamcorder_dealloc_subcontext(hcamcorder->sub_context);
		hcamcorder->sub_context = NULL;
		goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
	}

	_mmcamcorder_set_state(handle, state_TO);

	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

	return MM_ERROR_NONE;

_ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK:
	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

_ERR_CAMCORDER_CMD_PRECON:
	_mmcam_dbg_err("Realize fail (type %d, state %d, ret %x)",
	               hcamcorder->type, state, ret);

	/* rollback camera state to vconf key */
	if (hcamcorder->type != MM_CAMCORDER_MODE_AUDIO) {
		int vconf_camera_state = 0;

		/* get current camera state of vconf key */
		vconf_get_int(VCONFKEY_CAMERA_STATE, &vconf_camera_state);
		vconf_set_int(VCONFKEY_CAMERA_STATE, VCONFKEY_CAMERA_STATE_NULL);

		_mmcam_dbg_log("VCONFKEY_CAMERA_STATE prev %d -> cur %d",
		               vconf_camera_state, VCONFKEY_CAMERA_STATE_NULL);
	}

	return ret;
}


int _mmcamcorder_unrealize(MMHandleType handle)
{
	int ret = MM_ERROR_NONE;
	int state = MM_CAMCORDER_STATE_NONE;
	int state_FROM = MM_CAMCORDER_STATE_READY;
	int state_TO = MM_CAMCORDER_STATE_NULL;

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
		/* TODO */
	}

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

	_mmcamcorder_set_state(handle, state_TO);

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

	/* set attributes related sensor */
	if (hcamcorder->type != MM_CAMCORDER_MODE_AUDIO) {
		_mmcamcorder_set_attribute_to_camsensor(handle);
	}

	ret = hcamcorder->command((MMHandleType)hcamcorder, _MMCamcorder_CMD_PREVIEW_START);
	if (ret != MM_ERROR_NONE) {
		/* check internal error of gstreamer */
		if (hcamcorder->sub_context->error_code != MM_ERROR_NONE) {
			ret = hcamcorder->sub_context->error_code;
			_mmcam_dbg_log("gstreamer error is occurred. return it %x", ret);
		}
		goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
	}

	_mmcamcorder_set_state(handle, state_TO);

	if (hcamcorder->type != MM_CAMCORDER_MODE_AUDIO) {
		int vconf_camera_state = 0;

		/* check camera state of vconf key */
		vconf_get_int(VCONFKEY_CAMERA_STATE, &vconf_camera_state);

		/* set camera state to vconf key */
		vconf_set_int(VCONFKEY_CAMERA_STATE, VCONFKEY_CAMERA_STATE_PREVIEW);

		_mmcam_dbg_log("VCONFKEY_CAMERA_STATE prev %d -> cur %d",
		               vconf_camera_state, VCONFKEY_CAMERA_STATE_PREVIEW);
	}

	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

	return MM_ERROR_NONE;

_ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK:
	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

_ERR_CAMCORDER_CMD_PRECON:
	/* check internal error of gstreamer */
	if (hcamcorder->sub_context &&
	    hcamcorder->sub_context->error_code != MM_ERROR_NONE) {
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

	ret = hcamcorder->command((MMHandleType)hcamcorder, _MMCamcorder_CMD_PREVIEW_STOP);
	if (ret != MM_ERROR_NONE) {
		goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
	}

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
	int state_FROM_0 = MM_CAMCORDER_STATE_PREPARE;
	int state_FROM_1 = MM_CAMCORDER_STATE_RECORDING;
	int state_FROM_2 = MM_CAMCORDER_STATE_PAUSED;
	int state_TO = MM_CAMCORDER_STATE_CAPTURING;

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
	if (state != state_FROM_0 &&
	    state != state_FROM_1 &&
	    state != state_FROM_2) {
		_mmcam_dbg_err("Wrong state(%d)", state);
		ret = MM_ERROR_CAMCORDER_INVALID_STATE;
		goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
	}

	/* Handle capture in recording case */
	if (state == state_FROM_1 || state == state_FROM_2) {
		if (hcamcorder->capture_in_recording == TRUE) {
			_mmcam_dbg_err("Capturing in recording (%d)", state);
			ret = MM_ERROR_CAMCORDER_DEVICE_BUSY;
			goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
		} else {
			hcamcorder->capture_in_recording = TRUE;
		}
	}

	ret = hcamcorder->command((MMHandleType)hcamcorder, _MMCamcorder_CMD_CAPTURE);
	if (ret != MM_ERROR_NONE) {
		goto _ERR_CAMCORDER_CMD;
	}

	/* Do not change state when recording snapshot capture */
	if (state == state_FROM_0) {
		_mmcamcorder_set_state(handle, state_TO);
	}

	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

	/* Init break continuous shot attr */
	if (mm_camcorder_set_attributes(handle, NULL, "capture-break-cont-shot", 0, NULL) != MM_ERROR_NONE) {
		_mmcam_dbg_warn("capture-break-cont-shot set 0 failed");
	}

	return MM_ERROR_NONE;

_ERR_CAMCORDER_CMD:
	if (hcamcorder->capture_in_recording) {
		hcamcorder->capture_in_recording = FALSE;
	}

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

	ret = hcamcorder->command((MMHandleType)hcamcorder, _MMCamcorder_CMD_PREVIEW_START);
	if (ret != MM_ERROR_NONE) {
		goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
	}

	_mmcamcorder_set_state(handle, state_TO);

	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

	return MM_ERROR_NONE;

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

	ret = hcamcorder->command((MMHandleType)hcamcorder, _MMCamcorder_CMD_RECORD);
	if (ret != MM_ERROR_NONE) {
		/* check internal error of gstreamer */
		if (hcamcorder->sub_context->error_code != MM_ERROR_NONE) {
			ret = hcamcorder->sub_context->error_code;
			_mmcam_dbg_log("gstreamer error is occurred. return it %x", ret);
		}
		goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
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

_ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK:
	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

_ERR_CAMCORDER_CMD_PRECON:
	/* check internal error of gstreamer */
	if (hcamcorder->sub_context &&
	    hcamcorder->sub_context->error_code != MM_ERROR_NONE) {
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

	ret = hcamcorder->command((MMHandleType)hcamcorder, _MMCamcorder_CMD_PAUSE);
	if (ret != MM_ERROR_NONE) {
		goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
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

	ret = hcamcorder->command((MMHandleType)hcamcorder, _MMCamcorder_CMD_COMMIT);
	if (ret != MM_ERROR_NONE) {
		goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
	}

	/* set camera state to vconf key */
	if (hcamcorder->type != MM_CAMCORDER_MODE_AUDIO) {
		int vconf_camera_state = 0;

		/* get current camera state of vconf key */
		vconf_get_int(VCONFKEY_CAMERA_STATE, &vconf_camera_state);
		vconf_set_int(VCONFKEY_CAMERA_STATE, VCONFKEY_CAMERA_STATE_PREVIEW);

		_mmcam_dbg_log("VCONFKEY_CAMERA_STATE prev %d -> cur %d",
		               vconf_camera_state, VCONFKEY_CAMERA_STATE_PREVIEW);
	}

	/* check who calls unrealize. it's no need to set ASM state if caller is ASM */
	if (hcamcorder->state_change_by_system != _MMCAMCORDER_STATE_CHANGE_BY_ASM) {
		/* TODO */
	}

	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

	_mmcamcorder_set_state(handle,state_TO);

	return MM_ERROR_NONE;

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

	ret = hcamcorder->command((MMHandleType)hcamcorder, _MMCamcorder_CMD_CANCEL);
	if (ret != MM_ERROR_NONE) {
		goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
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

	if (!_MMCAMCORDER_TRYLOCK_CMD(handle)) {
		_mmcam_dbg_err("Another command is running.");
		return MM_ERROR_CAMCORDER_CMD_IS_RUNNING;
	}

	state = _mmcamcorder_get_state(handle);
	if (state == MM_CAMCORDER_STATE_CAPTURING ||
	    state < MM_CAMCORDER_STATE_PREPARE) {
		_mmcam_dbg_err("Not proper state. state[%d]", state);
		_MMCAMCORDER_UNLOCK_CMD(handle);
		return MM_ERROR_CAMCORDER_INVALID_STATE;
	}

	/* TODO : call a auto or manual focus function */
	ret = mm_camcorder_get_attributes(handle, &err_attr_name,
	                                  MMCAM_CAMERA_FOCUS_MODE, &focus_mode,
	                                  NULL);
	if (ret != MM_ERROR_NONE) {
		_mmcam_dbg_warn("Get focus-mode fail. (%s:%x)", err_attr_name, ret);
		SAFE_FREE (err_attr_name);
		_MMCAMCORDER_UNLOCK_CMD(handle);
		return ret;
	}

	if (focus_mode == MM_CAMCORDER_FOCUS_MODE_MANUAL) {
		ret = _mmcamcorder_adjust_manual_focus(handle, direction);
	} else if (focus_mode == MM_CAMCORDER_FOCUS_MODE_AUTO ||
		   focus_mode == MM_CAMCORDER_FOCUS_MODE_TOUCH_AUTO ||
		   focus_mode == MM_CAMCORDER_FOCUS_MODE_CONTINUOUS) {
		ret = _mmcamcorder_adjust_auto_focus(handle);
	} else {
		_mmcam_dbg_err("It doesn't adjust focus. Focusing mode(%d)", focus_mode);
		ret = MM_ERROR_CAMCORDER_NOT_SUPPORTED;
	}

	_MMCAMCORDER_UNLOCK_CMD(handle);

	return ret;
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
	ret = gst_init_check (argc, &argv, &err);
	if (!ret) {
		_mmcam_dbg_err("Could not initialize GStreamer: %s ",
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

	sc->element_num = _MMCAMCORDER_PIPELINE_ELEMENT_NUM;
	sc->encode_element_num = _MMCAMCORDER_ENCODE_PIPELINE_ELEMENT_NUM;

	/* alloc info for each mode */
	switch (type) {
	case MM_CAMCORDER_MODE_AUDIO:
		sc->info_audio = malloc( sizeof(_MMCamcorderAudioInfo));
		if(!sc->info_audio) {
			_mmcam_dbg_err("Failed to alloc info structure");
			goto ALLOC_SUBCONTEXT_FAILED;
		}
		memset(sc->info_audio, 0x00, sizeof(_MMCamcorderAudioInfo));
		break;
	case MM_CAMCORDER_MODE_VIDEO_CAPTURE:
	default:
		sc->info_image = malloc( sizeof(_MMCamcorderImageInfo));
		if(!sc->info_image) {
			_mmcam_dbg_err("Failed to alloc info structure");
			goto ALLOC_SUBCONTEXT_FAILED;
		}
		memset(sc->info_image, 0x00, sizeof(_MMCamcorderImageInfo));

		/* init sound status */
		sc->info_image->sound_status = _SOUND_STATUS_INIT;

		sc->info_video = malloc( sizeof(_MMCamcorderVideoInfo));
		if(!sc->info_video) {
			_mmcam_dbg_err("Failed to alloc info structure");
			goto ALLOC_SUBCONTEXT_FAILED;
		}
		memset(sc->info_video, 0x00, sizeof(_MMCamcorderVideoInfo));
		pthread_mutex_init(&(sc->info_video->size_check_lock), NULL);
		break;
	}

	/* alloc element array */
	sc->element = (_MMCamcorderGstElement *)malloc(sizeof(_MMCamcorderGstElement) * sc->element_num);
	if(!sc->element) {
		_mmcam_dbg_err("Failed to alloc element structure");
		goto ALLOC_SUBCONTEXT_FAILED;
	}

	sc->encode_element = (_MMCamcorderGstElement *)malloc(sizeof(_MMCamcorderGstElement) * sc->encode_element_num);
	if(!sc->encode_element) {
		_mmcam_dbg_err("Failed to alloc encode element structure");
		goto ALLOC_SUBCONTEXT_FAILED;
	}

	for (i = 0 ; i < sc->element_num ; i++) {
		sc->element[i].id = _MMCAMCORDER_NONE;
		sc->element[i].gst = NULL;
	}

	for (i = 0 ; i < sc->encode_element_num ; i++) {
		sc->encode_element[i].id = _MMCAMCORDER_NONE;
		sc->encode_element[i].gst = NULL;
	}

	sc->cam_stability_count = 0;
	sc->drop_vframe = 0;
	sc->pass_first_vframe = 0;
	sc->is_modified_rate = FALSE;

	return sc;

ALLOC_SUBCONTEXT_FAILED:
	if (sc) {
		if (sc->info_audio) {
			free(sc->info_audio);
			sc->info_audio = NULL;
		}
		if (sc->info_image) {
			free(sc->info_image);
			sc->info_image = NULL;
		}
		if (sc->info_video) {
			pthread_mutex_destroy(&(sc->info_video->size_check_lock));
			free(sc->info_video);
			sc->info_video = NULL;
		}
		if (sc->element) {
			free(sc->element);
			sc->element = NULL;
		}
		if (sc->encode_element) {
			free(sc->encode_element);
			sc->encode_element = NULL;
		}
		free(sc);
		sc = NULL;
	}

	return NULL;
}


void _mmcamcorder_dealloc_subcontext(_MMCamcorderSubContext *sc)
{
	_mmcam_dbg_log("");

	if (sc) {
		if (sc->element) {
			_mmcam_dbg_log("release element");
			free(sc->element);
			sc->element = NULL;
		}

		if (sc->encode_element) {
			_mmcam_dbg_log("release encode_element");
			free(sc->encode_element);
			sc->encode_element = NULL;
		}

		if (sc->info_image) {
			_mmcam_dbg_log("release info_image");
			free(sc->info_image);
			sc->info_image = NULL;
		}

		if (sc->info_video) {
			_mmcam_dbg_log("release info_video");
			if (sc->info_video->filename) {
				free(sc->info_video->filename);
				sc->info_video->filename = NULL;
			}
			pthread_mutex_destroy(&(sc->info_video->size_check_lock));
			free(sc->info_video);
			sc->info_video = NULL;
		}

		if (sc->info_audio) {
			_mmcam_dbg_log("release info_audio");
			if (sc->info_audio->filename) {
				free(sc->info_audio->filename);
				sc->info_audio->filename = NULL;
			}
			free(sc->info_audio);
			sc->info_audio = NULL;
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
	case MM_CAMCORDER_MODE_AUDIO:
		hcamcorder->command = _mmcamcorder_audio_command;
		break;
	case MM_CAMCORDER_MODE_VIDEO_CAPTURE:
	default:
		hcamcorder->command = _mmcamcorder_video_capture_command;
		break;
	}

	return MM_ERROR_NONE;
}


gboolean _mmcamcorder_pipeline_cb_message(GstBus *bus, GstMessage *message, gpointer data)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(data);
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

		if (hcamcorder->type != MM_CAMCORDER_MODE_AUDIO) {
			mmf_return_val_if_fail(sc->info_video, TRUE);
			if (sc->info_video->b_commiting) {
				_mmcamcorder_video_handle_eos((MMHandleType)hcamcorder);
			}
		} else {
			mmf_return_val_if_fail(sc->info_audio, TRUE);
			if (sc->info_audio->b_commiting) {
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
		const GValue *vnewstate;
		GstState newstate;
		GstElement *pipeline = NULL;

		sc = MMF_CAMCORDER_SUBCONTEXT(hcamcorder);
		if ((sc) && (sc->element)) {
			if (sc->element[_MMCAMCORDER_MAIN_PIPE].gst) {
				pipeline = sc->element[_MMCAMCORDER_MAIN_PIPE].gst;
				if (message->src == (GstObject*)pipeline) {
					vnewstate = (GValue*)gst_structure_get_value(gst_message_get_structure(message), "new-state");
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
		GstClock *pipe_clock = NULL;
		gst_message_parse_new_clock(message, &pipe_clock);
		_mmcam_dbg_log("GST_MESSAGE_NEW_CLOCK : %s", (pipe_clock ? GST_OBJECT_NAME (pipe_clock) : "NULL"));
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
		break;
	case GST_MESSAGE_SEGMENT_START:
		_mmcam_dbg_log("GST_MESSAGE_SEGMENT_START");
		break;
	case GST_MESSAGE_SEGMENT_DONE:
		_mmcam_dbg_log("GST_MESSAGE_SEGMENT_DONE");
		break;
	case GST_MESSAGE_DURATION_CHANGED:
		_mmcam_dbg_log("GST_MESSAGE_DURATION_CHANGED");
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
	case GST_MESSAGE_QOS:
//		_mmcam_dbg_log("GST_MESSAGE_QOS");
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

	if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR) {
		/* parse error message */
		gst_message_parse_error(message, &err, &debug_info);

		if (debug_info) {
			_mmcam_dbg_err("GST ERROR : %s", debug_info);
			g_free(debug_info);
			debug_info = NULL;
		}

		if (!err) {
			_mmcam_dbg_warn("failed to parse error message");
			return GST_BUS_PASS;
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
			case GST_RESOURCE_ERROR_OPEN_WRITE:
				_mmcam_dbg_err("Camera device [open failed]");
				sc->error_code = MM_ERROR_COMMON_INVALID_PERMISSION;
				//sc->error_code = MM_ERROR_CAMCORDER_DEVICE_OPEN; // SECURITY PART REQUEST PRIVILEGE
				break;
			case GST_RESOURCE_ERROR_OPEN_READ_WRITE:
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
			_MMCamcorderMsgItem msg;

			/* post error to application */
			sc->error_occurs = TRUE;
			msg.id = MM_MESSAGE_CAMCORDER_ERROR;
			msg.param.code = sc->error_code;
			_mmcamcroder_send_message((MMHandleType)hcamcorder, &msg);

			goto DROP_MESSAGE;
		}
	} else if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ELEMENT) {
		_MMCamcorderMsgItem msg;

		if (gst_structure_has_name(gst_message_get_structure(message), "avsysvideosrc-AF") ||
		    gst_structure_has_name(gst_message_get_structure(message), "camerasrc-AF")) {
			int focus_state = 0;

			gst_structure_get_int(gst_message_get_structure(message), "focus-state", &focus_state);
			_mmcam_dbg_log("Focus State:%d", focus_state);

			msg.id = MM_MESSAGE_CAMCORDER_FOCUS_CHANGED;
			msg.param.code = focus_state;
			_mmcamcroder_send_message((MMHandleType)hcamcorder, &msg);

			goto DROP_MESSAGE;
		} else if (gst_structure_has_name(gst_message_get_structure(message), "camerasrc-HDR")) {
			int progress = 0;
			int status = 0;

			if (gst_structure_get_int(gst_message_get_structure(message), "progress", &progress)) {
				gst_structure_get_int(gst_message_get_structure(message), "status", &status);
				_mmcam_dbg_log("HDR progress %d percent, status %d", progress, status);

				msg.id = MM_MESSAGE_CAMCORDER_HDR_PROGRESS;
				msg.param.code = progress;
				_mmcamcroder_send_message((MMHandleType)hcamcorder, &msg);
			}

			goto DROP_MESSAGE;
		} else if (gst_structure_has_name(gst_message_get_structure(message), "camerasrc-FD")) {
			int i = 0;
			const GValue *g_value = gst_structure_get_value(gst_message_get_structure(message), "face-info");;
			GstCameraControlFaceDetectInfo *fd_info = NULL;
			MMCamFaceDetectInfo *cam_fd_info = NULL;

			if (g_value) {
				fd_info = (GstCameraControlFaceDetectInfo *)g_value_get_pointer(g_value);
			}

			if (fd_info == NULL) {
				_mmcam_dbg_warn("fd_info is NULL");
				goto DROP_MESSAGE;
			}

			cam_fd_info = (MMCamFaceDetectInfo *)malloc(sizeof(MMCamFaceDetectInfo));
			if (cam_fd_info == NULL) {
				_mmcam_dbg_warn("cam_fd_info alloc failed");

				free(fd_info);
				fd_info = NULL;

				goto DROP_MESSAGE;
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

			goto DROP_MESSAGE;
		} else if (gst_structure_has_name(gst_message_get_structure(message), "camerasrc-Capture")) {
			int capture_done = FALSE;

			if (gst_structure_get_int(gst_message_get_structure(message), "capture-done", &capture_done)) {
				sc = MMF_CAMCORDER_SUBCONTEXT(hcamcorder);
				if (sc && sc->info_image) {
					/* play capture sound */
					_mmcamcorder_sound_solo_play((MMHandleType)hcamcorder, _MMCAMCORDER_FILEPATH_CAPTURE_SND, FALSE);
				}
			}

			goto DROP_MESSAGE;
		}
	}

	return GST_BUS_PASS;

DROP_MESSAGE:
	gst_message_unref(message);
	message = NULL;

	return GST_BUS_DROP;
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
	case MM_CAMCORDER_MODE_AUDIO:
		ret = _mmcamcorder_create_audio_pipeline(handle);
		if (ret != MM_ERROR_NONE) {
			return ret;
		}
		break;
	case MM_CAMCORDER_MODE_VIDEO_CAPTURE:
	default:
		ret = _mmcamcorder_create_preview_pipeline(handle);
		if (ret != MM_ERROR_NONE) {
			return ret;
		}

		/* connect capture signal */
		if (!sc->bencbin_capture) {
			ret = _mmcamcorder_connect_capture_signal(handle);
			if (ret != MM_ERROR_NONE) {
				return ret;
			}
		}
		break;
	}

	pipeline = sc->element[_MMCAMCORDER_MAIN_PIPE].gst;
	if (type != MM_CAMCORDER_MODE_AUDIO) {
		ret = _mmcamcorder_gst_set_state(handle, pipeline, GST_STATE_READY);
	}
#ifdef _MMCAMCORDER_GET_DEVICE_INFO
	if (!_mmcamcorder_get_device_info(handle)) {
		_mmcam_dbg_err("Getting device information error!!");
	}
#endif

	_mmcam_dbg_log("ret[%x]", ret);
	if (ret != MM_ERROR_NONE) {
		_mmcam_dbg_err("error : destroy pipeline");
		_mmcamcorder_destroy_pipeline(handle, hcamcorder->type);
	}

	return ret;
}


void _mmcamcorder_destroy_pipeline(MMHandleType handle, int type)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;
	gint i = 0;
	int element_num = 0;
	_MMCamcorderGstElement *element = NULL;
	GstBus *bus = NULL;

	mmf_return_if_fail(hcamcorder);

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	mmf_return_if_fail(sc);

	_mmcam_dbg_log("");

	/* Inside each pipeline destroy function, Set GST_STATE_NULL to Main pipeline */
	switch (type) {
	case MM_CAMCORDER_MODE_VIDEO_CAPTURE:
		element = sc->element;
		element_num = sc->element_num;
		bus = gst_pipeline_get_bus(GST_PIPELINE(sc->element[_MMCAMCORDER_MAIN_PIPE].gst));
		_mmcamcorder_destroy_video_capture_pipeline(handle);
		break;
	case MM_CAMCORDER_MODE_AUDIO:
		element = sc->encode_element;
		element_num = sc->encode_element_num;
		bus = gst_pipeline_get_bus(GST_PIPELINE(sc->encode_element[_MMCAMCORDER_ENCODE_MAIN_PIPE].gst));
		_mmcamcorder_destroy_audio_pipeline(handle);
		break;
	default:
		_mmcam_dbg_err("unknown type %d", type);
		break;
	}

	_mmcam_dbg_log("Pipeline clear!!");

	/* Remove pipeline message callback */
	if (hcamcorder->pipeline_cb_event_id > 0) {
		g_source_remove(hcamcorder->pipeline_cb_event_id);
		hcamcorder->pipeline_cb_event_id = 0;
	}

	/* Remove remained message in bus */
	if (bus) {
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
	for (i = 0 ; i < element_num ; i++ ) {
		if (element[i].gst) {
			if (GST_IS_ELEMENT(element[i].gst)) {
				_mmcam_dbg_warn("Still alive element - ID[%d], name [%s], ref count[%d], status[%s]",
				                element[i].id,
				                GST_OBJECT_NAME(element[i].gst),
				                GST_OBJECT_REFCOUNT(element[i].gst),
				                gst_element_state_get_name(GST_STATE(element[i].gst)));
				g_object_weak_unref(G_OBJECT(element[i].gst), (GWeakNotify)_mmcamcorder_element_release_noti, sc);
			} else {
				_mmcam_dbg_warn("The element[%d] is still aliving, check it", element[i].id);
			}

			element[i].id = _MMCAMCORDER_NONE;
			element[i].gst = NULL;
		}
	}

	return;
}


int _mmcamcorder_gst_set_state_async(MMHandleType handle, GstElement *pipeline, GstState target_state)
{
	GstStateChangeReturn setChangeReturn = GST_STATE_CHANGE_FAILURE;

	_MMCAMCORDER_LOCK_GST_STATE(handle);
	setChangeReturn = gst_element_set_state(pipeline, target_state);
	_MMCAMCORDER_UNLOCK_GST_STATE(handle);

	return setChangeReturn;
}


#ifdef _MMCAMCORDER_USE_SET_ATTR_CB
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
#endif /* _MMCAMCORDER_USE_SET_ATTR_CB */


int _mmcamcorder_gst_set_state(MMHandleType handle, GstElement *pipeline, GstState target_state)
{
	unsigned int k = 0;
	_MMCamcorderSubContext *sc = NULL;
	GstState pipeline_state = GST_STATE_VOID_PENDING;
	GstStateChangeReturn setChangeReturn = GST_STATE_CHANGE_FAILURE;
	GstStateChangeReturn getChangeReturn = GST_STATE_CHANGE_FAILURE;
	GstClockTime get_timeout = __MMCAMCORDER_SET_GST_STATE_TIMEOUT * GST_SECOND;
	pthread_mutex_t *state_lock = NULL;

	mmf_return_val_if_fail(handle, MM_ERROR_CAMCORDER_NOT_INITIALIZED);
	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	mmf_return_val_if_fail(sc && sc->element, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	if (sc->element[_MMCAMCORDER_MAIN_PIPE].gst == pipeline) {
		_mmcam_dbg_log("Set state to %d - PREVIEW PIPELINE", target_state);
		state_lock = &_MMCAMCORDER_GET_GST_STATE_LOCK(handle);
	} else {
		_mmcam_dbg_log("Set state to %d - ENDODE PIPELINE", target_state);
		state_lock = &_MMCAMCORDER_GET_GST_ENCODE_STATE_LOCK(handle);
	}

	pthread_mutex_lock(state_lock);

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
					_mmcam_dbg_log("Set state to %d - DONE", target_state);
					pthread_mutex_unlock(state_lock);
					return MM_ERROR_NONE;
				}
				break;
			case GST_STATE_CHANGE_ASYNC:
				_mmcam_dbg_log("status=GST_STATE_CHANGE_ASYNC.");
				break;
			default:
				pthread_mutex_unlock(state_lock);
				_mmcam_dbg_log("status=GST_STATE_CHANGE_FAILURE.");
				return MM_ERROR_CAMCORDER_GST_STATECHANGE;
			}

			pthread_mutex_unlock(state_lock);

			_mmcam_dbg_err("timeout of gst_element_get_state()!!");

			return MM_ERROR_CAMCORDER_RESPONSE_TIMEOUT;
		}
		usleep(_MMCAMCORDER_STATE_CHECK_INTERVAL);
	}

	pthread_mutex_unlock(state_lock);

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
		_mmcam_dbg_warn("This error domain is not defined.");

		/* we treat system error as an internal error */
		msg.param.code = MM_ERROR_CAMCORDER_INTERNAL;
	}

	if (message->src) {
		msg_src_element = GST_ELEMENT_NAME(GST_ELEMENT_CAST(message->src));
		_mmcam_dbg_err("-Msg src : [%s] Domain : [%s]   Error : [%s]  Code : [%d] is tranlated to error code : [0x%x]",
		               msg_src_element, g_quark_to_string (error->domain), error->message, error->code, msg.param.code);
	} else {
		_mmcam_dbg_err("Domain : [%s]   Error : [%s]  Code : [%d] is tranlated to error code : [0x%x]",
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
	element = GST_ELEMENT_CAST(sc->encode_element[_MMCAMCORDER_ENCSINK_VENC].gst);

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
	element = GST_ELEMENT_CAST(sc->encode_element[_MMCAMCORDER_ENCSINK_VENC].gst);
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
	element = GST_ELEMENT_CAST(sc->encode_element[_MMCAMCORDER_ENCSINK_VENC].gst);
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
