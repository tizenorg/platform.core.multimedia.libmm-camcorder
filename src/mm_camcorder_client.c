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

#include "mm_camcorder_client.h"
#include "mm_camcorder_internal.h"
#include <sched.h>

#include <storage.h>


static _MMCamcorderInfoConverting g_client_display_info[] = {
	{
		CONFIGURE_TYPE_MAIN,
		CONFIGURE_CATEGORY_MAIN_VIDEO_OUTPUT,
		MM_CAM_CLIENT_DISPLAY_DEVICE,
		MM_CAMCORDER_ATTR_NONE,
		"DisplayDevice",
		MM_CAMCONVERT_TYPE_INT_ARRAY,
		NULL,
	},
	{
		CONFIGURE_TYPE_MAIN,
		CONFIGURE_CATEGORY_MAIN_VIDEO_OUTPUT,
		MM_CAM_CLIENT_DISPLAY_MODE,
		MM_CAMCORDER_ATTR_NONE,
		"DisplayMode",
		MM_CAMCONVERT_TYPE_INT_ARRAY,
		NULL,
	},
	{
		CONFIGURE_TYPE_MAIN,
		CONFIGURE_CATEGORY_MAIN_VIDEO_OUTPUT,
		MM_CAM_CLIENT_DISPLAY_SURFACE,
		MM_CAMCORDER_ATTR_NONE,
		"Videosink",
		MM_CAMCONVERT_TYPE_INT_ARRAY,
		NULL,
	},
};


static int _storage_device_supported_cb(int storage_id, storage_type_e type, storage_state_e state, const char *path, void *user_data)
{
	char **root_directory = (char **)user_data;

	if (root_directory == NULL) {
		_mmcam_dbg_warn("user data is NULL");
		return FALSE;
	}

	_mmcam_dbg_log("storage id %d, type %d, state %d, path %s",
	               storage_id, type, state, path ? path : "NULL");

	if (type == STORAGE_TYPE_INTERNAL && path) {
		if (*root_directory) {
			free(*root_directory);
			*root_directory = NULL;
		}

		*root_directory = strdup(path);
		if (*root_directory) {
			_mmcam_dbg_log("get root directory %s", *root_directory);
			return FALSE;
		} else {
			_mmcam_dbg_warn("strdup %s failed");
		}
	}

	return TRUE;
}

bool _mmcamcorder_client_commit_display_handle(MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	int current_state = MM_CAMCORDER_STATE_NONE;
	const char *videosink_name = NULL;
	void *p_handle = NULL;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;

	mmf_return_val_if_fail(handle, FALSE);

	/* check type */
	if (hcamcorder->type == MM_CAMCORDER_MODE_AUDIO) {
		_mmcam_dbg_err("invalid mode %d", hcamcorder->type);
		return FALSE;
	}

	/* check current state */
	current_state = _mmcamcorder_get_state(handle);
	if (current_state < MM_CAMCORDER_STATE_READY) {
		_mmcam_dbg_log("NOT initialized. this will be applied later");
		return TRUE;
	}

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);

	p_handle = value->value.p_val;
	if (p_handle) {
		/* get videosink name */
		_mmcamcorder_conf_get_value_element_name(sc->VideosinkElement, &videosink_name);
		if (videosink_name == NULL) {
			_mmcam_dbg_err("Please check videosink element in configuration file");
			return FALSE;
		}

		_mmcam_dbg_log("Commit : videosinkname[%s]", videosink_name);

		if (!strcmp(videosink_name, "xvimagesink") ||
		    !strcmp(videosink_name, "ximagesink")) {
			_mmcam_dbg_log("Commit : Set XID[%x]", *(int*)(p_handle));
			gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(sc->element[_MMCAMCORDER_CLIENT_VIDEOSINK_SINK].gst), *(int*)(p_handle));
		} else if (!strcmp(videosink_name, "evasimagesink") ||
			   !strcmp(videosink_name, "evaspixmapsink")) {
			_mmcam_dbg_log("Commit : Set evas object [%p]", p_handle);
			MMCAMCORDER_G_OBJECT_SET_POINTER(sc->element[_MMCAMCORDER_CLIENT_VIDEOSINK_SINK].gst, "evas-object", p_handle);
#ifdef HAVE_WAYLAND
		} else if (!strcmp(videosink_name, "waylandsink")) {
			MMCamWaylandInfo *wl_info = (MMCamWaylandInfo *)p_handle;
			GstContext *context = NULL;

			context = gst_wayland_display_handle_context_new((struct wl_display *)wl_info->display);
			if (context) {
				gst_element_set_context(sc->element[_MMCAMCORDER_CLIENT_VIDEOSINK_SINK].gst, context);
			} else {
				_mmcam_dbg_warn("gst_wayland_display_handle_context_new failed");
			}

			gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(sc->element[_MMCAMCORDER_CLIENT_VIDEOSINK_SINK].gst), (guintptr)wl_info->surface);
			gst_video_overlay_set_render_rectangle(GST_VIDEO_OVERLAY(sc->element[_MMCAMCORDER_CLIENT_VIDEOSINK_SINK].gst),
							       wl_info->window_x,
							       wl_info->window_y,
							       wl_info->window_width,
							       wl_info->window_height);
#endif /* HAVE_WAYLAND */
		} else {
			_mmcam_dbg_warn("Commit : Nothing to commit with this element[%s]", videosink_name);
			return FALSE;
		}
	} else {
		_mmcam_dbg_warn("Display handle is NULL");
		return FALSE;
	}

	return TRUE;
}

