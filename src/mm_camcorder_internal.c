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
#include <fcntl.h>
#include <gst/gst.h>
#include <gst/gstutils.h>
#include <gst/gstpad.h>
#include <sys/time.h>

#include <mm_error.h>
#include "mm_camcorder_internal.h"
#include <mm_types.h>

#include <gst/video/colorbalance.h>
#include <gst/video/cameracontrol.h>
#include <asm/types.h>

#include <system_info.h>
#include <mm_session.h>
#include <mm_session_private.h>

#include <murphy/common/glib-glue.h>

/*---------------------------------------------------------------------------------------
|    GLOBAL VARIABLE DEFINITIONS for internal						|
---------------------------------------------------------------------------------------*/
struct sigaction mm_camcorder_int_old_action;
struct sigaction mm_camcorder_abrt_old_action;
struct sigaction mm_camcorder_segv_old_action;
struct sigaction mm_camcorder_term_old_action;
struct sigaction mm_camcorder_sys_old_action;

/*---------------------------------------------------------------------------------------
|    LOCAL VARIABLE DEFINITIONS for internal						|
---------------------------------------------------------------------------------------*/
#define __MMCAMCORDER_CMD_ITERATE_MAX           3
#define __MMCAMCORDER_SET_GST_STATE_TIMEOUT     5
#define __MMCAMCORDER_FORCE_STOP_TRY_COUNT      30
#define __MMCAMCORDER_FORCE_STOP_WAIT_TIME      100000  /* us */
#define __MMCAMCORDER_SOUND_WAIT_TIMEOUT        3
#define __MMCAMCORDER_FOCUS_CHANGE_REASON_LEN   64


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

static void __mm_camcorder_signal_handler(int signo);
static void _mmcamcorder_constructor() __attribute__((constructor));

/*=======================================================================================
|  FUNCTION DEFINITIONS									|
=======================================================================================*/
/*---------------------------------------------------------------------------------------
|    GLOBAL FUNCTION DEFINITIONS:							|
---------------------------------------------------------------------------------------*/


static void __mm_camcorder_signal_handler(int signo)
{
	pid_t my_pid = getpid();

	_mmcam_dbg_warn("start - signo [%d], pid [%d]", signo, my_pid);

	/* call old signal handler */
	switch (signo) {
	case SIGINT:
		sigaction(SIGINT, &mm_camcorder_int_old_action, NULL);
		raise(signo);
		break;
	case SIGABRT:
		sigaction(SIGABRT, &mm_camcorder_abrt_old_action, NULL);
		raise(signo);
		break;
	case SIGSEGV:
		sigaction(SIGSEGV, &mm_camcorder_segv_old_action, NULL);
		raise(signo);
		break;
	case SIGTERM:
		sigaction(SIGTERM, &mm_camcorder_term_old_action, NULL);
		raise(signo);
		break;
	case SIGSYS:
		sigaction(SIGSYS, &mm_camcorder_sys_old_action, NULL);
		raise(signo);
		break;
	default:
		break;
	}

	_mmcam_dbg_warn("done");

	return;
}


static void _mmcamcorder_constructor()
{
	struct sigaction mm_camcorder_action;
	mm_camcorder_action.sa_handler = __mm_camcorder_signal_handler;
	mm_camcorder_action.sa_flags = SA_NOCLDSTOP;

	_mmcam_dbg_warn("start");

	sigemptyset(&mm_camcorder_action.sa_mask);

	sigaction(SIGINT, &mm_camcorder_action, &mm_camcorder_int_old_action);
	sigaction(SIGABRT, &mm_camcorder_action, &mm_camcorder_abrt_old_action);
	sigaction(SIGSEGV, &mm_camcorder_action, &mm_camcorder_segv_old_action);
	sigaction(SIGTERM, &mm_camcorder_action, &mm_camcorder_term_old_action);
	sigaction(SIGSYS, &mm_camcorder_action, &mm_camcorder_sys_old_action);

	_mmcam_dbg_warn("done");

	return;
}

