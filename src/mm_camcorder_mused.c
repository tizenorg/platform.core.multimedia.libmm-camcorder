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

/*===========================================================================================
|																							|
|  INCLUDE FILES																			|
|  																							|
========================================================================================== */
#include <glib.h>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/video/videooverlay.h>
#ifdef HAVE_WAYLAND
#include <gst/wayland/wayland.h>
#endif
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/time.h>
#include <stdlib.h>

#include <mm_error.h>
#include <mm_debug.h>

#include "mm_camcorder_mused.h"
#include "mm_camcorder_internal.h"
#include <sched.h>

#define MMCAMCORDER_MAX_INT	(2147483647)

#define MMCAMCORDER_FREEIF(x) \
if ( x ) \
	g_free( x ); \
x = NULL;

typedef enum
{
	MM_CAM_MUSED_DISPLAY_SHM_SOCKET_PATH,
	MM_CAM_MUSED_DISPLAY_HANDLE,
	MM_CAM_MUSED_DISPLAY_SURFACE,
	MM_CAM_MUSED_CLIENT_ATTRIBUTE_NUM
}MMCamcorderMusedAttrsID;

MMHandleType
_mmcamcorder_mused_alloc_attribute(MMHandleType handle)
{
	_mmcam_dbg_log( "" );

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	MMHandleType attrs = 0;
	mmf_attrs_construct_info_t *attrs_const_info = NULL;
	unsigned int attr_count = 0;
	unsigned int idx;

	if (hcamcorder == NULL) {
		_mmcam_dbg_err("handle is NULL");
		return 0;
	}

	/* Create attribute constructor */
	_mmcam_dbg_log("start");

	/* alloc 'mmf_attrs_construct_info_t' */
	attr_count = MM_CAM_MUSED_CLIENT_ATTRIBUTE_NUM;
	attrs_const_info = malloc(attr_count * sizeof(mmf_attrs_construct_info_t));
	if (!attrs_const_info) {
		_mmcam_dbg_err("Fail to alloc constructor.");
		return 0;
	}

	/* alloc default attribute info */
	hcamcorder->cam_attrs_const_info = (mm_cam_attr_construct_info *)malloc(sizeof(mm_cam_attr_construct_info) * attr_count);
	if (hcamcorder->cam_attrs_const_info == NULL) {
		_mmcam_dbg_err("failed to alloc default attribute info");
		free(attrs_const_info);
		attrs_const_info = NULL;
		return 0;
	}

	/* basic attributes' info */
	mm_cam_attr_construct_info temp_info[] = {
		{
			MM_CAM_MUSED_DISPLAY_SHM_SOCKET_PATH,
			"mused-display-shm-socket-path",
			MMF_VALUE_TYPE_STRING,
			MM_ATTRS_FLAG_RW,
			{(void*)NULL},
			MM_ATTRS_VALID_TYPE_NONE,
			{0},
			{0},
			NULL,
		},
		{
			MM_CAM_MUSED_DISPLAY_HANDLE,
			"mused-display-handle",
			MMF_VALUE_TYPE_DATA,
			MM_ATTRS_FLAG_RW,
			{(void*)NULL},
			MM_ATTRS_VALID_TYPE_NONE,
			{0},
			{0},
			NULL,
		},
		{
			MM_CAM_MUSED_DISPLAY_SURFACE,
			"mused-display-surface",
			MMF_VALUE_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			{(void*)MM_DISPLAY_SURFACE_X},
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			MM_DISPLAY_SURFACE_X,
			MM_DISPLAY_SURFACE_X_EXT,
			NULL,
		},
	};

	memcpy(hcamcorder->cam_attrs_const_info, temp_info, sizeof(mm_cam_attr_construct_info) * attr_count);

	for (idx = 0 ; idx < attr_count ; idx++) {
		/* attribute order check. This should be same. */
		if (idx != hcamcorder->cam_attrs_const_info[idx].attrid) {
			_mmcam_dbg_err("Please check attributes order. Is the idx same with enum val?");
			free(attrs_const_info);
			attrs_const_info = NULL;
			free(hcamcorder->cam_attrs_const_info);
			hcamcorder->cam_attrs_const_info = NULL;
			return 0;
		}

		attrs_const_info[idx].name = hcamcorder->cam_attrs_const_info[idx].name;
		attrs_const_info[idx].value_type = hcamcorder->cam_attrs_const_info[idx].value_type;
		attrs_const_info[idx].flags = hcamcorder->cam_attrs_const_info[idx].flags;
		attrs_const_info[idx].default_value = hcamcorder->cam_attrs_const_info[idx].default_value.value_void;
	}

	/* Camcorder Attributes */
	_mmcam_dbg_log("Create Camcorder Attributes[%p, %d]", attrs_const_info, attr_count);

	attrs = mmf_attrs_new_from_data("Camcorder_Mused_Attributes",
	                                attrs_const_info,
	                                attr_count,
	                                _mmcamcorder_commit_camcorder_attrs,
	                                (void *)handle);

	free(attrs_const_info);
	attrs_const_info = NULL;

	if (attrs == 0) {
		_mmcam_dbg_err("Fail to alloc attribute handle");
		free(hcamcorder->cam_attrs_const_info);
		hcamcorder->cam_attrs_const_info = NULL;
		return 0;
	}

	for (idx = 0; idx < attr_count; idx++)
	{
		_mmcam_dbg_log("Valid type [%s]", hcamcorder->cam_attrs_const_info[idx].name);

		mmf_attrs_set_valid_type (attrs, idx, hcamcorder->cam_attrs_const_info[idx].validity_type);

		switch (hcamcorder->cam_attrs_const_info[idx].validity_type)
		{
			case MM_ATTRS_VALID_TYPE_INT_ARRAY:
				if (hcamcorder->cam_attrs_const_info[idx].validity_value_1.int_array &&
				    hcamcorder->cam_attrs_const_info[idx].validity_value_2.count > 0) {
					mmf_attrs_set_valid_array(attrs, idx,
					                          (const int *)(hcamcorder->cam_attrs_const_info[idx].validity_value_1.int_array),
					                          hcamcorder->cam_attrs_const_info[idx].validity_value_2.count,
					                          hcamcorder->cam_attrs_const_info[idx].default_value.value_int);
				}
			break;
			case MM_ATTRS_VALID_TYPE_INT_RANGE:
				_mmcam_dbg_err("MM_ATTRS_VALID_TYPE_INT_RANGE");
				mmf_attrs_set_valid_range(attrs, idx,
				                          hcamcorder->cam_attrs_const_info[idx].validity_value_1.int_min,
				                          hcamcorder->cam_attrs_const_info[idx].validity_value_2.int_max,
					                  hcamcorder->cam_attrs_const_info[idx].default_value.value_int);
			break;
			case MM_ATTRS_VALID_TYPE_DOUBLE_ARRAY:
				if (hcamcorder->cam_attrs_const_info[idx].validity_value_1.double_array &&
				    hcamcorder->cam_attrs_const_info[idx].validity_value_2.count > 0) {
					mmf_attrs_set_valid_double_array(attrs, idx,
					                                 (const double *)(hcamcorder->cam_attrs_const_info[idx].validity_value_1.double_array),
					                                 hcamcorder->cam_attrs_const_info[idx].validity_value_2.count,
					                                 hcamcorder->cam_attrs_const_info[idx].default_value.value_double);
				}
			break;
			case MM_ATTRS_VALID_TYPE_DOUBLE_RANGE:
				mmf_attrs_set_valid_double_range(attrs, idx,
				                                 hcamcorder->cam_attrs_const_info[idx].validity_value_1.double_min,
				                                 hcamcorder->cam_attrs_const_info[idx].validity_value_2.double_max,
				                                 hcamcorder->cam_attrs_const_info[idx].default_value.value_double);
			break;
			case MM_ATTRS_VALID_TYPE_NONE:
			break;
			case MM_ATTRS_VALID_TYPE_INVALID:
			default:
				_mmcam_dbg_err("Valid type error.");
			break;
		}
	}

	return attrs;
}