bool _mmcamcorder_client_commit_display_rect(MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	int current_state = MM_CAMCORDER_STATE_NONE;
	int method = 0;
	const char *videosink_name = NULL;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;

	mmf_return_val_if_fail(handle, FALSE);

	/* check type */
	if (hcamcorder->type == MM_CAMCORDER_MODE_AUDIO) {
		_mmcam_dbg_err("invalid mode %d", hcamcorder->type);
		return FALSE;
	}

	/* check current state */
	current_state = _mmcamcorder_get_state(handle);
	if (current_state < MM_CAMCORDER_STATE_READY) {
		_mmcam_dbg_log("NOT initialized. this will be applied later");
		return TRUE;
	}

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);

	/* check current method */
	mm_camcorder_get_attributes(handle, NULL,
	                            MMCAM_DISPLAY_GEOMETRY_METHOD, &method,
	                            NULL);
	if (method != MM_DISPLAY_METHOD_CUSTOM_ROI) {
		_mmcam_dbg_log("current method[%d] is not supported rect", method);
		return FALSE;
	}

	/* Get videosink name */
	_mmcamcorder_conf_get_value_element_name(sc->VideosinkElement, &videosink_name);
	if (videosink_name == NULL) {
		_mmcam_dbg_err("Please check videosink element in configuration file");
		return FALSE;
	}

	if (!strcmp(videosink_name, "xvimagesink") ||
	    !strcmp(videosink_name, "evaspixmapsink")) {
		int rect_x = 0;
		int rect_y = 0;
		int rect_width = 0;
		int rect_height = 0;
		int flags = MM_ATTRS_FLAG_NONE;
		MMCamAttrsInfo info;

		mm_camcorder_get_attributes(handle, NULL,
		                            MMCAM_DISPLAY_RECT_X, &rect_x,
		                            MMCAM_DISPLAY_RECT_Y, &rect_y,
		                            MMCAM_DISPLAY_RECT_WIDTH, &rect_width,
		                            MMCAM_DISPLAY_RECT_HEIGHT, &rect_height,
		                            NULL);
		switch (attr_idx) {
		case MM_CAM_CLIENT_DISPLAY_RECT_X:
			mm_camcorder_get_attribute_info(handle, MMCAM_DISPLAY_RECT_Y, &info);
			flags |= info.flag;
			memset(&info, 0x00, sizeof(info));
			mm_camcorder_get_attribute_info(handle, MMCAM_DISPLAY_RECT_WIDTH, &info);
			flags |= info.flag;
			memset(&info, 0x00, sizeof(info));
			mm_camcorder_get_attribute_info(handle, MMCAM_DISPLAY_RECT_HEIGHT, &info);
			flags |= info.flag;
			rect_x = value->value.i_val;
			break;
		case MM_CAM_CLIENT_DISPLAY_RECT_Y:
			mm_camcorder_get_attribute_info(handle, MMCAM_DISPLAY_RECT_WIDTH, &info);
			flags |= info.flag;
			memset(&info, 0x00, sizeof(info));
			mm_camcorder_get_attribute_info(handle, MMCAM_DISPLAY_RECT_HEIGHT, &info);
			flags |= info.flag;
			rect_y = value->value.i_val;
			break;
		case MM_CAM_CLIENT_DISPLAY_RECT_WIDTH:
			mm_camcorder_get_attribute_info(handle, MMCAM_DISPLAY_RECT_HEIGHT, &info);
			flags |= info.flag;
			rect_width = value->value.i_val;
			break;
		case MM_CAM_CLIENT_DISPLAY_RECT_HEIGHT:
			rect_height = value->value.i_val;
			break;
		default:
			_mmcam_dbg_err("Wrong attr_idx!");
			return FALSE;
		}

		if (!(flags & MM_ATTRS_FLAG_MODIFIED)) {
			_mmcam_dbg_log("RECT(x,y,w,h) = (%d,%d,%d,%d)",
			               rect_x, rect_y, rect_width, rect_height);
			g_object_set(sc->element[_MMCAMCORDER_CLIENT_VIDEOSINK_SINK].gst,
			             "dst-roi-x", rect_x,
			             "dst-roi-y", rect_y,
			             "dst-roi-w", rect_width,
			             "dst-roi-h", rect_height,
			             NULL);
		}

		return TRUE;
	} else {
		_mmcam_dbg_warn("videosink[%s] does not support display rect.", videosink_name);
		return FALSE;
	}
}

bool _mmcamcorder_client_commit_display_rotation(MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	int current_state = MM_CAMCORDER_STATE_NONE;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);

	mmf_return_val_if_fail(handle, FALSE);

	/* check type */
	if (hcamcorder->type == MM_CAMCORDER_MODE_AUDIO) {
		_mmcam_dbg_err("invalid mode %d", hcamcorder->type);
		return FALSE;
	}

	/* check current state */
	current_state = _mmcamcorder_get_state(handle);
	if (current_state < MM_CAMCORDER_STATE_READY) {
		_mmcam_dbg_log("NOT initialized. this will be applied later [rotate:%d]", value->value.i_val);
		return TRUE;
	}

	return _mmcamcorder_set_display_rotation(handle, value->value.i_val, _MMCAMCORDER_CLIENT_VIDEOSINK_SINK);
}

bool _mmcamcorder_client_commit_display_visible(MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	int current_state = MM_CAMCORDER_STATE_NONE;
	const char *videosink_name = NULL;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;

	mmf_return_val_if_fail(handle, FALSE);

	/* check type */
	if (hcamcorder->type == MM_CAMCORDER_MODE_AUDIO) {
		_mmcam_dbg_err("invalid mode %d", hcamcorder->type);
		return FALSE;
	}

	/* check current state */
	current_state = _mmcamcorder_get_state(handle);
	if (current_state < MM_CAMCORDER_STATE_READY) {
		_mmcam_dbg_log("NOT initialized. this will be applied later");
		return TRUE;
	}

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);

	/* Get videosink name */
	_mmcamcorder_conf_get_value_element_name(sc->VideosinkElement, &videosink_name);
	if (videosink_name == NULL) {
		_mmcam_dbg_err("Please check videosink element in configuration file");
		return FALSE;
	}

	if (!strcmp(videosink_name, "waylandsink") || !strcmp(videosink_name, "xvimagesink") ||
		!strcmp(videosink_name, "evasimagesink") || !strcmp(videosink_name, "evaspixmapsink")) {
		MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_CLIENT_VIDEOSINK_SINK].gst, "visible", value->value.i_val);
		_mmcam_dbg_log("Set visible [%d] done.", value->value.i_val);
		return TRUE;
	} else {
		_mmcam_dbg_warn("videosink[%s] does not support VISIBLE.", videosink_name);
		return FALSE;
	}
}

bool _mmcamcorder_client_commit_display_geometry_method (MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	int method = 0;
	int current_state = MM_CAMCORDER_STATE_NONE;
	const char *videosink_name = NULL;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;

	mmf_return_val_if_fail(handle, FALSE);

	/* check type */
	if (hcamcorder->type == MM_CAMCORDER_MODE_AUDIO) {
		_mmcam_dbg_err("invalid mode %d", hcamcorder->type);
		return FALSE;
	}

	/* check current state */
	current_state = _mmcamcorder_get_state(handle);
	if (current_state < MM_CAMCORDER_STATE_READY) {
		_mmcam_dbg_log("NOT initialized. this will be applied later");
		return TRUE;
	}

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);

	/* Get videosink name */
	_mmcamcorder_conf_get_value_element_name(sc->VideosinkElement, &videosink_name);
	if (videosink_name == NULL) {
		_mmcam_dbg_err("Please check videosink element in configuration file");
		return FALSE;
	}

	if (!strcmp(videosink_name, "waylandsink") || !strcmp(videosink_name, "xvimagesink") ||
		!strcmp(videosink_name, "evasimagesink") || !strcmp(videosink_name, "evaspixmapsink")) {
		method = value->value.i_val;
		MMCAMCORDER_G_OBJECT_SET( sc->element[_MMCAMCORDER_CLIENT_VIDEOSINK_SINK].gst, "display-geometry-method", method);
		return TRUE;
	} else {
		_mmcam_dbg_warn("videosink[%s] does not support geometry method.", videosink_name);
		return FALSE;
	}
}