/* Internal command functions {*/
int _mmcamcorder_create(MMHandleType *handle, MMCamPreset *info)
{
	int ret = MM_ERROR_NONE;
	int sys_info_ret = SYSTEM_INFO_ERROR_NONE;
	int UseConfCtrl = 0;
	int rcmd_fmt_capture = MM_PIXEL_FORMAT_YUYV;
	int rcmd_fmt_recording = MM_PIXEL_FORMAT_NV12;
	int rcmd_dpy_rotation = MM_DISPLAY_ROTATION_270;
	int play_capture_sound = TRUE;
	int camera_device_count = MM_VIDEO_DEVICE_NUM;
	int camera_default_flip = MM_FLIP_NONE;
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
	hcamcorder->session_type = MM_SESSION_TYPE_MEDIA;

	g_mutex_init(&(hcamcorder->mtsafe).lock);
	g_cond_init(&(hcamcorder->mtsafe).cond);
	g_mutex_init(&(hcamcorder->mtsafe).cmd_lock);
	g_mutex_init(&(hcamcorder->mtsafe).asm_lock);
	g_mutex_init(&(hcamcorder->mtsafe).state_lock);
	g_mutex_init(&(hcamcorder->mtsafe).gst_state_lock);
	g_mutex_init(&(hcamcorder->mtsafe).gst_encode_state_lock);
	g_mutex_init(&(hcamcorder->mtsafe).message_cb_lock);
	g_mutex_init(&(hcamcorder->mtsafe).vcapture_cb_lock);
	g_mutex_init(&(hcamcorder->mtsafe).vstream_cb_lock);
	g_mutex_init(&(hcamcorder->mtsafe).astream_cb_lock);

	g_mutex_init(&hcamcorder->restart_preview_lock);

	/* Sound mutex/cond init */
	g_mutex_init(&hcamcorder->snd_info.open_mutex);
	g_cond_init(&hcamcorder->snd_info.open_cond);
	g_mutex_init(&hcamcorder->snd_info.play_mutex);
	g_cond_init(&hcamcorder->snd_info.play_cond);

	/* init for sound thread */
	g_mutex_init(&hcamcorder->task_thread_lock);
	g_cond_init(&hcamcorder->task_thread_cond);
	hcamcorder->task_thread_state = _MMCAMCORDER_SOUND_STATE_NONE;

	/* create task thread */
	hcamcorder->task_thread = g_thread_try_new("MMCAM_TASK_THREAD",
		(GThreadFunc)_mmcamcorder_util_task_thread_func, (gpointer)hcamcorder, NULL);
	if (hcamcorder->task_thread == NULL) {
		_mmcam_dbg_err("_mmcamcorder_create::failed to create task thread");
		ret = MM_ERROR_CAMCORDER_RESOURCE_CREATION;
		goto _ERR_DEFAULT_VALUE_INIT;
	}

	if (info->videodev_type < MM_VIDEO_DEVICE_NONE ||
	    info->videodev_type >= MM_VIDEO_DEVICE_NUM) {
		_mmcam_dbg_err("_mmcamcorder_create::video device type is out of range.");
		ret = MM_ERROR_CAMCORDER_INVALID_ARGUMENT;
		goto _ERR_DEFAULT_VALUE_INIT;
	}

	/* set device type */
	hcamcorder->device_type = info->videodev_type;
	_mmcam_dbg_warn("Device Type : %d", hcamcorder->device_type);

	if (MM_ERROR_NONE == _mm_session_util_read_information(-1, &hcamcorder->session_type, &hcamcorder->session_flags)) {
		_mmcam_dbg_log("use sound focus function.");
		hcamcorder->sound_focus_register = TRUE;

		if (MM_ERROR_NONE != mm_sound_focus_get_id(&hcamcorder->sound_focus_id)) {
			_mmcam_dbg_err("mm_sound_focus_get_uniq failed");
			ret = MM_ERROR_POLICY_BLOCKED;
			goto _ERR_DEFAULT_VALUE_INIT;
		}

		if (MM_ERROR_NONE != mm_sound_register_focus_for_session(hcamcorder->sound_focus_id,
		                                                         getpid(),
		                                                         "media",
		                                                         _mmcamcorder_sound_focus_cb,
		                                                         hcamcorder)) {
			_mmcam_dbg_err("mm_sound_register_focus failed");
			ret = MM_ERROR_POLICY_BLOCKED;
			goto _ERR_DEFAULT_VALUE_INIT;
		}

		_mmcam_dbg_log("mm_sound_register_focus done - id %d, session type %d, flags 0x%x",
			       hcamcorder->sound_focus_id, hcamcorder->session_type, hcamcorder->session_flags);
	} else {
		_mmcam_dbg_log("_mm_session_util_read_information failed. skip sound focus function.");
		hcamcorder->sound_focus_register = FALSE;
	}

	/* Get Camera Configure information from Camcorder INI file */
	_mmcamcorder_conf_get_info((MMHandleType)hcamcorder, CONFIGURE_TYPE_MAIN, CONFIGURE_MAIN_FILE, &hcamcorder->conf_main);

	if (!(hcamcorder->conf_main)) {
		_mmcam_dbg_err( "Failed to get configure(main) info." );

		ret = MM_ERROR_CAMCORDER_CREATE_CONFIGURE;
		goto _ERR_DEFAULT_VALUE_INIT;
	}

	hcamcorder->attributes = _mmcamcorder_alloc_attribute((MMHandleType)hcamcorder, info);
	if (!(hcamcorder->attributes)) {
		_mmcam_dbg_err("_mmcamcorder_create::alloc attribute error.");

		ret = MM_ERROR_CAMCORDER_RESOURCE_CREATION;
		goto _ERR_DEFAULT_VALUE_INIT;
	}

	if (info->videodev_type != MM_VIDEO_DEVICE_NONE) {
		_mmcamcorder_conf_get_value_int((MMHandleType)hcamcorder, hcamcorder->conf_main,
		                                CONFIGURE_CATEGORY_MAIN_VIDEO_INPUT,
		                                "UseConfCtrl", &UseConfCtrl);

		if (UseConfCtrl) {
			int resolution_width = 0;
			int resolution_height = 0;
			MMCamAttrsInfo fps_info;

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
				goto _ERR_DEFAULT_VALUE_INIT;
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
				goto _ERR_DEFAULT_VALUE_INIT;
			}

			ret = _mmcamcorder_init_convert_table((MMHandleType)hcamcorder);
			if (ret != MM_ERROR_NONE) {
				_mmcam_dbg_warn("converting table initialize error!!");
				ret = MM_ERROR_CAMCORDER_INTERNAL;
				goto _ERR_DEFAULT_VALUE_INIT;
			}

			ret = _mmcamcorder_init_attr_from_configure((MMHandleType)hcamcorder, MM_CAMCONVERT_CATEGORY_ALL);
			if (ret != MM_ERROR_NONE) {
				_mmcam_dbg_warn("converting table initialize error!!");
				ret = MM_ERROR_CAMCORDER_INTERNAL;
				goto _ERR_DEFAULT_VALUE_INIT;
			}

			/* Get device info, recommend preview fmt and display rotation from INI */
			_mmcamcorder_conf_get_value_int((MMHandleType)hcamcorder, hcamcorder->conf_ctrl,
			                                CONFIGURE_CATEGORY_CTRL_CAMERA,
			                                "RecommendPreviewFormatCapture",
			                                &rcmd_fmt_capture);

			_mmcamcorder_conf_get_value_int((MMHandleType)hcamcorder, hcamcorder->conf_ctrl,
			                                CONFIGURE_CATEGORY_CTRL_CAMERA,
			                                "RecommendPreviewFormatRecord",
			                                &rcmd_fmt_recording);

			_mmcamcorder_conf_get_value_int((MMHandleType)hcamcorder, hcamcorder->conf_ctrl,
			                                CONFIGURE_CATEGORY_CTRL_CAMERA,
			                                "RecommendDisplayRotation",
			                                &rcmd_dpy_rotation);

			_mmcamcorder_conf_get_value_int((MMHandleType)hcamcorder, hcamcorder->conf_main,
			                                CONFIGURE_CATEGORY_MAIN_CAPTURE,
			                                "PlayCaptureSound",
			                                &play_capture_sound);

			_mmcamcorder_conf_get_value_int((MMHandleType)hcamcorder, hcamcorder->conf_main,
			                                CONFIGURE_CATEGORY_MAIN_VIDEO_INPUT,
			                                "DeviceCount",
			                                &camera_device_count);

			_mmcamcorder_conf_get_value_int((MMHandleType)hcamcorder, hcamcorder->conf_ctrl,
			                                CONFIGURE_CATEGORY_CTRL_CAMERA,
			                                "FacingDirection",
			                                &camera_facing_direction);

			_mmcamcorder_conf_get_value_int((MMHandleType)hcamcorder, hcamcorder->conf_ctrl,
			                                CONFIGURE_CATEGORY_CTRL_EFFECT,
			                                "BrightnessStepDenominator",
			                                &hcamcorder->brightness_step_denominator);

			_mmcamcorder_conf_get_value_int((MMHandleType)hcamcorder, hcamcorder->conf_ctrl,
			                                CONFIGURE_CATEGORY_CTRL_CAPTURE,
			                                "SupportZSL",
			                                &hcamcorder->support_zsl_capture);

			_mmcam_dbg_log("Recommend fmt[cap:%d,rec:%d], dpy rot %d, cap snd %d, dev cnt %d, cam facing dir %d, step denom %d, support zsl %d",
			               rcmd_fmt_capture, rcmd_fmt_recording, rcmd_dpy_rotation,
			               play_capture_sound, camera_device_count, camera_facing_direction,
			               hcamcorder->brightness_step_denominator, hcamcorder->support_zsl_capture);

			/* Get UseZeroCopyFormat value from INI */
			_mmcamcorder_conf_get_value_int((MMHandleType)hcamcorder, hcamcorder->conf_main,
			                                CONFIGURE_CATEGORY_MAIN_VIDEO_INPUT,
			                                "UseZeroCopyFormat",
			                                &(hcamcorder->use_zero_copy_format));

			/* Get SupportMediaPacketPreviewCb value from INI */
			_mmcamcorder_conf_get_value_int((MMHandleType)hcamcorder, hcamcorder->conf_main,
			                                CONFIGURE_CATEGORY_MAIN_VIDEO_INPUT,
			                                "SupportMediaPacketPreviewCb",
			                                &(hcamcorder->support_media_packet_preview_cb));

			ret = mm_camcorder_get_attributes((MMHandleType)hcamcorder, NULL,
			                            MMCAM_CAMERA_WIDTH, &resolution_width,
			                            MMCAM_CAMERA_HEIGHT, &resolution_height,
			                            NULL);

			mm_camcorder_get_fps_list_by_resolution((MMHandleType)hcamcorder, resolution_width, resolution_height, &fps_info);

			_mmcam_dbg_log("UseZeroCopyFormat : %d", hcamcorder->use_zero_copy_format);
			_mmcam_dbg_log("SupportMediaPacketPreviewCb : %d", hcamcorder->support_media_packet_preview_cb);
			_mmcam_dbg_log("res : %d X %d, Default FPS by resolution  : %d", resolution_width, resolution_height, fps_info.int_array.def);

			if (camera_facing_direction == 1) {
				if (rcmd_dpy_rotation == MM_DISPLAY_ROTATION_270 || rcmd_dpy_rotation == MM_DISPLAY_ROTATION_90) {
					camera_default_flip = MM_FLIP_VERTICAL;
				} else {
					camera_default_flip = MM_FLIP_HORIZONTAL;
				}
				_mmcam_dbg_log("camera_default_flip : [%d]",camera_default_flip);
			}

			mm_camcorder_set_attributes((MMHandleType)hcamcorder, &err_attr_name,
			                            MMCAM_CAMERA_DEVICE_COUNT, camera_device_count,
			                            MMCAM_CAMERA_FACING_DIRECTION, camera_facing_direction,
			                            MMCAM_RECOMMEND_PREVIEW_FORMAT_FOR_CAPTURE, rcmd_fmt_capture,
			                            MMCAM_RECOMMEND_PREVIEW_FORMAT_FOR_RECORDING, rcmd_fmt_recording,
			                            MMCAM_RECOMMEND_DISPLAY_ROTATION, rcmd_dpy_rotation,
			                            MMCAM_SUPPORT_ZSL_CAPTURE, hcamcorder->support_zsl_capture,
			                            MMCAM_SUPPORT_ZERO_COPY_FORMAT, hcamcorder->use_zero_copy_format,
			                            MMCAM_SUPPORT_MEDIA_PACKET_PREVIEW_CB, hcamcorder->support_media_packet_preview_cb,
			                            MMCAM_CAMERA_FPS, fps_info.int_array.def,
			                            MMCAM_DISPLAY_FLIP, camera_default_flip,
			                            "capture-sound-enable", play_capture_sound,
			                            NULL);
			if (err_attr_name) {
				_mmcam_dbg_err("Set %s FAILED.", err_attr_name);
				SAFE_FREE(err_attr_name);
				ret = MM_ERROR_CAMCORDER_INTERNAL;
				goto _ERR_DEFAULT_VALUE_INIT;
			}

			/* Get default value of brightness */
			mm_camcorder_get_attributes((MMHandleType)hcamcorder, &err_attr_name,
			                            MMCAM_FILTER_BRIGHTNESS, &hcamcorder->brightness_default,
			                            NULL);
			if (err_attr_name) {
				_mmcam_dbg_err("Get brightness FAILED.");
				SAFE_FREE(err_attr_name);
				ret = MM_ERROR_CAMCORDER_INTERNAL;
				goto _ERR_DEFAULT_VALUE_INIT;
			}
			_mmcam_dbg_log("Default brightness : %d", hcamcorder->brightness_default);
		} else {
			_mmcam_dbg_log( "Disable Configure Control system." );
			hcamcorder->conf_ctrl = NULL;
		}
	} else {
		_mmcamcorder_conf_get_value_int((MMHandleType)hcamcorder, hcamcorder->conf_main,
			                            CONFIGURE_CATEGORY_MAIN_VIDEO_INPUT,
			                            "DeviceCount",
			                            &camera_device_count);
		mm_camcorder_set_attributes((MMHandleType)hcamcorder, &err_attr_name,
			                            MMCAM_CAMERA_DEVICE_COUNT, camera_device_count,
			                            NULL);
		if (err_attr_name) {
			_mmcam_dbg_err("Set %s FAILED.", err_attr_name);
			SAFE_FREE(err_attr_name);
			ret = MM_ERROR_CAMCORDER_INTERNAL;
			goto _ERR_DEFAULT_VALUE_INIT;
		}

		ret = _mmcamcorder_init_attr_from_configure((MMHandleType)hcamcorder, MM_CAMCONVERT_CATEGORY_AUDIO);
		if (ret != MM_ERROR_NONE) {
			_mmcam_dbg_warn("init attribute from configure error : 0x%x", ret);
			ret = MM_ERROR_CAMCORDER_INTERNAL;
			goto _ERR_DEFAULT_VALUE_INIT;
		}
	}

	/* initialize resource manager */
	ret = _mmcamcorder_resource_manager_init(&hcamcorder->resource_manager, (void *)hcamcorder);
	if (ret != MM_ERROR_NONE) {
		_mmcam_dbg_err("failed to initialize resource manager");
		ret = MM_ERROR_CAMCORDER_INTERNAL;
		goto _ERR_DEFAULT_VALUE_INIT;
	}

	traceBegin(TTRACE_TAG_CAMERA, "MMCAMCORDER:CREATE:INIT_GSTREAMER");

	ret = __mmcamcorder_gstreamer_init(hcamcorder->conf_main);

	traceEnd(TTRACE_TAG_CAMERA);

	if (!ret) {
		_mmcam_dbg_err( "Failed to initialize gstreamer!!" );
		ret = MM_ERROR_CAMCORDER_NOT_INITIALIZED;
		goto _ERR_DEFAULT_VALUE_INIT;
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
	sys_info_ret = system_info_get_platform_string("http://tizen.org/system/model_name", &hcamcorder->model_name);
	if (hcamcorder->model_name) {
		_mmcam_dbg_log("model name [%s], sys_info_ret 0x%x", hcamcorder->model_name, sys_info_ret);
	} else {
		_mmcam_dbg_warn("failed get model name, sys_info_ret 0x%x", sys_info_ret);
	}

	/* get software version */
	sys_info_ret = system_info_get_platform_string("http://tizen.org/system/build.string", &hcamcorder->software_version);
	if (hcamcorder->software_version) {
		_mmcam_dbg_log("software version [%s], sys_info_ret 0x%d", hcamcorder->software_version, sys_info_ret);
	} else {
		_mmcam_dbg_warn("failed get software version, sys_info_ret 0x%x", sys_info_ret);
	}

	/* Set initial state */
	_mmcamcorder_set_state((MMHandleType)hcamcorder, MM_CAMCORDER_STATE_NULL);

	_mmcam_dbg_log("created handle %p", hcamcorder);

	*handle = (MMHandleType)hcamcorder;

	return MM_ERROR_NONE;

_ERR_DEFAULT_VALUE_INIT:
	/* unregister sound focus */
	if (hcamcorder->sound_focus_register && hcamcorder->sound_focus_id > 0) {
		if (MM_ERROR_NONE != mm_sound_unregister_focus(hcamcorder->sound_focus_id)) {
			_mmcam_dbg_err("mm_sound_unregister_focus[id %d] failed", hcamcorder->sound_focus_id);
		} else {
			_mmcam_dbg_log("mm_sound_unregister_focus[id %d] done", hcamcorder->sound_focus_id);
		}
	} else {
		_mmcam_dbg_warn("no need to unregister sound focus[%d, id %d]",
		               hcamcorder->sound_focus_register, hcamcorder->sound_focus_id);
	}

	/* Remove attributes */
	if (hcamcorder->attributes) {
		_mmcamcorder_dealloc_attribute((MMHandleType)hcamcorder, hcamcorder->attributes);
		hcamcorder->attributes = 0;
	}

	/* Release lock, cond */
	g_mutex_clear(&(hcamcorder->mtsafe).lock);
	g_cond_clear(&(hcamcorder->mtsafe).cond);
	g_mutex_clear(&(hcamcorder->mtsafe).cmd_lock);
	g_mutex_clear(&(hcamcorder->mtsafe).asm_lock);
	g_mutex_clear(&(hcamcorder->mtsafe).state_lock);
	g_mutex_clear(&(hcamcorder->mtsafe).gst_state_lock);
	g_mutex_clear(&(hcamcorder->mtsafe).gst_encode_state_lock);
	g_mutex_clear(&(hcamcorder->mtsafe).message_cb_lock);
	g_mutex_clear(&(hcamcorder->mtsafe).vcapture_cb_lock);
	g_mutex_clear(&(hcamcorder->mtsafe).vstream_cb_lock);
	g_mutex_clear(&(hcamcorder->mtsafe).astream_cb_lock);

	g_mutex_clear(&hcamcorder->snd_info.open_mutex);
	g_cond_clear(&hcamcorder->snd_info.open_cond);
	g_mutex_clear(&hcamcorder->restart_preview_lock);

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

	if (hcamcorder->task_thread) {
		g_mutex_lock(&hcamcorder->task_thread_lock);
		_mmcam_dbg_log("send signal for task thread exit");
		hcamcorder->task_thread_state = _MMCAMCORDER_TASK_THREAD_STATE_EXIT;
		g_cond_signal(&hcamcorder->task_thread_cond);
		g_mutex_unlock(&hcamcorder->task_thread_lock);
		g_thread_join(hcamcorder->task_thread);
		hcamcorder->task_thread = NULL;
	}
	g_mutex_clear(&hcamcorder->task_thread_lock);
	g_cond_clear(&hcamcorder->task_thread_cond);

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
	g_mutex_lock(&hcamcorder->task_thread_lock);
	_mmcam_dbg_log("send signal for task thread exit");
	hcamcorder->task_thread_state = _MMCAMCORDER_TASK_THREAD_STATE_EXIT;
	g_cond_signal(&hcamcorder->task_thread_cond);
	g_mutex_unlock(&hcamcorder->task_thread_lock);

	/* wait for completion of sound play */
	_mmcamcorder_sound_solo_play_wait(handle);

	/* Release SubContext and pipeline */
	if (hcamcorder->sub_context) {
		if (hcamcorder->sub_context->element) {
			_mmcamcorder_destroy_pipeline(handle, hcamcorder->type);
		}

		_mmcamcorder_dealloc_subcontext(hcamcorder->sub_context);
		hcamcorder->sub_context = NULL;
	}

	/* de-initialize resource manager */
	ret = _mmcamcorder_resource_manager_deinit(&hcamcorder->resource_manager);
	if (ret != MM_ERROR_NONE) {
		_mmcam_dbg_err("failed to de-initialize resource manager 0x%x", ret);
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
	_mmcamcorder_remove_message_all(handle);

	/* unregister sound focus */
	if (hcamcorder->sound_focus_register && hcamcorder->sound_focus_id > 0) {
		if (mm_sound_unregister_focus(hcamcorder->sound_focus_id) != MM_ERROR_NONE) {
			_mmcam_dbg_err("mm_sound_unregister_focus[id %d] failed",
			               hcamcorder->sound_focus_id);
		} else {
			_mmcam_dbg_log("mm_sound_unregister_focus[id %d] done",
			               hcamcorder->sound_focus_id);
		}
	} else {
		_mmcam_dbg_log("no need to unregister sound focus.[%d, id %d]",
		               hcamcorder->sound_focus_register, hcamcorder->sound_focus_id);
	}

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
	g_thread_join(hcamcorder->task_thread);
	hcamcorder->task_thread = NULL;

	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

	/* Release lock, cond */
	g_mutex_clear(&(hcamcorder->mtsafe).lock);
	g_cond_clear(&(hcamcorder->mtsafe).cond);
	g_mutex_clear(&(hcamcorder->mtsafe).cmd_lock);
	g_mutex_clear(&(hcamcorder->mtsafe).asm_lock);
	g_mutex_clear(&(hcamcorder->mtsafe).state_lock);
	g_mutex_clear(&(hcamcorder->mtsafe).gst_state_lock);
	g_mutex_clear(&(hcamcorder->mtsafe).gst_encode_state_lock);
	g_mutex_clear(&(hcamcorder->mtsafe).message_cb_lock);
	g_mutex_clear(&(hcamcorder->mtsafe).vcapture_cb_lock);
	g_mutex_clear(&(hcamcorder->mtsafe).vstream_cb_lock);
	g_mutex_clear(&(hcamcorder->mtsafe).astream_cb_lock);

	g_mutex_clear(&hcamcorder->snd_info.open_mutex);
	g_cond_clear(&hcamcorder->snd_info.open_cond);
	g_mutex_clear(&hcamcorder->restart_preview_lock);
	g_mutex_clear(&hcamcorder->task_thread_lock);
	g_cond_clear(&hcamcorder->task_thread_cond);

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
	int ret_sound = MM_ERROR_NONE;
	int ret_resource = MM_ERROR_NONE;
	int state = MM_CAMCORDER_STATE_NONE;
	int state_FROM = MM_CAMCORDER_STATE_NULL;
	int state_TO = MM_CAMCORDER_STATE_READY;
	int display_surface_type = MM_DISPLAY_SURFACE_OVERLAY;
	int pid_for_sound_focus = 0;
	double motion_rate = _MMCAMCORDER_DEFAULT_RECORDING_MOTION_RATE;
	char *videosink_element_type = NULL;
	const char *videosink_name = NULL;
	char *socket_path = NULL;
	int socket_path_len;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);

	/*_mmcam_dbg_log("");*/

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

	mm_camcorder_get_attributes(handle, NULL,
		MMCAM_DISPLAY_SURFACE, &display_surface_type,
		MMCAM_CAMERA_RECORDING_MOTION_RATE, &motion_rate,
		NULL);

	/* sound focus */
	if (hcamcorder->sound_focus_register) {
		mm_camcorder_get_attributes(handle, NULL,
		                            MMCAM_PID_FOR_SOUND_FOCUS, &pid_for_sound_focus,
		                            NULL);

		if (pid_for_sound_focus == 0) {
			pid_for_sound_focus = getpid();
			_mmcam_dbg_warn("pid for sound focus is not set, use my pid %d", pid_for_sound_focus);
		}

		/* acquire sound focus or set sound focus watch callback */
		hcamcorder->acquired_focus = 0;
		hcamcorder->sound_focus_watch_id = 0;

		/* check session flags */
		if (hcamcorder->session_flags & MM_SESSION_OPTION_PAUSE_OTHERS) {
			/* acquire sound focus */
			_mmcam_dbg_log("PAUSE_OTHERS - acquire sound focus");

			ret_sound = mm_sound_acquire_focus(0, FOCUS_FOR_BOTH, NULL);
			if (ret_sound != MM_ERROR_NONE) {
				_mmcam_dbg_err("mm_sound_acquire_focus failed [0x%x]", ret_sound);

				/* TODO: MM_ERROR_POLICY_BLOCKED_BY_CALL, MM_ERROR_POLICY_BLOCKED_BY_ALARM*/
				ret = MM_ERROR_POLICY_BLOCKED;
				goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
			}

			hcamcorder->acquired_focus = FOCUS_FOR_BOTH;
		} else if (hcamcorder->session_flags & MM_SESSION_OPTION_UNINTERRUPTIBLE) {
			/* do nothing */
			_mmcam_dbg_log("SESSION_UNINTERRUPTIBLE - do nothing for sound focus");
		} else {
			/* set sound focus watch callback */
			_mmcam_dbg_log("ETC - set sound focus watch callback - pid %d", pid_for_sound_focus);

			ret_sound = mm_sound_set_focus_watch_callback_for_session(pid_for_sound_focus,
			                                                          FOCUS_FOR_BOTH,
			                                                          (mm_sound_focus_changed_watch_cb)_mmcamcorder_sound_focus_watch_cb,
			                                                          hcamcorder,
			                                                          &hcamcorder->sound_focus_watch_id);
			if (ret_sound != MM_ERROR_NONE) {
				_mmcam_dbg_err("mm_sound_set_focus_watch_callback failed [0x%x]", ret_sound);

				/* TODO: MM_ERROR_POLICY_BLOCKED_BY_CALL, MM_ERROR_POLICY_BLOCKED_BY_ALARM*/
				ret = MM_ERROR_POLICY_BLOCKED;
				goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
			}

			_mmcam_dbg_log("sound focus watch cb id %d", hcamcorder->sound_focus_watch_id);
		}
	} else {
		_mmcam_dbg_log("no need to register sound focus");
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
	case MM_DISPLAY_SURFACE_OVERLAY:
		videosink_element_type = strdup("VideosinkElementOverlay");
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
	case MM_DISPLAY_SURFACE_REMOTE:
		mm_camcorder_get_attributes(handle, NULL,
			MMCAM_DISPLAY_SOCKET_PATH, &socket_path, &socket_path_len,
			NULL);
		if (socket_path == NULL) {
			_mmcam_dbg_warn("REMOTE surface, but socket path is NULL -> to NullSink");
			videosink_element_type = strdup("VideosinkElementNull");
		} else
			videosink_element_type = strdup("VideosinkElementRemote");
		break;
	default:
		videosink_element_type = strdup("VideosinkElementOverlay");
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

	if (hcamcorder->type == MM_CAMCORDER_MODE_VIDEO_CAPTURE) {
		/* prepare resource manager for camera */
		if((_mmcamcorder_resource_manager_prepare(&hcamcorder->resource_manager, RESOURCE_TYPE_CAMERA))) {
			_mmcam_dbg_err("could not prepare for camera resource");
			ret = MM_ERROR_CAMCORDER_INTERNAL;
			goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
		}

		/* prepare resource manager for "video_overlay only if display surface is X" */
		mm_camcorder_get_attributes(handle, NULL,
                                            MMCAM_DISPLAY_SURFACE, &display_surface_type,
                                            NULL);
		if(display_surface_type == MM_DISPLAY_SURFACE_OVERLAY) {
			if((_mmcamcorder_resource_manager_prepare(&hcamcorder->resource_manager, RESOURCE_TYPE_VIDEO_OVERLAY))) {
				_mmcam_dbg_err("could not prepare for video overlay resource");
				ret = MM_ERROR_CAMCORDER_INTERNAL;
				goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
			}
		}
		/* acquire resources */
		if((hcamcorder->resource_manager.rset && _mmcamcorder_resource_manager_acquire(&hcamcorder->resource_manager))) {
			_mmcam_dbg_err("could not acquire resources");
			_mmcamcorder_resource_manager_unprepare(&hcamcorder->resource_manager);
			goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
		}
	}

	/* create pipeline */
	traceBegin(TTRACE_TAG_CAMERA, "MMCAMCORDER:REALIZE:CREATE_PIPELINE");

	ret = _mmcamcorder_create_pipeline(handle, hcamcorder->type);

	traceEnd(TTRACE_TAG_CAMERA);

	if (ret != MM_ERROR_NONE) {
		/* check internal error of gstreamer */
		if (hcamcorder->error_code != MM_ERROR_NONE) {
			ret = hcamcorder->error_code;
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

	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

	return MM_ERROR_NONE;

_ERR_CAMCORDER_CMD:
	/* release hw resources */
	if (hcamcorder->type == MM_CAMCORDER_MODE_VIDEO_CAPTURE) {
		ret_resource = _mmcamcorder_resource_manager_release(&hcamcorder->resource_manager);
		if (ret_resource == MM_ERROR_RESOURCE_INVALID_STATE) {
			_mmcam_dbg_warn("it could be in the middle of resource callback or there's no acquired resource");
		}
		else if (ret_resource != MM_ERROR_NONE) {
			_mmcam_dbg_err("failed to release resource, ret_resource(0x%x)", ret_resource);
		}
		ret_resource = _mmcamcorder_resource_manager_unprepare(&hcamcorder->resource_manager);
		if (ret_resource != MM_ERROR_NONE) {
			_mmcam_dbg_err("failed to unprepare resource manager, ret_resource(0x%x)", ret_resource);
		}
	}

_ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK:
	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

	if (hcamcorder->sound_focus_register) {
		if (hcamcorder->sound_focus_watch_id > 0) {
			if (mm_sound_unset_focus_watch_callback(hcamcorder->sound_focus_watch_id) != MM_ERROR_NONE) {
				_mmcam_dbg_warn("mm_sound_unset_focus_watch_callback[id %d] failed",
				                hcamcorder->sound_focus_watch_id);
			} else {
				_mmcam_dbg_warn("mm_sound_unset_focus_watch_callback[id %d] done",
				                hcamcorder->sound_focus_watch_id);
			}
		}

		if (hcamcorder->acquired_focus > 0) {
				if (mm_sound_release_focus(0, hcamcorder->acquired_focus, NULL) != MM_ERROR_NONE) {
					_mmcam_dbg_err("mm_sound_release_focus[focus %d] failed",
					               hcamcorder->acquired_focus);
				} else {
					_mmcam_dbg_err("mm_sound_release_focus[focus %d] done",
					               hcamcorder->acquired_focus);
				}
		}
	}

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

	if (hcamcorder->type == MM_CAMCORDER_MODE_VIDEO_CAPTURE) {
		ret = _mmcamcorder_resource_manager_release(&hcamcorder->resource_manager);
		if (ret == MM_ERROR_RESOURCE_INVALID_STATE) {
			_mmcam_dbg_warn("it could be in the middle of resource callback or there's no acquired resource");
			ret = MM_ERROR_NONE;
		} else if (ret != MM_ERROR_NONE) {
			_mmcam_dbg_err("failed to release resource, ret(0x%x)", ret);
			ret = MM_ERROR_CAMCORDER_INTERNAL;
			goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
		}
		ret = _mmcamcorder_resource_manager_unprepare(&hcamcorder->resource_manager);
		if (ret != MM_ERROR_NONE) {
			_mmcam_dbg_err("failed to unprepare resource manager, ret(0x%x)", ret);
		}
	}

	/* Deinitialize main context member */
	hcamcorder->command = NULL;

	_mmcam_dbg_log("focus register %d, session flag 0x%x, state_change_by_system %d",
	               hcamcorder->sound_focus_register, hcamcorder->session_flags, hcamcorder->state_change_by_system);

	/* release sound focus or unset sound focus watch callback */
	if (hcamcorder->sound_focus_register) {
		int ret_sound = MM_ERROR_NONE;

		_mmcam_dbg_log("state_change_by_system %d, session flag 0x%x, acquired_focus %d, sound_focus_id %d, sound_focus_watch_id %d",
		               hcamcorder->state_change_by_system, hcamcorder->session_flags, hcamcorder->acquired_focus,
		               hcamcorder->sound_focus_id, hcamcorder->sound_focus_watch_id);

		if (hcamcorder->state_change_by_system != _MMCAMCORDER_STATE_CHANGE_BY_ASM &&
		    hcamcorder->sound_focus_watch_id > 0) {
			ret_sound = mm_sound_unset_focus_watch_callback(hcamcorder->sound_focus_watch_id);
			if (ret_sound != MM_ERROR_NONE) {
				_mmcam_dbg_warn("mm_sound_unset_focus_watch_callback failed [0x%x]",
				                ret_sound);
			} else {
				_mmcam_dbg_warn("mm_sound_unset_focus_watch_callback done");
			}
		} else {
			_mmcam_dbg_warn("no need to unset watch callback.[state_change_by_system %d, sound_focus_watch_id %d]",
			                hcamcorder->state_change_by_system, hcamcorder->sound_focus_watch_id);
		}

		if (hcamcorder->acquired_focus > 0) {
			ret_sound = mm_sound_release_focus(0, hcamcorder->acquired_focus, NULL);
			if (ret_sound != MM_ERROR_NONE) {
				_mmcam_dbg_warn("mm_sound_release_focus failed [0x%x]",
				                ret_sound);
			} else {
				_mmcam_dbg_log("mm_sound_release_focus done");
			}
		} else {
			_mmcam_dbg_warn("no need to release focus - current acquired focus %d",
			                hcamcorder->acquired_focus);
		}
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
	hcamcorder->error_code = MM_ERROR_NONE;

	/* set attributes related sensor */
	if (hcamcorder->type != MM_CAMCORDER_MODE_AUDIO) {
		/* init for gdbus */
		hcamcorder->gdbus_conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
		if (hcamcorder->gdbus_conn == NULL) {
			_mmcam_dbg_err("failed to get gdbus");
			ret = MM_ERROR_CAMCORDER_RESOURCE_CREATION;
			goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
		}

		g_mutex_init(&hcamcorder->gdbus_info_sound.sync_mutex);
		g_cond_init(&hcamcorder->gdbus_info_sound.sync_cond);
		g_mutex_init(&hcamcorder->gdbus_info_solo_sound.sync_mutex);
		g_cond_init(&hcamcorder->gdbus_info_solo_sound.sync_cond);

		_mmcamcorder_set_attribute_to_camsensor(handle);
	}

	ret = hcamcorder->command((MMHandleType)hcamcorder, _MMCamcorder_CMD_PREVIEW_START);
	if (ret != MM_ERROR_NONE) {
		/* check internal error of gstreamer */
		if (hcamcorder->error_code != MM_ERROR_NONE) {
			ret = hcamcorder->error_code;
			_mmcam_dbg_log("gstreamer error is occurred. return it %x", ret);
		}
		goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
	}

	_mmcamcorder_set_state(handle, state_TO);

	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

	return MM_ERROR_NONE;

_ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK:
	if (hcamcorder->gdbus_conn) {
		g_object_unref(hcamcorder->gdbus_conn);
		hcamcorder->gdbus_conn = NULL;

		g_mutex_clear(&hcamcorder->gdbus_info_sound.sync_mutex);
		g_cond_clear(&hcamcorder->gdbus_info_sound.sync_cond);
		g_mutex_clear(&hcamcorder->gdbus_info_solo_sound.sync_mutex);
		g_cond_clear(&hcamcorder->gdbus_info_solo_sound.sync_cond);
	}

	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

_ERR_CAMCORDER_CMD_PRECON:
	/* check internal error of gstreamer */
	if (hcamcorder->error_code != MM_ERROR_NONE) {
		ret = hcamcorder->error_code;
		hcamcorder->error_code = MM_ERROR_NONE;

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
		g_mutex_lock(&hcamcorder->gdbus_info_sound.sync_mutex);
		if (hcamcorder->gdbus_info_sound.subscribe_id > 0) {
			_mmcam_dbg_warn("subscribe_id[%u] is remained. remove it.",
				hcamcorder->gdbus_info_sound.subscribe_id);
			g_dbus_connection_signal_unsubscribe(hcamcorder->gdbus_conn,
				hcamcorder->gdbus_info_sound.subscribe_id);
		}
		g_mutex_unlock(&hcamcorder->gdbus_info_sound.sync_mutex);

		g_mutex_lock(&hcamcorder->gdbus_info_solo_sound.sync_mutex);
		if (hcamcorder->gdbus_info_solo_sound.subscribe_id > 0) {
			_mmcam_dbg_warn("subscribe_id[%u] is remained. remove it.",
				hcamcorder->gdbus_info_solo_sound.subscribe_id);
			g_dbus_connection_signal_unsubscribe(hcamcorder->gdbus_conn,
				hcamcorder->gdbus_info_solo_sound.subscribe_id);
		}
		g_mutex_unlock(&hcamcorder->gdbus_info_solo_sound.sync_mutex);

		g_object_unref(hcamcorder->gdbus_conn);
		hcamcorder->gdbus_conn = NULL;

		g_mutex_clear(&hcamcorder->gdbus_info_sound.sync_mutex);
		g_cond_clear(&hcamcorder->gdbus_info_sound.sync_cond);
		g_mutex_clear(&hcamcorder->gdbus_info_solo_sound.sync_mutex);
		g_cond_clear(&hcamcorder->gdbus_info_solo_sound.sync_cond);
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
			g_mutex_lock(&hcamcorder->task_thread_lock);
			if (hcamcorder->task_thread_state == _MMCAMCORDER_TASK_THREAD_STATE_NONE) {
				hcamcorder->capture_in_recording = TRUE;
				hcamcorder->task_thread_state = _MMCAMCORDER_TASK_THREAD_STATE_CHECK_CAPTURE_IN_RECORDING;
				_mmcam_dbg_log("send signal for capture in recording");
				g_cond_signal(&hcamcorder->task_thread_cond);
				g_mutex_unlock(&hcamcorder->task_thread_lock);
			} else {
				_mmcam_dbg_err("task thread busy : %d", hcamcorder->task_thread_state);
				g_mutex_unlock(&hcamcorder->task_thread_lock);
				ret = MM_ERROR_CAMCORDER_INVALID_STATE;
				goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
			}
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
	hcamcorder->error_code = MM_ERROR_NONE;

	ret = hcamcorder->command((MMHandleType)hcamcorder, _MMCamcorder_CMD_RECORD);
	if (ret != MM_ERROR_NONE) {
		/* check internal error of gstreamer */
		if (hcamcorder->error_code != MM_ERROR_NONE) {
			ret = hcamcorder->error_code;
			_mmcam_dbg_log("gstreamer error is occurred. return it %x", ret);
		}
		goto _ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK;
	}

	_mmcamcorder_set_state(handle, state_TO);

	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

	return MM_ERROR_NONE;

_ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK:
	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

_ERR_CAMCORDER_CMD_PRECON:
	/* check internal error of gstreamer */
	if (hcamcorder->error_code != MM_ERROR_NONE) {
		ret = hcamcorder->error_code;
		hcamcorder->error_code = MM_ERROR_NONE;

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

	/*_mmcam_dbg_log("");*/

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
	if (control == NULL) {
		_mmcam_dbg_err("cast CAMERA_CONTROL failed");
		_MMCAMCORDER_UNLOCK_CMD(hcamcorder);
		return MM_ERROR_CAMCORDER_INTERNAL;
	}

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

	/*_mmcam_dbg_log("");*/

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
	if (control == NULL) {
		_mmcam_dbg_err("cast CAMERA_CONTROL failed");
		return MM_ERROR_CAMCORDER_INTERNAL;
	}

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

	/*_mmcam_dbg_log("");*/

	sc = MMF_CAMCORDER_SUBCONTEXT(hcamcorder);

	if (!GST_IS_CAMERA_CONTROL(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst)) {
		_mmcam_dbg_log("Can't cast Video source into camera control.");
		return MM_ERROR_NONE;
	}

	control = GST_CAMERA_CONTROL (sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst);
	if (control) {
		/* Start AF */
		ret = gst_camera_control_start_auto_focus(control);
	} else {
		_mmcam_dbg_err("cast CAMERA_CONTROL failed");
		ret = FALSE;
	}

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

	/*_mmcam_dbg_log("");*/

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
	if (control) {
		ret = gst_camera_control_stop_auto_focus(control);
	} else {
		_mmcam_dbg_err("cast CAMERA_CONTROL failed");
		ret = FALSE;
	}

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
			g_free(argv[i]);
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
	/*_mmcam_dbg_log("state=%d",state);*/

	_MMCAMCORDER_UNLOCK_STATE(handle);

	return state;
}


void _mmcamcorder_set_state(MMHandleType handle, int state)
{
	int old_state;
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderMsgItem msg;

	mmf_return_if_fail(hcamcorder);

	/*_mmcam_dbg_log("");*/

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
			msg.param.state.code = hcamcorder->interrupt_code;
			break;
		case _MMCAMCORDER_STATE_CHANGE_BY_RM:
			msg.id = MM_MESSAGE_CAMCORDER_STATE_CHANGED_BY_RM;
			msg.param.state.code = MM_ERROR_NONE;
			break;
		case _MMCAMCORDER_STATE_CHANGE_NORMAL:
		default:
			msg.id = MM_MESSAGE_CAMCORDER_STATE_CHANGED;
			msg.param.state.code = MM_ERROR_NONE;
			break;
		}

		msg.param.state.previous = old_state;
		msg.param.state.current = state;

		/*_mmcam_dbg_log("_mmcamcorder_send_message : msg : %p, id:%x", &msg, msg.id);*/
		_mmcamcorder_send_message(handle, &msg);
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

	/*_mmcam_dbg_log("");*/

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
		sc->info_audio = g_malloc0( sizeof(_MMCamcorderAudioInfo));
		if(!sc->info_audio) {
			_mmcam_dbg_err("Failed to alloc info structure");
			goto ALLOC_SUBCONTEXT_FAILED;
		}
		break;
	case MM_CAMCORDER_MODE_VIDEO_CAPTURE:
	default:
		sc->info_image = g_malloc0( sizeof(_MMCamcorderImageInfo));
		if(!sc->info_image) {
			_mmcam_dbg_err("Failed to alloc info structure");
			goto ALLOC_SUBCONTEXT_FAILED;
		}

		/* init sound status */
		sc->info_image->sound_status = _SOUND_STATUS_INIT;

		sc->info_video = g_malloc0( sizeof(_MMCamcorderVideoInfo));
		if(!sc->info_video) {
			_mmcam_dbg_err("Failed to alloc info structure");
			goto ALLOC_SUBCONTEXT_FAILED;
		}
		g_mutex_init(&sc->info_video->size_check_lock);
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

	sc->fourcc = 0x80000000;
	sc->cam_stability_count = 0;
	sc->drop_vframe = 0;
	sc->pass_first_vframe = 0;
	sc->is_modified_rate = FALSE;

	return sc;

ALLOC_SUBCONTEXT_FAILED:
	if (sc) {
		SAFE_G_FREE(sc->info_audio);
		SAFE_G_FREE(sc->info_image);
		if (sc->info_video) {
			g_mutex_clear(&sc->info_video->size_check_lock);
		}
		SAFE_G_FREE(sc->info_video);
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
			SAFE_G_FREE(sc->info_video->filename);
			g_mutex_clear(&sc->info_video->size_check_lock);
			free(sc->info_video);
			sc->info_video = NULL;
		}

		if (sc->info_audio) {
			_mmcam_dbg_log("release info_audio");
			SAFE_G_FREE(sc->info_audio->filename);
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

	/*_mmcam_dbg_log("");*/

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
					vnewstate = gst_structure_get_value(gst_message_get_structure(message), "new-state");
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
		/*_mmcam_dbg_log("GST_MESSAGE_NEW_CLOCK : %s", (clock ? GST_OBJECT_NAME (clock) : "NULL"));*/
		break;
	}
	case GST_MESSAGE_STRUCTURE_CHANGE:
		_mmcam_dbg_log("GST_MESSAGE_STRUCTURE_CHANGE");
		break;
	case GST_MESSAGE_STREAM_STATUS:
		/*_mmcam_dbg_log("GST_MESSAGE_STREAM_STATUS");*/
		break;
	case GST_MESSAGE_APPLICATION:
		_mmcam_dbg_log("GST_MESSAGE_APPLICATION");
		break;
	case GST_MESSAGE_ELEMENT:
		/*_mmcam_dbg_log("GST_MESSAGE_ELEMENT");*/
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
		/*_mmcam_dbg_log("GST_MESSAGE_ASYNC_DONE");*/
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
				hcamcorder->error_code = MM_ERROR_CAMCORDER_DEVICE_BUSY;
				break;
			case GST_RESOURCE_ERROR_OPEN_WRITE:
				_mmcam_dbg_err("Camera device [open failed]");
				hcamcorder->error_code = MM_ERROR_COMMON_INVALID_PERMISSION;
				//sc->error_code = MM_ERROR_CAMCORDER_DEVICE_OPEN; // SECURITY PART REQUEST PRIVILEGE
				break;
			case GST_RESOURCE_ERROR_OPEN_READ_WRITE:
				_mmcam_dbg_err("Camera device [open failed]");
				hcamcorder->error_code = MM_ERROR_CAMCORDER_DEVICE_OPEN;
				break;
			case GST_RESOURCE_ERROR_OPEN_READ:
				_mmcam_dbg_err("Camera device [register trouble]");
				hcamcorder->error_code = MM_ERROR_CAMCORDER_DEVICE_REG_TROUBLE;
				break;
			case GST_RESOURCE_ERROR_NOT_FOUND:
				_mmcam_dbg_err("Camera device [device not found]");
				hcamcorder->error_code = MM_ERROR_CAMCORDER_DEVICE_NOT_FOUND;
				break;
			case GST_RESOURCE_ERROR_TOO_LAZY:
				_mmcam_dbg_err("Camera device [timeout]");
				hcamcorder->error_code = MM_ERROR_CAMCORDER_DEVICE_TIMEOUT;
				break;
			case GST_RESOURCE_ERROR_SETTINGS:
				_mmcam_dbg_err("Camera device [not supported]");
				hcamcorder->error_code = MM_ERROR_CAMCORDER_NOT_SUPPORTED;
				break;
			case GST_RESOURCE_ERROR_FAILED:
				_mmcam_dbg_err("Camera device [working failed].");
				hcamcorder->error_code = MM_ERROR_CAMCORDER_DEVICE_IO;
				break;
			default:
				_mmcam_dbg_err("Camera device [General(%d)]", err->code);
				hcamcorder->error_code = MM_ERROR_CAMCORDER_DEVICE;
				break;
			}

			hcamcorder->error_occurs = TRUE;
		}

		g_error_free(err);

		/* store error code and drop this message if cmd is running */
		if (hcamcorder->error_code != MM_ERROR_NONE) {
			_MMCamcorderMsgItem msg;

			/* post error to application */
			hcamcorder->error_occurs = TRUE;
			msg.id = MM_MESSAGE_CAMCORDER_ERROR;
			msg.param.code = hcamcorder->error_code;
			_mmcamcorder_send_message((MMHandleType)hcamcorder, &msg);

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
			_mmcamcorder_send_message((MMHandleType)hcamcorder, &msg);

			goto DROP_MESSAGE;
		} else if (gst_structure_has_name(gst_message_get_structure(message), "camerasrc-HDR")) {
			int progress = 0;
			int status = 0;

			if (gst_structure_get_int(gst_message_get_structure(message), "progress", &progress)) {
				gst_structure_get_int(gst_message_get_structure(message), "status", &status);
				_mmcam_dbg_log("HDR progress %d percent, status %d", progress, status);

				msg.id = MM_MESSAGE_CAMCORDER_HDR_PROGRESS;
				msg.param.code = progress;
				_mmcamcorder_send_message((MMHandleType)hcamcorder, &msg);
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

			cam_fd_info = (MMCamFaceDetectInfo *)g_malloc(sizeof(MMCamFaceDetectInfo));
			if (cam_fd_info == NULL) {
				_mmcam_dbg_warn("cam_fd_info alloc failed");
				SAFE_FREE(fd_info);
				goto DROP_MESSAGE;
			}

			/* set total face count */
			cam_fd_info->num_of_faces = fd_info->num_of_faces;

			if (cam_fd_info->num_of_faces > 0) {
				cam_fd_info->face_info = (MMCamFaceInfo *)g_malloc(sizeof(MMCamFaceInfo) * cam_fd_info->num_of_faces);
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
					SAFE_G_FREE(cam_fd_info);
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

				_mmcamcorder_send_message((MMHandleType)hcamcorder, &msg);
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
					_mmcamcorder_sound_solo_play((MMHandleType)hcamcorder, _MMCAMCORDER_SAMPLE_SOUND_NAME_CAPTURE01, FALSE);
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


GstBusSyncReply _mmcamcorder_audio_pipeline_bus_sync_callback(GstBus *bus, GstMessage *message, gpointer data)
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
		element = GST_ELEMENT_CAST(sc->encode_element[_MMCAMCORDER_AUDIOSRC_SRC].gst);

		/* check domain[RESOURCE] and element[VIDEOSRC] */
		if (err->domain == GST_RESOURCE_ERROR &&
		    GST_ELEMENT_CAST(message->src) == element) {
			_MMCamcorderMsgItem msg;
			switch (err->code) {
			case GST_RESOURCE_ERROR_OPEN_READ_WRITE:
			case GST_RESOURCE_ERROR_OPEN_WRITE:
				_mmcam_dbg_err("audio device [open failed]");
				hcamcorder->error_code = MM_ERROR_COMMON_INVALID_PERMISSION;
				/* post error to application */
				hcamcorder->error_occurs = TRUE;
				msg.id = MM_MESSAGE_CAMCORDER_ERROR;
				msg.param.code = hcamcorder->error_code;
				_mmcam_dbg_err(" error : sc->error_occurs %d", hcamcorder->error_occurs);
				g_error_free(err);
				_mmcamcorder_send_message((MMHandleType)hcamcorder, &msg);
				gst_message_unref(message);
				message = NULL;
				return GST_BUS_DROP;
			default:
				break;
			}
		}

		g_error_free(err);

	}

	return GST_BUS_PASS;
}


void _mmcamcorder_sound_focus_cb(int id, mm_sound_focus_type_e focus_type,
                                 mm_sound_focus_state_e focus_state, const char *reason_for_change,
                                 const char *additional_info, void *user_data)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(user_data);
	int current_state = MM_CAMCORDER_STATE_NONE;

	mmf_return_if_fail((MMHandleType)hcamcorder);

	current_state = _mmcamcorder_get_state((MMHandleType)hcamcorder);
	if (current_state <= MM_CAMCORDER_STATE_NONE ||
	    current_state >= MM_CAMCORDER_STATE_NUM) {
		_mmcam_dbg_err("Abnormal state. Or null handle. (%p, %d)", hcamcorder, current_state);
		return;
	}

	_mmcam_dbg_log("sound focus callback : focus state %d, reason %s",
	               focus_state, reason_for_change ? reason_for_change : "N/A");

	if (hcamcorder->session_flags & MM_SESSION_OPTION_UNINTERRUPTIBLE) {
		_mmcam_dbg_warn("session flag is UNINTERRUPTIBLE. do nothing.");
		return;
	}

	_MMCAMCORDER_LOCK_ASM(hcamcorder);

	/* set value to inform a status is changed by asm */
	hcamcorder->state_change_by_system = _MMCAMCORDER_STATE_CHANGE_BY_ASM;

	/* check the reason */
	if (!strncmp(reason_for_change, "ringtone-voip", __MMCAMCORDER_FOCUS_CHANGE_REASON_LEN) ||
	    !strncmp(reason_for_change, "ringtone-call", __MMCAMCORDER_FOCUS_CHANGE_REASON_LEN) ||
	    !strncmp(reason_for_change, "voip", __MMCAMCORDER_FOCUS_CHANGE_REASON_LEN) ||
	    !strncmp(reason_for_change, "call-voice", __MMCAMCORDER_FOCUS_CHANGE_REASON_LEN)) {
		hcamcorder->interrupt_code = MM_MSG_CODE_INTERRUPTED_BY_CALL_START;
	} else if (!strncmp(reason_for_change, "alarm", __MMCAMCORDER_FOCUS_CHANGE_REASON_LEN)) {
		hcamcorder->interrupt_code = MM_MSG_CODE_INTERRUPTED_BY_ALARM_START;
	} else {
		hcamcorder->interrupt_code = MM_MSG_CODE_INTERRUPTED_BY_MEDIA;
	}

	if (focus_state == FOCUS_IS_RELEASED) {
		hcamcorder->acquired_focus &= ~focus_type;

		_mmcam_dbg_log("FOCUS is released [type %d, remained focus %d] : Stop pipeline[state:%d]",
		               focus_type, hcamcorder->acquired_focus, current_state);

		__mmcamcorder_force_stop(hcamcorder);

		_mmcam_dbg_log("Finish opeartion. Pipeline is released");
	} else if (focus_state == FOCUS_IS_ACQUIRED) {
		_MMCamcorderMsgItem msg;

		hcamcorder->acquired_focus |= focus_type;

		_mmcam_dbg_log("FOCUS is acquired [type %d, new focus %d]",
		               focus_type, hcamcorder->acquired_focus);

		msg.id = MM_MESSAGE_READY_TO_RESUME;
		_mmcamcorder_send_message((MMHandleType)hcamcorder, &msg);

		_mmcam_dbg_log("Finish opeartion");
	} else {
		_mmcam_dbg_log("unknown focus state %d", focus_state);
	}

	/* restore value */
	hcamcorder->state_change_by_system = _MMCAMCORDER_STATE_CHANGE_NORMAL;

	_MMCAMCORDER_UNLOCK_ASM(hcamcorder);

	return;
}


void _mmcamcorder_sound_focus_watch_cb(mm_sound_focus_type_e focus_type, mm_sound_focus_state_e focus_state,
                                       const char *reason_for_change, const char *additional_info, void *user_data)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(user_data);
	int current_state = MM_CAMCORDER_STATE_NONE;

	mmf_return_if_fail((MMHandleType)hcamcorder);

	current_state = _mmcamcorder_get_state((MMHandleType)hcamcorder);
	if (current_state <= MM_CAMCORDER_STATE_NONE ||
	    current_state >= MM_CAMCORDER_STATE_NUM) {
		_mmcam_dbg_err("Abnormal state. Or null handle. (%p, %d)", hcamcorder, current_state);
		return;
	}

	_mmcam_dbg_log("sound focus watch callback : focus state %d, reason %s",
		       focus_state, reason_for_change ? reason_for_change : "N/A");

	if (hcamcorder->session_flags & MM_SESSION_OPTION_UNINTERRUPTIBLE) {
		_mmcam_dbg_warn("session flag is UNINTERRUPTIBLE. do nothing.");
		return;
	}

	_MMCAMCORDER_LOCK_ASM(hcamcorder);

	/* set value to inform a status is changed by asm */
	hcamcorder->state_change_by_system = _MMCAMCORDER_STATE_CHANGE_BY_ASM;

	/* check the reason */
	if (!strncmp(reason_for_change, "ringtone-voip", __MMCAMCORDER_FOCUS_CHANGE_REASON_LEN) ||
	    !strncmp(reason_for_change, "ringtone-call", __MMCAMCORDER_FOCUS_CHANGE_REASON_LEN) ||
	    !strncmp(reason_for_change, "voip", __MMCAMCORDER_FOCUS_CHANGE_REASON_LEN) ||
	    !strncmp(reason_for_change, "call-voice", __MMCAMCORDER_FOCUS_CHANGE_REASON_LEN)) {
		hcamcorder->interrupt_code = MM_MSG_CODE_INTERRUPTED_BY_CALL_START;
	} else if (!strncmp(reason_for_change, "alarm", __MMCAMCORDER_FOCUS_CHANGE_REASON_LEN)) {
		hcamcorder->interrupt_code = MM_MSG_CODE_INTERRUPTED_BY_ALARM_START;
	} else {
		hcamcorder->interrupt_code = MM_MSG_CODE_INTERRUPTED_BY_MEDIA;
	}

	if (focus_state == FOCUS_IS_RELEASED) {
		_MMCamcorderMsgItem msg;

		_mmcam_dbg_log("other process's FOCUS is acquired");

		msg.id = MM_MESSAGE_READY_TO_RESUME;
		_mmcamcorder_send_message((MMHandleType)hcamcorder, &msg);

		_mmcam_dbg_log("Finish opeartion");
	} else if (focus_state == FOCUS_IS_ACQUIRED) {
		_mmcam_dbg_log("other process's FOCUS is released : Stop pipeline[state:%d]", current_state);

		__mmcamcorder_force_stop(hcamcorder);

		_mmcam_dbg_log("Finish opeartion. Pipeline is released");
	} else {
		_mmcam_dbg_log("unknown focus state %d", focus_state);
	}

	/* restore value */
	hcamcorder->state_change_by_system = _MMCAMCORDER_STATE_CHANGE_NORMAL;

	_MMCAMCORDER_UNLOCK_ASM(hcamcorder);

	return;
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
		traceBegin(TTRACE_TAG_CAMERA, "MMCAMCORDER:REALIZE:SET_READY_TO_PIPELINE");

		ret = _mmcamcorder_gst_set_state(handle, pipeline, GST_STATE_READY);

		traceEnd(TTRACE_TAG_CAMERA);
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
	GMutex *state_lock = NULL;

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

	g_mutex_lock(state_lock);

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
					g_mutex_unlock(state_lock);
					return MM_ERROR_NONE;
				}
				break;
			case GST_STATE_CHANGE_ASYNC:
				_mmcam_dbg_log("status=GST_STATE_CHANGE_ASYNC.");
				break;
			default:
				g_mutex_unlock(state_lock);
				_mmcam_dbg_log("status=GST_STATE_CHANGE_FAILURE.");
				return MM_ERROR_CAMCORDER_GST_STATECHANGE;
			}

			g_mutex_unlock(state_lock);

			_mmcam_dbg_err("timeout of gst_element_get_state()!!");

			return MM_ERROR_CAMCORDER_RESPONSE_TIMEOUT;
		}

		usleep(_MMCAMCORDER_STATE_CHECK_INTERVAL);
	}

	g_mutex_unlock(state_lock);

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