int mm_camcorder_mused_create(MMHandleType *handle)
{
	int ret = MM_ERROR_NONE;
	int UseConfCtrl = 0;
	int rcmd_fmt_capture = MM_PIXEL_FORMAT_YUYV;
	int rcmd_fmt_recording = MM_PIXEL_FORMAT_NV12;
	int rcmd_dpy_rotation = MM_DISPLAY_ROTATION_270;
	int play_capture_sound = TRUE;
	int camera_device_count = MM_VIDEO_DEVICE_NUM;
	int camera_facing_direction = MM_CAMCORDER_CAMERA_FACING_DIRECTION_REAR;
	int resource_fd = -1;
	char *err_attr_name = NULL;
	const char *ConfCtrlFile = NULL;
	mmf_camcorder_t *hcamcorder = NULL;
	type_element *EvasSurfaceElement = NULL;

	_mmcam_dbg_log("Enter");

	mmf_return_val_if_fail(handle, MM_ERROR_CAMCORDER_INVALID_ARGUMENT);

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
	pthread_mutex_init(&((hcamcorder->mtsafe).state_lock), NULL);
	pthread_mutex_init(&((hcamcorder->mtsafe).gst_state_lock), NULL);
	pthread_mutex_init(&((hcamcorder->mtsafe).message_cb_lock), NULL);
	pthread_mutex_init(&(hcamcorder->restart_preview_lock), NULL);

	/* Get Camera Configure information from Camcorder INI file */
	_mmcamcorder_conf_get_info((MMHandleType)hcamcorder, CONFIGURE_TYPE_MAIN, CONFIGURE_MAIN_FILE, &hcamcorder->conf_main);

	if (!(hcamcorder->conf_main)) {
		_mmcam_dbg_err( "Failed to get configure(main) info." );

		ret = MM_ERROR_CAMCORDER_CREATE_CONFIGURE;
		goto _ERR_AFTER_ASM_REGISTER;
	}
	_mmcam_dbg_log("aloc attribute handle : 0x%x", (MMHandleType)hcamcorder);
	hcamcorder->attributes = _mmcamcorder_mused_alloc_attribute((MMHandleType)hcamcorder);
	if (!(hcamcorder->attributes)) {
		_mmcam_dbg_err("_mmcamcorder_create::alloc attribute error.");

		ret = MM_ERROR_CAMCORDER_RESOURCE_CREATION;
		goto _ERR_AFTER_ASM_REGISTER;
	}

	/* Set initial state */
	_mmcamcorder_set_state((MMHandleType)hcamcorder, MM_CAMCORDER_STATE_NULL);

	_mmcam_dbg_log("created handle %p", hcamcorder);

	*handle = (MMHandleType)hcamcorder;
	_mmcam_dbg_log("created client handle : 0x%x", *handle);

	return MM_ERROR_NONE;

_ERR_AFTER_ASM_REGISTER:

_ERR_DEFAULT_VALUE_INIT:
	/* Release lock, cond */
	pthread_mutex_destroy(&((hcamcorder->mtsafe).lock));
	pthread_cond_destroy(&((hcamcorder->mtsafe).cond));
	pthread_mutex_destroy(&((hcamcorder->mtsafe).cmd_lock));
	pthread_mutex_destroy(&((hcamcorder->mtsafe).state_lock));
	pthread_mutex_destroy(&((hcamcorder->mtsafe).gst_state_lock));
	pthread_mutex_destroy(&((hcamcorder->mtsafe).gst_encode_state_lock));
	pthread_mutex_destroy(&((hcamcorder->mtsafe).message_cb_lock));

	pthread_mutex_destroy(&(hcamcorder->restart_preview_lock));

	if (hcamcorder->conf_ctrl) {
		_mmcamcorder_conf_release_info(handle, &hcamcorder->conf_ctrl);
	}

	if (hcamcorder->conf_main) {
		_mmcamcorder_conf_release_info(handle, &hcamcorder->conf_main);
	}

	/* Release handle */
	memset(hcamcorder, 0x00, sizeof(mmf_camcorder_t));
	free(hcamcorder);

	return ret;
}