bool _mmcamcorder_client_commit_display_scale(MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	int zoom = 0;
	int current_state = MM_CAMCORDER_STATE_NONE;
	const char *videosink_name = NULL;
	GstElement *vs_element = NULL;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;

	mmf_return_val_if_fail(handle, FALSE);

	/* check type */
	if (hcamcorder->type == MM_CAMCORDER_MODE_AUDIO) {
		_mmcam_dbg_err("invalid mode %d", hcamcorder->type);
		return FALSE;
	}

	/* check current state */
	current_state = _mmcamcorder_get_state(handle);
	if (current_state < MM_CAMCORDER_STATE_READY) {
		_mmcam_dbg_log("NOT initialized. this will be applied later");
		return TRUE;
	}

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);

	/* Get videosink name */
	_mmcamcorder_conf_get_value_element_name(sc->VideosinkElement, &videosink_name);
	if (videosink_name == NULL) {
		_mmcam_dbg_err("Please check videosink element in configuration file");
		return FALSE;
	}

	zoom = value->value.i_val;
	if (!strcmp(videosink_name, "waylandsink") || !strcmp(videosink_name, "xvimagesink")) {
		vs_element = sc->element[_MMCAMCORDER_CLIENT_VIDEOSINK_SINK].gst;

		MMCAMCORDER_G_OBJECT_SET(vs_element, "zoom", (float)(zoom + 1));
		_mmcam_dbg_log("Set display zoom to %d", zoom + 1);

		return TRUE;
	} else {
		_mmcam_dbg_warn("videosink[%s] does not support scale", videosink_name);
		return FALSE;
	}
}

bool _mmcamcorder_client_commit_display_mode(MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	int current_state = MM_CAMCORDER_STATE_NONE;
	const char *videosink_name = NULL;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;

	mmf_return_val_if_fail(handle, FALSE);

	/* check type */
	if (hcamcorder->type == MM_CAMCORDER_MODE_AUDIO) {
		_mmcam_dbg_err("invalid mode %d", hcamcorder->type);
		return FALSE;
	}

	/* check current state */
	current_state = _mmcamcorder_get_state(handle);
	if (current_state < MM_CAMCORDER_STATE_READY) {
		_mmcam_dbg_log("NOT initialized. this will be applied later");
		return TRUE;
	}

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);

	_mmcamcorder_conf_get_value_element_name(sc->VideosinkElement, &videosink_name);
	if (videosink_name == NULL) {
		_mmcam_dbg_err("Please check videosink element in configuration file");
		return FALSE;
	}

	_mmcam_dbg_log("Commit : videosinkname[%s]", videosink_name);

	if (!strcmp(videosink_name, "waylandsink") || !strcmp(videosink_name, "xvimagesink")) {
		_mmcam_dbg_log("Commit : display mode [%d]", value->value.i_val);
		MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_CLIENT_VIDEOSINK_SINK].gst, "display-mode", value->value.i_val);
		return TRUE;
	} else {
		_mmcam_dbg_warn("Commit : This element [%s] does not support display mode", videosink_name);
		return FALSE;
	}
}

bool _mmcamcorder_client_commit_display_evas_do_scaling(MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	int current_state = MM_CAMCORDER_STATE_NONE;
	int do_scaling = 0;
	const char *videosink_name = NULL;

	mmf_camcorder_t *hcamcorder= MMF_CAMCORDER( handle);
	_MMCamcorderSubContext *sc = NULL;

	mmf_return_val_if_fail(handle, FALSE);

	/* check type */
	if (hcamcorder->type == MM_CAMCORDER_MODE_AUDIO) {
		_mmcam_dbg_err("invalid mode %d", hcamcorder->type);
		return FALSE;
	}

	/* check current state */
	current_state = _mmcamcorder_get_state(handle);
	if (current_state < MM_CAMCORDER_STATE_READY) {
		_mmcam_dbg_log("NOT initialized. this will be applied later");
		return TRUE;
	}

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);

	do_scaling = value->value.i_val;

	/* Get videosink name */
	_mmcamcorder_conf_get_value_element_name(sc->VideosinkElement, &videosink_name);
	if (videosink_name == NULL) {
		_mmcam_dbg_err("Please check videosink element in configuration file");
		return FALSE;
	}

	if (!strcmp(videosink_name, "evaspixmapsink")) {
		MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_CLIENT_VIDEOSINK_SINK].gst, "origin-size", !do_scaling);
		_mmcam_dbg_log("Set origin-size to %d", !(value->value.i_val));
		return TRUE;
	} else {
		_mmcam_dbg_warn("videosink[%s] does not support scale", videosink_name);
		return FALSE;
	}
}

bool _mmcamcorder_client_commit_display_flip(MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	int current_state = MM_CAMCORDER_STATE_NONE;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);

	mmf_return_val_if_fail(handle, FALSE);

	/* check type */
	if (hcamcorder->type == MM_CAMCORDER_MODE_AUDIO) {
		_mmcam_dbg_err("invalid mode %d", hcamcorder->type);
		return FALSE;
	}

	/* check current state */
	current_state = _mmcamcorder_get_state(handle);
	if (current_state < MM_CAMCORDER_STATE_READY) {
		_mmcam_dbg_log("NOT initialized. this will be applied later [flip:%d]", value->value.i_val);
		return TRUE;
	}

	return _mmcamcorder_set_display_flip(handle, value->value.i_val, _MMCAMCORDER_CLIENT_VIDEOSINK_SINK);
}