void __mmcamcorder_force_stop(mmf_camcorder_t *hcamcorder)
{
	int i = 0;
	int loop = 0;
	int itr_cnt = 0;
	int result = MM_ERROR_NONE;
	int cmd_try_count = 0;
	int current_state = MM_CAMCORDER_STATE_NONE;

	mmf_return_if_fail(hcamcorder);

	/* check command running */
	do {
		if (!_MMCAMCORDER_TRYLOCK_CMD(hcamcorder)) {
			if (cmd_try_count++ < __MMCAMCORDER_FORCE_STOP_TRY_COUNT) {
				_mmcam_dbg_warn("Another command is running. try again after %d ms", __MMCAMCORDER_FORCE_STOP_WAIT_TIME/1000);
				usleep(__MMCAMCORDER_FORCE_STOP_WAIT_TIME);
			} else {
				_mmcam_dbg_err("wait timeout");
				break;
			}
		} else {
			_MMCAMCORDER_UNLOCK_CMD(hcamcorder);
			break;
		}
	} while (TRUE);

	current_state = _mmcamcorder_get_state((MMHandleType)hcamcorder);

	_mmcam_dbg_warn("Force STOP MMFW Camcorder");

	for (loop = 0 ; current_state > MM_CAMCORDER_STATE_NULL && loop < __MMCAMCORDER_CMD_ITERATE_MAX * 3 ; loop++) {
		itr_cnt = __MMCAMCORDER_CMD_ITERATE_MAX;
		switch (current_state) {
		case MM_CAMCORDER_STATE_CAPTURING:
		{
			_MMCamcorderSubContext *sc = MMF_CAMCORDER_SUBCONTEXT(hcamcorder);
			_MMCamcorderImageInfo *info = NULL;

			mmf_return_if_fail(sc);
			mmf_return_if_fail((info = sc->info_image));

			_mmcam_dbg_warn("Stop capturing.");

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
			_mmcam_dbg_warn("Stop recording.");

			while ((itr_cnt--) && ((result = _mmcamcorder_commit((MMHandleType)hcamcorder)) != MM_ERROR_NONE)) {
				_mmcam_dbg_warn("Can't commit.(%x)", result);
			}
			break;
		}
		case MM_CAMCORDER_STATE_PREPARE:
		{
			_mmcam_dbg_warn("Stop preview.");

			while ((itr_cnt--) && ((result = _mmcamcorder_stop((MMHandleType)hcamcorder)) != MM_ERROR_NONE)) {
				_mmcam_dbg_warn("Can't stop preview.(%x)", result);
			}
			break;
		}
		case MM_CAMCORDER_STATE_READY:
		{
			_mmcam_dbg_warn("unrealize");

			if ((result = _mmcamcorder_unrealize((MMHandleType)hcamcorder)) != MM_ERROR_NONE) {
				_mmcam_dbg_warn("Can't unrealize.(%x)", result);
			}
			break;
		}
		case MM_CAMCORDER_STATE_NULL:
		default:
			_mmcam_dbg_warn("Already stopped.");
			break;
		}

		current_state = _mmcamcorder_get_state((MMHandleType)hcamcorder);
	}

	_mmcam_dbg_warn( "Done." );

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
	hcamcorder->error_occurs = TRUE;
	msg.id = MM_MESSAGE_CAMCORDER_ERROR;
	_mmcamcorder_send_message(handle, &msg);

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

int _mmcamcorder_get_video_caps(MMHandleType handle, char **caps)
{
	GstPad *pad = NULL;
	GstCaps *sink_caps = NULL;
	_MMCamcorderSubContext *sc = NULL;

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	_mmcam_dbg_warn("Entered ");
	pad = gst_element_get_static_pad(sc->element[_MMCAMCORDER_VIDEOSINK_SINK].gst, "sink");
	if(!pad) {
		_mmcam_dbg_err("static pad is NULL");
		return MM_ERROR_CAMCORDER_INVALID_STATE;
	}

	sink_caps = gst_pad_get_current_caps(pad);
	gst_object_unref( pad );
	if(!sink_caps) {
		_mmcam_dbg_err("fail to get caps");
		return MM_ERROR_CAMCORDER_INVALID_STATE;
	}

	*caps = gst_caps_to_string(sink_caps);
	_mmcam_dbg_err("video caps : %s", *caps);
	gst_caps_unref(sink_caps);

	return MM_ERROR_NONE;
}