int _mmcamcorder_mused_videosink_window_set(MMHandleType handle)
{
	int err = MM_ERROR_NONE;
	int *overlay = NULL;
	char *err_name = NULL;
	const char *videosink_name = NULL;
	int size = 0;

	GstElement *vsink = NULL;
	MMCamWaylandInfo *wl_info = NULL;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;

	_mmcam_dbg_log("Enter");

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	mmf_return_val_if_fail(sc, MM_ERROR_CAMCORDER_NOT_INITIALIZED);
	mmf_return_val_if_fail(sc->element, MM_ERROR_CAMCORDER_NOT_INITIALIZED);
	mmf_return_val_if_fail(sc->element[_MMCAMCORDER_CLIENT_VIDEOSINK_SINK].gst, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	vsink = sc->element[_MMCAMCORDER_CLIENT_VIDEOSINK_SINK].gst;

	/* Get video display information */
	err = mm_camcorder_get_attributes(handle, &err_name,
	                                  MMCAM_MUSED_DISPLAY_HANDLE, (void**)&overlay, &size,
	                                  NULL);
	if (err != MM_ERROR_NONE) {
		if (err_name) {
			_mmcam_dbg_err("failed to get attributes [%s][0x%x]", err_name, err);
			free(err_name);
			err_name = NULL;
		} else {
			_mmcam_dbg_err("failed to get attributes [0x%x]", err);
		}

		return err;
	}
	wl_info = (MMCamWaylandInfo *)overlay;
	if (wl_info) {
		GstContext *context = NULL;

		context = gst_wayland_display_handle_context_new((struct wl_display *)wl_info->display);
		if (context) {
			gst_element_set_context(vsink, context);
		} else {
			_mmcam_dbg_warn("gst_wayland_display_handle_context_new failed");
		}

		gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(vsink), (guintptr)wl_info->surface);
		gst_video_overlay_set_render_rectangle(GST_VIDEO_OVERLAY(vsink),
						       wl_info->window_x,
						       wl_info->window_y,
						       wl_info->window_width,
						       wl_info->window_height);

	} else {
		_mmcam_dbg_err("Display Handle is invalid");
	}

	return MM_ERROR_NONE;
}