int _mmcamcorder_client_videosink_window_set(MMHandleType handle, type_element* VideosinkElement)
{
	int err = MM_ERROR_NONE;
	int size = 0;
	int retx = 0;
	int rety = 0;
	int retwidth = 0;
	int retheight = 0;
	int visible = 0;
	int rotation = MM_DISPLAY_ROTATION_NONE;
	int flip = MM_FLIP_NONE;
	int display_mode = MM_DISPLAY_MODE_DEFAULT;
	int display_geometry_method = MM_DISPLAY_METHOD_LETTER_BOX;
	int origin_size = 0;
	int zoom_attr = 0;
	int zoom_level = 0;
	int do_scaling = FALSE;
	int *overlay = NULL;
	gulong xid;
	char *err_name = NULL;
	const char *videosink_name = NULL;

	GstElement *vsink = NULL;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);
	mmf_return_val_if_fail(VideosinkElement, MM_ERROR_CAMCORDER_INVALID_ARGUMENT);

	_mmcam_dbg_log("");

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	mmf_return_val_if_fail(sc, MM_ERROR_CAMCORDER_NOT_INITIALIZED);
	mmf_return_val_if_fail(sc->element, MM_ERROR_CAMCORDER_NOT_INITIALIZED);
	mmf_return_val_if_fail(sc->element[_MMCAMCORDER_CLIENT_VIDEOSINK_SINK].gst, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	vsink = sc->element[_MMCAMCORDER_CLIENT_VIDEOSINK_SINK].gst;

	/* Get video display information */
	err = mm_camcorder_get_attributes(handle, &err_name,
	                                  MMCAM_DISPLAY_RECT_X, &retx,
	                                  MMCAM_DISPLAY_RECT_Y, &rety,
	                                  MMCAM_DISPLAY_RECT_WIDTH, &retwidth,
	                                  MMCAM_DISPLAY_RECT_HEIGHT, &retheight,
	                                  MMCAM_DISPLAY_ROTATION, &rotation,
	                                  MMCAM_DISPLAY_FLIP, &flip,
	                                  MMCAM_DISPLAY_VISIBLE, &visible,
	                                  MMCAM_DISPLAY_HANDLE, (void**)&overlay, &size,
	                                  MMCAM_DISPLAY_MODE, &display_mode,
	                                  MMCAM_DISPLAY_GEOMETRY_METHOD, &display_geometry_method,
	                                  MMCAM_DISPLAY_SCALE, &zoom_attr,
	                                  MMCAM_DISPLAY_EVAS_DO_SCALING, &do_scaling,
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

	_mmcam_dbg_log("(overlay=%p, size=%d)", overlay, size);

	_mmcamcorder_conf_get_value_element_name(sc->VideosinkElement, &videosink_name);

	if (videosink_name == NULL) {
		_mmcam_dbg_err("videosink_name is empty");
		return MM_ERROR_CAMCORDER_INVALID_CONDITION;
	}


	/* Set display handle */
	if (!strcmp(videosink_name, "xvimagesink") ||
	    !strcmp(videosink_name, "ximagesink")) {
		if (overlay) {
			xid = *overlay;
			_mmcam_dbg_log("xid = %lu )", xid);
			gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(vsink), xid);
		} else {
			_mmcam_dbg_warn("Handle is NULL. Set xid as 0.. but, it's not recommended.");
			gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(vsink), 0);
		}
	} else if (!strcmp(videosink_name, "evasimagesink") ||
	           !strcmp(videosink_name, "evaspixmapsink")) {
		_mmcam_dbg_log("videosink : %s, handle : %p", videosink_name, overlay);
		if (overlay) {
			MMCAMCORDER_G_OBJECT_SET_POINTER(vsink, "evas-object", overlay);
			MMCAMCORDER_G_OBJECT_SET(vsink, "origin-size", !do_scaling);
		} else {
			_mmcam_dbg_err("display handle(eavs object) is NULL");
			return MM_ERROR_CAMCORDER_INVALID_ARGUMENT;
		}
#ifdef HAVE_WAYLAND
	} else if (!strcmp(videosink_name, "waylandsink")) {
		MMCamWaylandInfo *wl_info = (MMCamWaylandInfo *)overlay;
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
			_mmcam_dbg_warn("Handle is NULL. skip setting.");
		}
#endif /* HAVE_WAYLAND */
	} else {
		_mmcam_dbg_warn("Who are you?? (Videosink: %s)", videosink_name);
	}

	_mmcam_dbg_log("%s set: display_geometry_method[%d],origin-size[%d],visible[%d],rotate[%d],flip[%d]",
	               videosink_name, display_geometry_method, origin_size, visible, rotation, flip);

	/* Set attribute */
	if (!strcmp(videosink_name, "waylandsink") || !strcmp(videosink_name, "xvimagesink") ||
		!strcmp(videosink_name, "evaspixmapsink")) {
		/* set rotation */
		MMCAMCORDER_G_OBJECT_SET(vsink, "rotate", rotation);

		/* set flip */
		MMCAMCORDER_G_OBJECT_SET(vsink, "flip", flip);

		switch (zoom_attr) {
		case MM_DISPLAY_SCALE_DEFAULT:
			zoom_level = 1;
			break;
		case MM_DISPLAY_SCALE_DOUBLE_LENGTH:
			zoom_level = 2;
			break;
		case MM_DISPLAY_SCALE_TRIPLE_LENGTH:
			zoom_level = 3;
			break;
		default:
			_mmcam_dbg_warn("Unsupported zoom value. set as default.");
			zoom_level = 1;
			break;
		}

		MMCAMCORDER_G_OBJECT_SET(vsink, "display-geometry-method", display_geometry_method);
		MMCAMCORDER_G_OBJECT_SET(vsink, "display-mode", display_mode);
		MMCAMCORDER_G_OBJECT_SET(vsink, "visible", visible);
		MMCAMCORDER_G_OBJECT_SET(vsink, "zoom", zoom_level);

		if (display_geometry_method == MM_DISPLAY_METHOD_CUSTOM_ROI) {
			g_object_set(vsink,
			             "dst-roi-x", retx,
			             "dst-roi-y", rety,
			             "dst-roi-w", retwidth,
			             "dst-roi-h", retheight,
			             NULL);
		}
	} else {
		_mmcam_dbg_warn("unsupported videosink [%s]", videosink_name);
	}

	return MM_ERROR_NONE;
}

int _mmcamcorder_client_init_attr_from_configure(MMHandleType handle)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderInfoConverting *info = NULL;

	int table_size = 0;
	int ret = MM_ERROR_NONE;

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	/* Initialize attribute related to display */
	info = g_client_display_info;
	table_size = sizeof(g_client_display_info) / sizeof(_MMCamcorderInfoConverting);
	ret = __mmcamcorder_set_info_to_attr(handle, info, table_size);
	if (ret != MM_ERROR_NONE) {
		_mmcam_dbg_err("display info set error : 0x%x", ret);
		return ret;
	}

	_mmcam_dbg_log("done");

	return ret;
}