void __mmcamcorder_mused_gst_destroy_pipeline(MMHandleType handle)
{
	_MMCamcorderSubContext *sc;
	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	
	_MMCAMCORDER_ELEMENT_REMOVE(sc->element, _MMCAMCORDER_CLIENT_VIDEOSRC_SRC);
	_MMCAMCORDER_ELEMENT_REMOVE(sc->element, _MMCAMCORDER_CLIENT_VIDEOSINK_SINK);
	_MMCAMCORDER_ELEMENT_REMOVE(sc->element, _MMCAMCORDER_CLIENT_MAIN_PIPE);
	return;
}

void mm_camcorder_mused_destroy(MMHandleType handle)
{
	int result = MM_ERROR_NONE;
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);

	_MMCAMCORDER_TRYLOCK_CMD(hcamcorder);

	__mmcamcorder_mused_gst_destroy_pipeline(handle);
	_mmcamcorder_dealloc_attribute(handle, hcamcorder->attributes);
	g_free(handle);

	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

	return;
}


int mm_camcorder_mused_realize(MMHandleType handle, char *caps)
{
	int result = MM_ERROR_NONE;

	result = _mmcamcorder_mused_realize(handle, caps);

	return result;
}

int _mmcamcorder_mused_realize(MMHandleType handle, char *string_caps)
{
	int ret = MM_ERROR_NONE;
	GstElement *src;
	GstElement *sink;
	GstBus *bus;
	GstCaps *caps;
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	MMHandleType attrs = hcamcorder->attributes;
	int width = 0;
	int height = 0;
	gboolean link;
	char *socket_path = NULL;
	int socket_path_length;
	int attr_ret;
	int surface_type = 0;
	gchar *videosink_element = NULL;
	gchar *videosrc_element = NULL;

	int state = MM_CAMCORDER_STATE_NONE;
	int state_FROM = MM_CAMCORDER_STATE_NULL;
	int state_TO = MM_CAMCORDER_STATE_READY;
	int vconf_camera_state = 0;
	_MMCamcorderSubContext *sc;
	GList *element_list = NULL;
	GstElement *pipeline = NULL;


	_mmcam_dbg_err("Enter");

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

	/* alloc sub context */
	hcamcorder->sub_context = _mmcamcorder_alloc_subcontext(hcamcorder->type);
	if(!hcamcorder->sub_context) {
		ret = MM_ERROR_CAMCORDER_RESOURCE_CREATION;
		goto _ERR_CAMCORDER_CMD;
	}

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);

	/* create pipeline */
	_MMCAMCORDER_PIPELINE_MAKE(sc, sc->element, _MMCAMCORDER_CLIENT_MAIN_PIPE, "camera_client", ret);

	/* create source */
	_MMCAMCORDER_ELEMENT_MAKE(sc, sc->element, _MMCAMCORDER_CLIENT_VIDEOSRC_SRC, "shmsrc", "shmsrc", element_list, ret);

	mm_camcorder_get_attributes(handle, NULL,
	                            MMCAM_MUSED_DISPLAY_SHM_SOCKET_PATH, &socket_path, &socket_path_length,
	                            NULL);

	g_object_set(sc->element[_MMCAMCORDER_CLIENT_VIDEOSRC_SRC].gst,
			"socket-path", socket_path,
			"is-live", TRUE,
			NULL);

	/* create sink */
	_MMCAMCORDER_ELEMENT_MAKE(sc, sc->element, _MMCAMCORDER_CLIENT_VIDEOSINK_SINK, "waylandsink", "waylandsink", element_list, ret);

	g_object_set(sc->element[_MMCAMCORDER_CLIENT_VIDEOSINK_SINK].gst,
			"draw-borders", 0,
			"force-aspect-ratio", 1,
			"enable-last-sample",0,
			"qos",0,
			"sync",0,
			"show-preroll-frame",0,
			NULL);

	if (_mmcamcorder_mused_videosink_window_set(handle) != MM_ERROR_NONE) {
		_mmcam_dbg_err("_mmcamcorder_videosink_window_set error");
		ret = MM_ERROR_CAMCORDER_INVALID_ARGUMENT;
		goto pipeline_creation_error;
	}

	/* add elements to main pipeline */
	if (!_mmcamcorder_add_elements_to_bin(GST_BIN(sc->element[_MMCAMCORDER_CLIENT_MAIN_PIPE].gst), element_list)) {
		_mmcam_dbg_err("element_list add error.");
		ret = MM_ERROR_CAMCORDER_RESOURCE_CREATION;
		goto pipeline_creation_error;
	}

	caps = gst_caps_from_string(string_caps);
	/* link elements */
	if (!_mmcamcorder_filtered_link_elements(element_list, caps)) {
		_mmcam_dbg_err( "element link error." );
		ret = MM_ERROR_CAMCORDER_GST_LINK;
		goto pipeline_creation_error;
	}

	pipeline = sc->element[_MMCAMCORDER_CLIENT_MAIN_PIPE].gst;

	ret = _mmcamcorder_gst_set_state(handle, pipeline, GST_STATE_READY);
	if (ret != MM_ERROR_NONE) {
		_mmcam_dbg_err("error : destroy pipeline");
		_mmcamcorder_destroy_pipeline(handle, hcamcorder->type);
	}

	ret = _mmcamcorder_gst_set_state(handle, pipeline, GST_STATE_PAUSED);
	if (ret != MM_ERROR_NONE) {
		_mmcam_dbg_err("error : destroy pipeline");
		_mmcamcorder_destroy_pipeline(handle, hcamcorder->type);
	}

	ret = _mmcamcorder_gst_set_state(handle, pipeline, GST_STATE_PLAYING);
	if (ret != MM_ERROR_NONE) {
		_mmcam_dbg_err("error : destroy pipeline");
		_mmcamcorder_destroy_pipeline(handle, hcamcorder->type);
	}

	/* set command function */
	_mmcamcorder_set_state(handle, state_TO);