MMHandleType _mmcamcorder_client_alloc_attribute(MMHandleType handle)
{
	_mmcam_dbg_log( "" );

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	MMHandleType attrs = 0;
	mmf_attrs_construct_info_t *attrs_const_info = NULL;
	unsigned int attr_count = 0;
	unsigned int idx;
	static int visible_values[] = { 0, 1 };	/*0: off, 1:on*/

	mmf_return_val_if_fail(hcamcorder, NULL);

	/* Create attribute constructor */
	_mmcam_dbg_log("start");

	/* alloc 'mmf_attrs_construct_info_t' */
	attr_count = MM_CAM_CLIENT_ATTRIBUTE_NUM;
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
			MM_CAM_CLIENT_DISPLAY_SOCKET_PATH,
			MMCAM_DISPLAY_SOCKET_PATH,
			MMF_VALUE_TYPE_STRING,
			MM_ATTRS_FLAG_RW,
			{(void*)NULL},
			MM_ATTRS_VALID_TYPE_NONE,
			{0},
			{0},
			NULL,
		},
		{
			MM_CAM_CLIENT_DISPLAY_HANDLE,
			"display-handle",
			MMF_VALUE_TYPE_DATA,
			MM_ATTRS_FLAG_RW,
			{(void*)NULL},
			MM_ATTRS_VALID_TYPE_NONE,
			{0},
			{0},
			_mmcamcorder_client_commit_display_handle,
		},
		{
			MM_CAM_CLIENT_DISPLAY_DEVICE,
			"display-device",
			MMF_VALUE_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			{(void*)MM_DISPLAY_DEVICE_MAINLCD},
			MM_ATTRS_VALID_TYPE_INT_ARRAY,
			{0},
			{0},
			NULL,
		},
		// 50
		{
			MM_CAM_CLIENT_DISPLAY_SURFACE,
			"display-surface",
			MMF_VALUE_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			{(void*)MM_DISPLAY_SURFACE_X},
			MM_ATTRS_VALID_TYPE_INT_ARRAY,
			{0},
			{0},
			NULL,
		},
		{
			MM_CAM_CLIENT_DISPLAY_RECT_X,
			"display-rect-x",
			MMF_VALUE_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			{(void*)0},
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			{.int_min = 0},
			{.int_max = _MMCAMCORDER_MAX_INT},
			_mmcamcorder_client_commit_display_rect,
		},
		{
			MM_CAM_CLIENT_DISPLAY_RECT_Y,
			"display-rect-y",
			MMF_VALUE_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			{(void*)0},
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			{.int_min = 0},
			{.int_max = _MMCAMCORDER_MAX_INT},
			_mmcamcorder_client_commit_display_rect,
		},
		{
			MM_CAM_CLIENT_DISPLAY_RECT_WIDTH,
			"display-rect-width",
			MMF_VALUE_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			{(void*)0},
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			{.int_min = 0},
			{.int_max = _MMCAMCORDER_MAX_INT},
			_mmcamcorder_client_commit_display_rect,
		},
		{
			MM_CAM_CLIENT_DISPLAY_RECT_HEIGHT,
			"display-rect-height",
			MMF_VALUE_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			{(void*)0},
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			{.int_min = 0},
			{.int_max = _MMCAMCORDER_MAX_INT},
			_mmcamcorder_client_commit_display_rect,
		},
		{
			MM_CAM_CLIENT_DISPLAY_SOURCE_X,
			"display-src-x",
			MMF_VALUE_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			{(void*)0},
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			{.int_min = 0},
			{.int_max = _MMCAMCORDER_MAX_INT},
			NULL,
		},
		{
			MM_CAM_CLIENT_DISPLAY_SOURCE_Y,
			"display-src-y",
			MMF_VALUE_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			{(void*)0},
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			{.int_min = 0},
			{.int_max = _MMCAMCORDER_MAX_INT},
			NULL,
		},
		{
			MM_CAM_CLIENT_DISPLAY_SOURCE_WIDTH,
			"display-src-width",
			MMF_VALUE_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			{(void*)0},
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			{.int_min = 0},
			{.int_max = _MMCAMCORDER_MAX_INT},
			NULL,
		},
		{
			MM_CAM_CLIENT_DISPLAY_SOURCE_HEIGHT,
			"display-src-height",
			MMF_VALUE_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			{(void*)0},
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			{.int_min = 0},
			{.int_max = _MMCAMCORDER_MAX_INT},
			NULL,
		},
		{
			MM_CAM_CLIENT_DISPLAY_ROTATION,
			"display-rotation",
			MMF_VALUE_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			{(void*)MM_DISPLAY_ROTATION_NONE},
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			{.int_min = MM_DISPLAY_ROTATION_NONE},
			{.int_max = MM_DISPLAY_ROTATION_270},
			_mmcamcorder_client_commit_display_rotation,
		},
		{ // 60
			MM_CAM_CLIENT_DISPLAY_VISIBLE,
			"display-visible",
			MMF_VALUE_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			{(void*)1},
			MM_ATTRS_VALID_TYPE_INT_ARRAY,
			{visible_values},
			{ARRAY_SIZE(visible_values)},
			_mmcamcorder_client_commit_display_visible,
		},
		{
			MM_CAM_CLIENT_DISPLAY_SCALE,
			"display-scale",
			MMF_VALUE_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			{(void*)0},
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			{.int_min = MM_DISPLAY_SCALE_DEFAULT},
			{.int_max = MM_DISPLAY_SCALE_TRIPLE_LENGTH},
			_mmcamcorder_client_commit_display_scale,
		},
		{
			MM_CAM_CLIENT_DISPLAY_GEOMETRY_METHOD,
			"display-geometry-method",
			MMF_VALUE_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			{(void*)0},
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			{.int_min = MM_DISPLAY_METHOD_LETTER_BOX},
			{.int_max = MM_DISPLAY_METHOD_CUSTOM_ROI},
			_mmcamcorder_client_commit_display_geometry_method,
		},
		{
			MM_CAM_CLIENT_DISPLAY_MODE,
			"display-mode",
			MMF_VALUE_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			{(void*)MM_DISPLAY_MODE_DEFAULT},
			MM_ATTRS_VALID_TYPE_INT_ARRAY,
			{0},
			{0},
			_mmcamcorder_client_commit_display_mode,
		},
		{
			MM_CAM_CLIENT_DISPLAY_EVAS_SURFACE_SINK,
			"display-evas-surface-sink",
			MMF_VALUE_TYPE_STRING,
			MM_ATTRS_FLAG_READABLE,
			{(void*)NULL},
			MM_ATTRS_VALID_TYPE_NONE,
			{0},
			{0},
			NULL,
		},
		{
			MM_CAM_CLIENT_DISPLAY_EVAS_DO_SCALING,
			"display-evas-do-scaling",
			MMF_VALUE_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			{(void*)TRUE},
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			{.int_min = FALSE},
			{.int_max = TRUE},
			_mmcamcorder_client_commit_display_evas_do_scaling,
		},
		{
			MM_CAM_CLIENT_DISPLAY_FLIP,
			"display-flip",
			MMF_VALUE_TYPE_INT,
			MM_ATTRS_FLAG_RW,
			{(void*)MM_FLIP_NONE},
			MM_ATTRS_VALID_TYPE_INT_RANGE,
			{.int_min = MM_FLIP_NONE},
			{.int_max = MM_FLIP_BOTH},
			_mmcamcorder_client_commit_display_flip,
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

static gboolean
__mmcamcorder_client_gstreamer_init(camera_conf * conf)
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

	free(argv);
	argv = NULL;

	free(argc);
	argc = NULL;

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

int _mmcamcorder_client_create_preview_elements(MMHandleType handle, const char *videosink_name, char *string_caps)
{
	int ret = MM_ERROR_NONE;
	GList *element_list = NULL;
	GstElement *pipeline = NULL;
	char *socket_path = NULL;
	int socket_path_length;
	GstCaps *caps;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);
	mmf_return_val_if_fail(hcamcorder->sub_context, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	_MMCamcorderSubContext *sc = MMF_CAMCORDER_SUBCONTEXT(handle);

	/* create pipeline */
	_MMCAMCORDER_PIPELINE_MAKE(sc, sc->element, _MMCAMCORDER_CLIENT_MAIN_PIPE, "camera_client", ret);

	/* create source */
	if (hcamcorder->use_zero_copy_format) {
		_MMCAMCORDER_ELEMENT_MAKE(sc, sc->element, _MMCAMCORDER_CLIENT_VIDEOSRC_SRC, "tizenipcsrc", "client_videosrc_src", element_list, ret);
	} else {
		_MMCAMCORDER_ELEMENT_MAKE(sc, sc->element, _MMCAMCORDER_CLIENT_VIDEOSRC_SRC, "shmsrc", "client_videosrc_src", element_list, ret);
	}

	mm_camcorder_get_attributes(handle, NULL,
	                            MMCAM_DISPLAY_SOCKET_PATH, &socket_path, &socket_path_length,
	                            NULL);

	if (socket_path == NULL ) {
		_mmcam_dbg_err("socket path is not set");
		goto set_videosrc_error;
	}

	_mmcam_dbg_err("ipc src socket path : %s", socket_path);

	g_object_set(sc->element[_MMCAMCORDER_CLIENT_VIDEOSRC_SRC].gst,
	             "socket-path", socket_path,
	             "is-live", TRUE,
	             NULL);

	/* create capsfilter */
	_MMCAMCORDER_ELEMENT_MAKE(sc, sc->element, _MMCAMCORDER_CLIENT_VIDEOSRC_FILT, "capsfilter", "client_vidoesrc_filt", element_list, ret);

	caps = gst_caps_from_string(string_caps);
	MMCAMCORDER_G_OBJECT_SET_POINTER(sc->element[_MMCAMCORDER_CLIENT_VIDEOSRC_FILT].gst, "caps", caps);

	gst_caps_unref(caps);

	caps = NULL;

	/* Making Video sink from here */
	_MMCAMCORDER_ELEMENT_MAKE(sc, sc->element, _MMCAMCORDER_CLIENT_VIDEOSINK_QUE, "queue", "client_videosink_queue", element_list, ret);

	/* Add color converting element */
	if (!strcmp(videosink_name, "evasimagesink") || !strcmp(videosink_name, "ximagesink")) {
		_MMCAMCORDER_ELEMENT_MAKE(sc, sc->element, _MMCAMCORDER_CLIENT_VIDEOSINK_CLS, "videoconvert", "client_videosrc_convert", element_list, ret);
	}

	/* create sink */
	_MMCAMCORDER_ELEMENT_MAKE(sc, sc->element, _MMCAMCORDER_CLIENT_VIDEOSINK_SINK, videosink_name, "client_videosink_sink", element_list, ret);

	if (strcmp(videosink_name, "fakesink") && strcmp(videosink_name, "tizenipcsink") &&
		strcmp(videosink_name, "shmsink")) {
		if (_mmcamcorder_client_videosink_window_set(handle, sc->VideosinkElement) != MM_ERROR_NONE) {
			_mmcam_dbg_err("_mmcamcorder_client_videosink_window_set error");
			ret = MM_ERROR_CAMCORDER_INVALID_ARGUMENT;
			goto pipeline_creation_error;
		}
	}

	_mmcamcorder_conf_set_value_element_property(sc->element[_MMCAMCORDER_CLIENT_VIDEOSINK_SINK].gst, sc->VideosinkElement);

	/* add elements to main pipeline */
	if (!_mmcamcorder_add_elements_to_bin(GST_BIN(sc->element[_MMCAMCORDER_CLIENT_MAIN_PIPE].gst), element_list)) {
		_mmcam_dbg_err("element_list add error.");
		ret = MM_ERROR_CAMCORDER_RESOURCE_CREATION;
		goto pipeline_creation_error;
	}

	/* link elements */
	if (!_mmcamcorder_link_elements(element_list)) {
		_mmcam_dbg_err( "element link error." );
		ret = MM_ERROR_CAMCORDER_GST_LINK;
		goto pipeline_creation_error;
	}

	pipeline = sc->element[_MMCAMCORDER_CLIENT_MAIN_PIPE].gst;

	ret = _mmcamcorder_gst_set_state(handle, pipeline, GST_STATE_PLAYING);
	if (ret != MM_ERROR_NONE) {
		_mmcam_dbg_err("error : destroy pipeline");
		goto pipeline_creation_error;
	}

	if (element_list) {
		g_list_free(element_list);
		element_list = NULL;
	}

	return MM_ERROR_NONE;

pipeline_creation_error:
	_MMCAMCORDER_ELEMENT_REMOVE(sc->element, _MMCAMCORDER_CLIENT_VIDEOSINK_SINK);
	if (!strcmp(videosink_name, "evasimagesink") || !strcmp(videosink_name, "ximagesink")) {
		_MMCAMCORDER_ELEMENT_REMOVE(sc->element, _MMCAMCORDER_CLIENT_VIDEOSINK_CLS);
	}
	_MMCAMCORDER_ELEMENT_REMOVE(sc->element, _MMCAMCORDER_CLIENT_VIDEOSINK_QUE);
set_videosrc_error:
	_MMCAMCORDER_ELEMENT_REMOVE(sc->element, _MMCAMCORDER_CLIENT_VIDEOSRC_SRC);
	_MMCAMCORDER_ELEMENT_REMOVE(sc->element, _MMCAMCORDER_CLIENT_MAIN_PIPE);

	if (element_list) {
		g_list_free(element_list);
		element_list = NULL;
	}

	return ret;
}

void __mmcamcorder_client_gst_destroy_pipeline(MMHandleType handle)
{
	_MMCamcorderSubContext *sc;
	sc = MMF_CAMCORDER_SUBCONTEXT(handle);

	if (sc->element[_MMCAMCORDER_CLIENT_MAIN_PIPE].gst) {
		MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_CLIENT_VIDEOSINK_QUE].gst, "empty-buffers", TRUE);

		_mmcamcorder_gst_set_state(handle, sc->element[_MMCAMCORDER_CLIENT_MAIN_PIPE].gst, GST_STATE_NULL);
		_mmcamcorder_remove_all_handlers(handle, _MMCAMCORDER_HANDLER_CATEGORY_ALL);

		gst_object_unref(sc->element[_MMCAMCORDER_CLIENT_MAIN_PIPE].gst);
	}

	return;
}

int _mmcamcorder_client_realize(MMHandleType handle, char *string_caps)
{
	int ret = MM_ERROR_NONE;
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);

	_MMCamcorderSubContext *sc;
	int display_surface_type = MM_DISPLAY_SURFACE_X;
	char *videosink_element_type = NULL;
	const char *videosink_name = NULL;
	char *err_name = NULL;

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);
	_mmcam_dbg_log("Enter");

	if (!_MMCAMCORDER_TRYLOCK_CMD(hcamcorder)) {
		_mmcam_dbg_err("Another command is running.");
		ret = MM_ERROR_CAMCORDER_CMD_IS_RUNNING;
		goto _ERR_CAMCORDER_CMD_PRECON;
	}

	/* alloc sub context */
	hcamcorder->sub_context = _mmcamcorder_alloc_subcontext(hcamcorder->type);
	if(!hcamcorder->sub_context) {
		ret = MM_ERROR_CAMCORDER_RESOURCE_CREATION;
		goto _ERR_CAMCORDER_CMD;
	}

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);

	ret = mm_camcorder_get_attributes(handle, &err_name,
	                            MMCAM_DISPLAY_SURFACE, &display_surface_type,
	                            NULL);

	if (ret != MM_ERROR_NONE) {
		_mmcam_dbg_warn("Get attrs fail. (%s:%x)", err_name, ret);
		SAFE_G_FREE(err_name);
		goto _ERR_CAMCORDER_SET_DISPLAY;
	}

	_mmcam_dbg_warn("display_surface_type : %d", display_surface_type);

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
		                              &sc->VideosinkElement);
		free(videosink_element_type);
		videosink_element_type = NULL;
	} else {
		_mmcam_dbg_warn("strdup failed(display_surface_type %d). Use default X type",
		                display_surface_type);

		_mmcamcorder_conf_get_element(handle, hcamcorder->conf_main,
		                              CONFIGURE_CATEGORY_MAIN_VIDEO_OUTPUT,
		                              _MMCAMCORDER_DEFAULT_VIDEOSINK_TYPE,
		                              &sc->VideosinkElement);
	}

	_mmcamcorder_conf_get_value_element_name(sc->VideosinkElement, &videosink_name);

	if(videosink_name != NULL) {
		_mmcam_dbg_log("Videosink name : %s", videosink_name);
		ret = _mmcamcorder_client_create_preview_elements(handle, videosink_name, string_caps);
		if (ret != MM_ERROR_NONE) {
			_mmcam_dbg_err("create client's preview elements Failed.");
			goto _ERR_CAMCORDER_SET_DISPLAY;
		}
	} else {
		_mmcam_dbg_err("Get Videosink name error");
		goto _ERR_CAMCORDER_SET_DISPLAY;
	}

	/* set command function */
	_mmcamcorder_set_state(handle, MM_CAMCORDER_STATE_READY);

	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);
	return ret;