pipeline_creation_error:
_ERR_CAMCORDER_CMD:
_ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK:
	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);
_ERR_CAMCORDER_CMD_PRECON:
	return ret;
}

int _mmcamcorder_mused_unrealize(MMHandleType handle)
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
		__mmcamcorder_mused_gst_destroy_pipeline(handle);
		/* Deallocate SubContext */
		_mmcamcorder_dealloc_subcontext(hcamcorder->sub_context);
		hcamcorder->sub_context = NULL;
	}

	/* Deinitialize main context member */
	hcamcorder->command = NULL;

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

int mm_camcorder_mused_unrealize(MMHandleType handle)
{
	int ret = MM_ERROR_NONE;

	ret = _mmcamcorder_mused_unrealize(handle);

	return ret;
}

static int _mmcamcorder_get_video_caps(MMHandleType handle, char **caps)
{
	int ret = MM_ERROR_NONE;
	GstPad *pad = NULL;
	GstCaps *sink_caps = NULL;
	
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;

	if (!_MMCAMCORDER_TRYLOCK_CMD(hcamcorder)) {
		_mmcam_dbg_err("Another command is running.");
		return MM_ERROR_CAMCORDER_CMD_IS_RUNNING;
	}

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	_mmcam_dbg_log("Entered ");
	pad = gst_element_get_static_pad(sc->element[_MMCAMCORDER_CLIENT_VIDEOSINK_SINK].gst, "sink");
	if(!pad) {
		_mmcam_dbg_err("static pad is NULL");
		_MMCAMCORDER_UNLOCK_CMD(hcamcorder);
		return MM_ERROR_CAMCORDER_INVALID_STATE;
	}

	sink_caps = gst_pad_get_current_caps(pad);
	gst_object_unref( pad );
	if(!sink_caps) {
		_mmcam_dbg_err("fail to get caps");
		_MMCAMCORDER_UNLOCK_CMD(hcamcorder);
		return MM_ERROR_CAMCORDER_INVALID_STATE;
	}

	*caps = gst_caps_to_string(sink_caps);
	_mmcam_dbg_err("video caps : %s", *caps);
	gst_caps_unref(sink_caps);

	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

	return MM_ERROR_NONE;
}

int mm_camcorder_mused_get_video_caps(MMHandleType handle, char **caps)
{
	return _mmcamcorder_get_video_caps(handle, caps);
}

static int _mmcamcorder_mused_set_shm_socket_path(MMHandleType handle, const char *path)
{
	int ret = MM_ERROR_NONE;
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_mmcam_dbg_log("aloc attribute handle : 0x%x, path:%s", handle, path);	

	_MMCAMCORDER_TRYLOCK_CMD(hcamcorder);

	mm_camcorder_set_attributes(handle, NULL,
	                            MMCAM_MUSED_DISPLAY_SHM_SOCKET_PATH, path, strlen(path),
	                            NULL);

	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

	return ret;
}

/*
 * Server and client both use functions
 */
int mm_camcorder_mused_set_shm_socket_path(MMHandleType handle, const char *path)
{
	int result = MM_ERROR_NONE;

	_mmcam_dbg_log("Entered ");
	result = _mmcamcorder_mused_set_shm_socket_path(handle, path);

	return result;
}