_ERR_CAMCORDER_SET_DISPLAY:
	if (hcamcorder->sub_context) {
		_mmcamcorder_dealloc_subcontext(hcamcorder->sub_context);
		hcamcorder->sub_context = NULL;
	}
_ERR_CAMCORDER_CMD:
	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);
_ERR_CAMCORDER_CMD_PRECON:
	return ret;
}

int _mmcamcorder_client_unrealize(MMHandleType handle)
{
	int ret = MM_ERROR_NONE;
	int state = MM_CAMCORDER_STATE_NONE;
	int state_FROM = MM_CAMCORDER_STATE_READY;
	int state_TO = MM_CAMCORDER_STATE_NULL;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);
	_mmcam_dbg_log("Enter");

	if (!hcamcorder) {
		_mmcam_dbg_err("Not initialized");
		ret = MM_ERROR_CAMCORDER_NOT_INITIALIZED;
		return ret;
	}

	_mmcam_dbg_log("try lock");
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
		_mmcam_dbg_warn("destroy pipeline");
		__mmcamcorder_client_gst_destroy_pipeline(handle);
		/* Deallocate SubContext */
		_mmcamcorder_dealloc_subcontext(hcamcorder->sub_context);
		hcamcorder->sub_context = NULL;
	}

	/* Deinitialize main context member */
	hcamcorder->command = NULL;

	_mmcam_dbg_log("un lock");
	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

	_mmcamcorder_set_state(handle, state_TO);

	return MM_ERROR_NONE;

_ERR_CAMCORDER_CMD_PRECON_AFTER_LOCK:
	_mmcam_dbg_log("un lock");
	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

_ERR_CAMCORDER_CMD_PRECON:
	/* send message */
	_mmcam_dbg_err("Unrealize fail (type %d, ret %x)",
	               hcamcorder->type, ret);

	return ret;
}

static int _mm_camcorder_client_set_socket_path(MMHandleType handle, const char *path)
{
	int ret = MM_ERROR_NONE;

	_mmcam_dbg_log("handle : 0x%x, path:%s", handle, path);

	mm_camcorder_set_attributes(handle, NULL,
	                            MMCAM_DISPLAY_SOCKET_PATH, path, strlen(path),
	                            NULL);

	return ret;
}

int mm_camcorder_client_create(MMHandleType *handle)
{
	int ret = MM_ERROR_NONE;
	mmf_camcorder_t *hcamcorder = NULL;

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

	g_mutex_init(&(hcamcorder->mtsafe).lock);
	g_cond_init(&(hcamcorder->mtsafe).cond);
	g_mutex_init(&(hcamcorder->mtsafe).cmd_lock);
	g_mutex_init(&(hcamcorder->mtsafe).state_lock);
	g_mutex_init(&(hcamcorder->mtsafe).gst_state_lock);
	g_mutex_init(&(hcamcorder->mtsafe).message_cb_lock);
	g_mutex_init(&hcamcorder->restart_preview_lock);

	/* Get Camera Configure information from Camcorder INI file */
	_mmcamcorder_conf_get_info((MMHandleType)hcamcorder, CONFIGURE_TYPE_MAIN, CONFIGURE_MAIN_FILE, &hcamcorder->conf_main);

	if (!(hcamcorder->conf_main)) {
		_mmcam_dbg_err( "Failed to get configure(main) info." );

		ret = MM_ERROR_CAMCORDER_CREATE_CONFIGURE;
		goto _ERR_CAMCORDER_CREATE_CONFIGURE;
	}
	_mmcam_dbg_log("aloc attribute handle : 0x%x", (MMHandleType)hcamcorder);
	hcamcorder->attributes = _mmcamcorder_client_alloc_attribute((MMHandleType)hcamcorder);
	if (!(hcamcorder->attributes)) {
		_mmcam_dbg_err("_mmcamcorder_create::alloc attribute error.");

		ret = MM_ERROR_CAMCORDER_RESOURCE_CREATION;
		goto _ERR_CAMCORDER_RESOURCE_CREATION;
	}

	ret = __mmcamcorder_client_gstreamer_init(hcamcorder->conf_main);
	if (!ret) {
		_mmcam_dbg_err( "Failed to initialize gstreamer!!" );
		ret = MM_ERROR_CAMCORDER_NOT_INITIALIZED;
		goto _ERR_DEFAULT_VALUE_INIT;
	}

	ret = _mmcamcorder_client_init_attr_from_configure((MMHandleType)hcamcorder);
	if (ret != MM_ERROR_NONE) {
		_mmcam_dbg_warn("client_init_attr_from_configure error!!");
		ret = MM_ERROR_CAMCORDER_INTERNAL;
		goto _ERR_DEFAULT_VALUE_INIT;
	}

	/* Get UseZeroCopyFormat value from INI */
	_mmcamcorder_conf_get_value_int((MMHandleType)hcamcorder, hcamcorder->conf_main,
	                                CONFIGURE_CATEGORY_MAIN_VIDEO_INPUT,
	                                "UseZeroCopyFormat",
	                                &(hcamcorder->use_zero_copy_format));

	/* Set initial state */
	_mmcamcorder_set_state((MMHandleType)hcamcorder, MM_CAMCORDER_STATE_NULL);

	_mmcam_dbg_log("created handle %p - UseZeroCopyFormat %d",
		hcamcorder, hcamcorder->use_zero_copy_format);

	*handle = (MMHandleType)hcamcorder;
	_mmcam_dbg_log("created client handle : 0x%x", *handle);

	return MM_ERROR_NONE;

_ERR_CAMCORDER_CREATE_CONFIGURE:
_ERR_CAMCORDER_RESOURCE_CREATION:
_ERR_DEFAULT_VALUE_INIT:
	/* Release lock, cond */
	g_mutex_clear(&(hcamcorder->mtsafe).lock);
	g_cond_clear(&(hcamcorder->mtsafe).cond);
	g_mutex_clear(&(hcamcorder->mtsafe).cmd_lock);
	g_mutex_clear(&(hcamcorder->mtsafe).state_lock);
	g_mutex_clear(&(hcamcorder->mtsafe).gst_state_lock);
	g_mutex_clear(&(hcamcorder->mtsafe).gst_encode_state_lock);
	g_mutex_clear(&(hcamcorder->mtsafe).message_cb_lock);

	g_mutex_clear(&(hcamcorder->restart_preview_lock));

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

void mm_camcorder_client_destroy(MMHandleType handle)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);

	_mmcam_dbg_log("try lock");

	if (!_MMCAMCORDER_TRYLOCK_CMD(hcamcorder)) {
		_mmcam_dbg_err("Another command is running.");
		goto _ERR_CAMCORDER_CMD_PRECON;
	}

	if (hcamcorder->attributes) {
		_mmcamcorder_dealloc_attribute(handle, hcamcorder->attributes);
	}

	_mmcam_dbg_log("unlock");
	_MMCAMCORDER_UNLOCK_CMD(hcamcorder);

	g_mutex_clear(&((hcamcorder->mtsafe).lock));
	g_cond_clear(&((hcamcorder->mtsafe).cond));
	g_mutex_clear(&((hcamcorder->mtsafe).cmd_lock));
	g_mutex_clear(&((hcamcorder->mtsafe).state_lock));
	g_mutex_clear(&((hcamcorder->mtsafe).gst_state_lock));
	g_mutex_clear(&((hcamcorder->mtsafe).message_cb_lock));
	g_mutex_clear(&(hcamcorder->restart_preview_lock));
	_mmcamcorder_set_state((MMHandleType)hcamcorder, MM_CAMCORDER_STATE_NONE);
	/* Release handle */
	memset(hcamcorder, 0x00, sizeof(mmf_camcorder_t));
	free(hcamcorder);

_ERR_CAMCORDER_CMD_PRECON:
	return;
}

int mm_camcorder_client_realize(MMHandleType handle, char *caps)
{
	int ret = MM_ERROR_NONE;
	ret = _mmcamcorder_client_realize(handle, caps);
	return ret;
}

int mm_camcorder_client_unrealize(MMHandleType handle)
{
	int ret = MM_ERROR_NONE;
	ret = _mmcamcorder_client_unrealize(handle);
	return ret;
}

int mm_camcorder_client_set_socket_path(MMHandleType handle, const char *path)
{
	int ret = MM_ERROR_NONE;
	_mmcam_dbg_log("Entered ");
	ret = _mm_camcorder_client_set_socket_path(handle, path);
	return ret;
}

int mm_camcorder_client_get_root_directory(char **root_directory)
{
	int ret = STORAGE_ERROR_NONE;

	if (root_directory == NULL) {
		_mmcam_dbg_warn("user data is NULL");
		return MM_ERROR_CAMCORDER_INVALID_ARGUMENT;
	}

	ret = storage_foreach_device_supported((storage_device_supported_cb)_storage_device_supported_cb, root_directory);
	if (ret != STORAGE_ERROR_NONE) {
		_mmcam_dbg_err("storage_foreach_device_supported failed 0x%x", ret);
		return MM_ERROR_CAMCORDER_INTERNAL;
	}

	return MM_ERROR_NONE;
}
