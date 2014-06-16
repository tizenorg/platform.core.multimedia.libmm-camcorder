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
=======================================================================================*/
#include <sys/time.h>
#include <sys/times.h>
#include <gst/video/cameracontrol.h>
#include <mm_sound.h>
#include "mm_camcorder_internal.h"
#include "mm_camcorder_stillshot.h"
#include "mm_camcorder_exifinfo.h"
#include "mm_camcorder_exifdef.h"

/*---------------------------------------------------------------------------------------
|    GLOBAL VARIABLE DEFINITIONS for internal						|
---------------------------------------------------------------------------------------*/
#define EXIF_SET_ERR( return_type,tag_id) \
	_mmcam_dbg_err("error=%x,tag=%x",return_type,tag_id); \
	if (return_type == MM_ERROR_CAMCORDER_LOW_MEMORY) { \
		goto exit; \
	}

/*---------------------------------------------------------------------------------------
|    LOCAL VARIABLE DEFINITIONS for internal						|
---------------------------------------------------------------------------------------*/
#define THUMBNAIL_WIDTH         320
#define THUMBNAIL_HEIGHT        240
#define THUMBNAIL_DEFAULT_RATIO 1.34
#define THUMBNAIL_JPEG_QUALITY  50
#define TRY_LOCK_MAX_COUNT      100
#define TRY_LOCK_TIME           50000   /* ms */


/*---------------------------------------------------------------------------------------
|    LOCAL FUNCTION PROTOTYPES:								|
---------------------------------------------------------------------------------------*/
/** STATIC INTERNAL FUNCTION **/
/* Functions for JPEG capture without Encode bin */
int _mmcamcorder_image_cmd_capture(MMHandleType handle);
int _mmcamcorder_image_cmd_preview_start(MMHandleType handle);
int _mmcamcorder_image_cmd_preview_stop(MMHandleType handle);
static void __mmcamcorder_image_capture_cb(GstElement *element, GstSample *sample1, GstSample *sample2, GstSample *sample3, gpointer u_data);

/* Function for JPEG capture with Encode bin */
static gboolean	__mmcamcorder_encodesink_handoff_callback(GstElement *fakesink, GstBuffer *buffer, GstPad *pad, gpointer u_data);

/* sound status changed callback */
static void __sound_status_changed_cb(keynode_t* node, void *data);
static void __volume_level_changed_cb(void* user_data);

/*=======================================================================================
|  FUNCTION DEFINITIONS									|
=======================================================================================*/

/*---------------------------------------------------------------------------------------
|    GLOBAL FUNCTION DEFINITIONS:							|
---------------------------------------------------------------------------------------*/


int _mmcamcorder_add_stillshot_pipeline(MMHandleType handle)
{
	int err = MM_ERROR_UNKNOWN;

	GstPad *srcpad = NULL;
	GstPad *sinkpad = NULL;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;
	_MMCamcorderImageInfo *info = NULL;
	
	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	mmf_return_val_if_fail(sc && sc->info_image && sc->element, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	info = sc->info_image;

	_mmcam_dbg_log("");

	/* Check main pipeline */
	if (!sc->element[_MMCAMCORDER_MAIN_PIPE].gst) {
		_mmcam_dbg_err( "Main Pipeline is not existed." );
		return MM_ERROR_CAMCORDER_RESOURCE_CREATION;
	}

	/* Create gstreamer element */
	_mmcam_dbg_log("Using Encodebin for capturing");

	__ta__("        _mmcamcorder_create_encodesink_bin",
	err = _mmcamcorder_create_encodesink_bin((MMHandleType)hcamcorder, MM_CAMCORDER_ENCBIN_PROFILE_IMAGE);
	);
	if (err != MM_ERROR_NONE) {
		return err;
	}

	gst_bin_add_many(GST_BIN(sc->element[_MMCAMCORDER_MAIN_PIPE].gst),
	                 sc->element[_MMCAMCORDER_ENCSINK_BIN].gst,
	                 NULL);

	/* Link each element */
	srcpad = gst_element_get_static_pad(sc->element[_MMCAMCORDER_VIDEOSRC_BIN].gst, "src1");
	sinkpad = gst_element_get_static_pad(sc->element[_MMCAMCORDER_ENCSINK_BIN].gst, "image_sink0");
	_MM_GST_PAD_LINK_UNREF(srcpad, sinkpad, err, element_link_error)

	MMCAMCORDER_SIGNAL_CONNECT(sc->element[_MMCAMCORDER_ENCSINK_SINK].gst,
	                           _MMCAMCORDER_HANDLER_STILLSHOT, "handoff",
	                           G_CALLBACK(__mmcamcorder_encodesink_handoff_callback),
	                           hcamcorder);

	return MM_ERROR_NONE;

element_link_error:
	_mmcam_dbg_err("Link encodebin failed!");

	if (sc->element[_MMCAMCORDER_ENCSINK_BIN].gst != NULL) {
		int ret = MM_ERROR_NONE;

		__ta__( "        EncodeBin Set NULL",
		ret = _mmcamcorder_gst_set_state(handle, sc->element[_MMCAMCORDER_ENCSINK_BIN].gst, GST_STATE_NULL);
		);
		_mmcam_dbg_log("encodebin _mmcamcorder_gst_set_state NULL ret[%d]", ret);

		gst_bin_remove(GST_BIN(sc->element[_MMCAMCORDER_MAIN_PIPE].gst),
		               sc->element[_MMCAMCORDER_ENCSINK_BIN].gst);

		_mmcamcorder_remove_element_handle(handle, _MMCAMCORDER_ENCSINK_BIN, _MMCAMCORDER_ENCSINK_SINK);

		_mmcam_dbg_log("Encodebin removed");
	}

	return MM_ERROR_CAMCORDER_GST_LINK;
}


int _mmcamcorder_connect_capture_signal(MMHandleType handle)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	mmf_return_val_if_fail(sc && sc->element, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	/*set video source element*/
	if (sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst) {
		MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst, "signal-still-capture", TRUE);

		MMCAMCORDER_SIGNAL_CONNECT(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst,
		                           _MMCAMCORDER_HANDLER_STILLSHOT, "still-capture",
		                           G_CALLBACK(__mmcamcorder_image_capture_cb),
		                           hcamcorder);
		return MM_ERROR_NONE;
	} else {
		_mmcam_dbg_err("videosrc element is not created yet");
		return MM_ERROR_CAMCORDER_NOT_INITIALIZED;
	}
}


int _mmcamcorder_remove_stillshot_pipeline(MMHandleType handle)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;
	_MMCamcorderImageInfo *info = NULL;

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	mmf_return_val_if_fail(sc && sc->info_image && sc->element, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	info = sc->info_image;

	_mmcam_dbg_log("");

	/* Check pipeline */
	if (sc->element[_MMCAMCORDER_MAIN_PIPE].gst) {
		_mmcamcorder_remove_all_handlers(handle, _MMCAMCORDER_HANDLER_STILLSHOT);

		if (sc->bencbin_capture) {
			GstPad *srcpad = NULL, *sinkpad = NULL;
			if (!sc->element[_MMCAMCORDER_ENCSINK_BIN].gst) {
				_mmcam_dbg_log("ENCSINK_BIN is already removed");
			} else {
				/* Unlink each element */
				srcpad = gst_element_get_static_pad(sc->element[_MMCAMCORDER_VIDEOSRC_BIN].gst, "src1");
				sinkpad = gst_element_get_static_pad(sc->element[_MMCAMCORDER_ENCSINK_BIN].gst, "image_sink0");
				_MM_GST_PAD_UNLINK_UNREF(srcpad, sinkpad);

				gst_bin_remove(GST_BIN(sc->element[_MMCAMCORDER_MAIN_PIPE].gst),
					sc->element[_MMCAMCORDER_ENCSINK_BIN].gst);

				_mmcamcorder_remove_element_handle(handle, _MMCAMCORDER_ENCSINK_BIN, _MMCAMCORDER_ENCSINK_SINK);
			}
		}
	} else {
		_mmcam_dbg_log("MAIN_PIPE is already removed");
	}

	return MM_ERROR_NONE;
}


void _mmcamcorder_destroy_video_capture_pipeline(MMHandleType handle)
{
	GstPad *reqpad1 = NULL;
	GstPad *reqpad2 = NULL;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;

	mmf_return_if_fail(hcamcorder);

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	mmf_return_if_fail(sc && sc->element);

	_mmcam_dbg_log("");

	if (sc->element[_MMCAMCORDER_MAIN_PIPE].gst) {
		MMCAMCORDER_G_OBJECT_SET( sc->element[_MMCAMCORDER_VIDEOSINK_QUE].gst, "empty-buffers", TRUE);
		_mmcamcorder_gst_set_state(handle, sc->element[_MMCAMCORDER_MAIN_PIPE].gst, GST_STATE_NULL);

		_mmcamcorder_remove_all_handlers(handle, _MMCAMCORDER_HANDLER_CATEGORY_ALL);

		reqpad1 = gst_element_get_static_pad(sc->element[_MMCAMCORDER_VIDEOSRC_TEE].gst, "src0");
		reqpad2 = gst_element_get_static_pad(sc->element[_MMCAMCORDER_VIDEOSRC_TEE].gst, "src1");
		gst_element_release_request_pad(sc->element[_MMCAMCORDER_VIDEOSRC_TEE].gst, reqpad1);
		gst_element_release_request_pad(sc->element[_MMCAMCORDER_VIDEOSRC_TEE].gst, reqpad2);
		gst_object_unref(reqpad1);
		gst_object_unref(reqpad2);

		gst_object_unref(sc->element[_MMCAMCORDER_MAIN_PIPE].gst);
		/* NULL initialization will be done in _mmcamcorder_element_release_noti */
	}
}


int _mmcamcorder_image_cmd_capture(MMHandleType handle)
{
	int ret = MM_ERROR_NONE;
	int UseCaptureMode = 0;
	int width = 0;
	int height = 0;
	int fps = 0;
	int cap_format = MM_PIXEL_FORMAT_NV12;
	int image_encoder = MM_IMAGE_CODEC_JPEG;
	int strobe_mode = MM_CAMCORDER_STROBE_MODE_OFF;
	unsigned int cap_fourcc = 0;

	char *err_name = NULL;
	char *videosrc_name = NULL;

	GstElement *pipeline = NULL;
	GstCameraControl *control = NULL;
	GstCaps *caps = NULL;
	GstClock *clock = NULL;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderImageInfo *info = NULL;
	_MMCamcorderSubContext *sc = NULL;
	type_element *VideosrcElement = NULL;
	MMCamcorderStateType current_state = MM_CAMCORDER_STATE_NONE;

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	mmf_return_val_if_fail(sc && sc->info_image && sc->element, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	_mmcam_dbg_log("");

	pipeline = sc->element[_MMCAMCORDER_MAIN_PIPE].gst;
	info = sc->info_image;

	_mmcamcorder_conf_get_value_int(hcamcorder->conf_main,
	                                CONFIGURE_CATEGORY_MAIN_CAPTURE,
	                                "UseCaptureMode",
	                                &UseCaptureMode);

	_mmcamcorder_conf_get_element(hcamcorder->conf_main,
	                              CONFIGURE_CATEGORY_MAIN_VIDEO_INPUT,
	                              "VideosrcElement",
	                              &VideosrcElement);

	_mmcamcorder_conf_get_value_element_name(VideosrcElement, &videosrc_name);

	if (info->capturing) {
		ret = MM_ERROR_CAMCORDER_DEVICE_BUSY;
		goto cmd_error;
	}

	MMTA_ACUM_ITEM_BEGIN("Real First Capture Start",false);

	/* get current state */
	mm_camcorder_get_state(handle, &current_state);

	/* set capture flag */
	info->capturing = TRUE;

	mm_camcorder_get_attributes(handle, &err_name,
	                            MMCAM_IMAGE_ENCODER, &image_encoder,
	                            MMCAM_CAMERA_WIDTH, &width,
	                            MMCAM_CAMERA_HEIGHT, &height,
	                            MMCAM_CAMERA_FPS, &fps,
	                            MMCAM_CAMERA_HDR_CAPTURE, &info->hdr_capture_mode,
	                            MMCAM_CAPTURE_FORMAT, &cap_format,
	                            MMCAM_CAPTURE_WIDTH, &info->width,
	                            MMCAM_CAPTURE_HEIGHT, &info->height,
	                            MMCAM_CAPTURE_COUNT, &info->count,
	                            MMCAM_CAPTURE_INTERVAL, &info->interval,
	                            MMCAM_STROBE_MODE, &strobe_mode,
	                            NULL);
	if (err_name) {
		_mmcam_dbg_warn("get_attributes err %s", err_name);
		free(err_name);
		err_name = NULL;
	}

	/* check capture count */
	if (info->count < 1) {
		_mmcam_dbg_err("capture count[%d] is invalid", info->count);
		ret = MM_ERROR_CAMCORDER_INVALID_ARGUMENT;
		goto cmd_error;
	} else if (info->count == 1) {
		info->type = _MMCamcorder_SINGLE_SHOT;
	} else {
		info->type = _MMCamcorder_MULTI_SHOT;
		info->next_shot_time = 0;
		info->multi_shot_stop = FALSE;

		/* sound init to pause other session */
		__ta__("        _mmcamcorder_sound_init",
#ifdef _MMCAMCORDER_UPLOAD_SAMPLE
		_mmcamcorder_sound_init(handle, _MMCAMCORDER_FILEPATH_CAPTURE2_SND);
#else /* _MMCAMCORDER_UPLOAD_SAMPLE */
		_mmcamcorder_sound_init(handle);
#endif /* _MMCAMCORDER_UPLOAD_SAMPLE */
		);
	}

	_mmcam_dbg_log("preview(%dx%d,fmt:%d), capture(%dx%d,fmt:%d), count(%d), hdr mode(%d)",
	               width, height, info->preview_format,
	               info->width, info->height, cap_format,
	               info->count, info->hdr_capture_mode);

	/* check state */
	if (current_state >= MM_CAMCORDER_STATE_RECORDING) {
		if (info->type == _MMCamcorder_MULTI_SHOT ||
		    info->hdr_capture_mode != MM_CAMCORDER_HDR_OFF ||
		    info->preview_format != MM_PIXEL_FORMAT_ITLV_JPEG_UYVY) {
			_mmcam_dbg_err("not support multi/HDR capture while recording or NOT ITLV format");
			ret = MM_ERROR_CAMCORDER_INVALID_STATE;
			goto cmd_error;
		}
	}

	info->capture_cur_count = 0;
	info->capture_send_count = 0;

	sc->internal_encode = FALSE;

	if (!sc->bencbin_capture) {
		/* Check encoding method */
		if (cap_format == MM_PIXEL_FORMAT_ENCODED) {
			if (sc->SensorEncodedCapture && info->type == _MMCamcorder_SINGLE_SHOT) {
				cap_fourcc = _mmcamcorder_get_fourcc(cap_format, image_encoder, hcamcorder->use_zero_copy_format);
				_mmcam_dbg_log("Sensor JPEG Capture");
			} else {
				/* no need to encode internally if ITLV format */
				if (info->preview_format != MM_PIXEL_FORMAT_ITLV_JPEG_UYVY) {
					sc->internal_encode = TRUE;
				}

				cap_fourcc = _mmcamcorder_get_fourcc(info->preview_format, image_encoder, hcamcorder->use_zero_copy_format);

				_mmcam_dbg_log("MSL JPEG Capture : capture fourcc %c%c%c%c",
				               cap_fourcc, cap_fourcc>>8, cap_fourcc>>16, cap_fourcc>>24);
			}
		} else {
			cap_fourcc = _mmcamcorder_get_fourcc(cap_format, MM_IMAGE_CODEC_INVALID, hcamcorder->use_zero_copy_format);
			if (info->type == _MMCamcorder_SINGLE_SHOT && !strcmp(videosrc_name, "camerasrc")) {
				MMCAMCORDER_G_OBJECT_SET( sc->element[_MMCAMCORDER_VIDEOSINK_QUE].gst, "empty-buffers", TRUE );
				MMCAMCORDER_G_OBJECT_SET( sc->element[_MMCAMCORDER_VIDEOSINK_SINK].gst, "stop-video", TRUE );
			}
		}

		_mmcam_dbg_log("capture format (%d)", cap_format);

		/* Note: width/height of capture is set in commit function of attribute or in create function of pipeline */
		MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst, "capture-fourcc", cap_fourcc);
		MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst, "capture-interval", info->interval);
		MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst, "capture-count", info->count);
		MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst, "hdr-capture", info->hdr_capture_mode);

		if (!GST_IS_CAMERA_CONTROL(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst)) {
			_mmcam_dbg_err("Can't cast Video source into camera control.");
			return MM_ERROR_CAMCORDER_NOT_SUPPORTED;
		}

		control = GST_CAMERA_CONTROL(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst);
		gst_camera_control_set_capture_command(control, GST_CAMERA_CONTROL_CAPTURE_COMMAND_START);
	} else {
		int need_change = 0;
		int set_width = 0;
		int set_height = 0;
		int cap_jpeg_quality = 0;

		if (current_state >= MM_CAMCORDER_STATE_RECORDING) {
			_mmcam_dbg_err("could not capture in this target while recording");
			ret = MM_ERROR_CAMCORDER_INVALID_STATE;
			goto cmd_error;
		}

		if (UseCaptureMode) {
			if (width != MMFCAMCORDER_HIGHQUALITY_WIDTH || height != MMFCAMCORDER_HIGHQUALITY_HEIGHT) {
				need_change = 1;
			}
		} else {
			if (width != info->width || height != info->height) {
				need_change = 1;
			}
		}

		/* make pipeline state as READY */
		ret = _mmcamcorder_gst_set_state(handle, sc->element[_MMCAMCORDER_MAIN_PIPE].gst, GST_STATE_READY);
		if (ret != MM_ERROR_NONE) {
			_mmcam_dbg_err("failed to set state PAUSED %x", ret);
			return ret;
		}

		/* add encodesinkbin */
		ret = _mmcamcorder_add_stillshot_pipeline((MMHandleType)hcamcorder);
		if (ret != MM_ERROR_NONE) {
			_mmcam_dbg_err("failed to create encodesinkbin %x", ret);
			return ret;
		}

		/* set JPEG quality */
		mm_camcorder_get_attributes(handle, &err_name,
		                            MMCAM_IMAGE_ENCODER_QUALITY, &cap_jpeg_quality,
		                            NULL);
		MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_ENCSINK_IENC].gst, "quality", cap_jpeg_quality);

		if (need_change) {
			_mmcam_dbg_log("Need to change resolution");

			if (UseCaptureMode) {
				set_width = MMFCAMCORDER_HIGHQUALITY_WIDTH;
				set_height = MMFCAMCORDER_HIGHQUALITY_HEIGHT;
			} else {
				set_width = info->width;
				set_height = info->height;
			}

			caps = gst_caps_new_simple("video/x-raw",
                                       "format", G_TYPE_STRING, sc->format_name,
			                           "width", G_TYPE_INT, set_width,
			                           "height", G_TYPE_INT, set_height,
			                           "framerate", GST_TYPE_FRACTION, fps, 1,
			                           "rotate", G_TYPE_INT, 0,
			                           NULL);
			if (caps == NULL) {
				_mmcam_dbg_err("failed to create caps");
				ret = MM_ERROR_CAMCORDER_LOW_MEMORY;
				goto cmd_error;
			}

			info->resolution_change = TRUE;
			MMCAMCORDER_G_OBJECT_SET( sc->element[_MMCAMCORDER_VIDEOSRC_FILT].gst, "caps", caps);
			gst_caps_unref(caps);

			/*MMCAMCORDER_G_OBJECT_SET( sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst, "num-buffers", info->count);*/
			MMCAMCORDER_G_OBJECT_SET( sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst, "req-negotiation",TRUE);

			/* FIXME: consider delay */
			clock = gst_pipeline_get_clock(GST_PIPELINE(pipeline));
			sc->stillshot_time = gst_clock_get_time(clock) - gst_element_get_base_time(GST_ELEMENT(pipeline));
			MMCAMCORDER_G_OBJECT_SET( sc->element[_MMCAMCORDER_ENCSINK_ENCBIN].gst, "block", FALSE);

			_mmcam_dbg_log("Change to target resolution(%d, %d)", set_width, set_height);
		} else {
			_mmcam_dbg_log("No need to change resolution. Open toggle now.");
		}

		MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_ENCSINK_SINK].gst,"signal-handoffs", TRUE);
		MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_ENCSINK_ENCBIN].gst, "block", FALSE);

		/* make pipeline state as PLAYING */
		ret = _mmcamcorder_gst_set_state(handle, sc->element[_MMCAMCORDER_MAIN_PIPE].gst, GST_STATE_PLAYING);
		if (ret != MM_ERROR_NONE) {
			_mmcam_dbg_err("failed to set state PLAYING %x", ret);
			return ret;
		}
	}

	/* Play capture sound here if single capture */
	if ((info->type == _MMCamcorder_SINGLE_SHOT &&
	     (info->preview_format != MM_PIXEL_FORMAT_ITLV_JPEG_UYVY || strobe_mode == MM_CAMCORDER_STROBE_MODE_OFF)) ||
	    info->hdr_capture_mode) {
		if (current_state < MM_CAMCORDER_STATE_RECORDING) {
			__ta__("        _mmcamcorder_sound_solo_play",
			_mmcamcorder_sound_solo_play((MMHandleType)hcamcorder, _MMCAMCORDER_FILEPATH_CAPTURE_SND, FALSE);
			);
		}

		/* set flag */
		info->played_capture_sound = TRUE;
	} else {
		/* set flag */
		info->played_capture_sound = FALSE;
	}

	return ret;

cmd_error:
	info->capturing = FALSE;
	return ret;
}


int _mmcamcorder_image_cmd_preview_start(MMHandleType handle)
{
	int ret = MM_ERROR_NONE;
	int width = 0;
	int height = 0;
	int fps = 0;
	int cap_width = 0;
	int cap_height = 0;
	int rotation = 0;
	int set_width = 0;
	int set_height = 0;
	int set_rotate = 0;
	int current_framecount = 0;
	int current_state = MM_CAMCORDER_STATE_NONE;
	gboolean fps_auto = FALSE;
	char *err_name = NULL;
	char *videosrc_name = NULL;

	GstState state;
	GstCaps *caps = NULL;
	GstElement *pipeline = NULL;
	GstCameraControl *control = NULL;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderImageInfo *info = NULL;
	_MMCamcorderSubContext *sc = NULL;
	type_element *VideosrcElement = NULL;

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	mmf_return_val_if_fail(sc && sc->info_image && sc->element, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	_mmcam_dbg_log("");

	pipeline = sc->element[_MMCAMCORDER_MAIN_PIPE].gst;
	info = sc->info_image;

	MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSINK_QUE].gst, "empty-buffers", TRUE);

	_mmcamcorder_conf_get_element(hcamcorder->conf_main,
	                              CONFIGURE_CATEGORY_MAIN_VIDEO_INPUT,
	                              "VideosrcElement",
	                              &VideosrcElement);

	_mmcamcorder_conf_get_value_element_name(VideosrcElement, &videosrc_name);

	sc->display_interval = 0;
	sc->previous_slot_time = 0;

	/* Image info */
	info->next_shot_time = 0;
	info->multi_shot_stop = TRUE;
	info->capturing = FALSE;

	_mmcamcorder_vframe_stablize(handle);

	current_state = _mmcamcorder_get_state(handle);
	_mmcam_dbg_log("current state [%d]", current_state);

	if (!sc->bencbin_capture) {
		_mmcam_dbg_log("Capture Preview start : avsysvideosrc - No need to set new caps.");

		/* just set capture stop command if current state is CAPTURING */
		if (current_state == MM_CAMCORDER_STATE_CAPTURING) {
			if (!GST_IS_CAMERA_CONTROL(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst)) {
				_mmcam_dbg_err("Can't cast Video source into camera control.");
				return MM_ERROR_CAMCORDER_NOT_SUPPORTED;
			}

			control = GST_CAMERA_CONTROL(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst);
			gst_camera_control_set_capture_command(control, GST_CAMERA_CONTROL_CAPTURE_COMMAND_STOP);

			current_framecount = sc->kpi.video_framecount;
			MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSINK_SINK].gst, "stop-video", FALSE);
			if (info->type == _MMCamcorder_SINGLE_SHOT) {
				MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSINK_QUE].gst, "empty-buffers", FALSE);
			}
		} else {
			int focus_mode = 0;

			/* This case is starting of preview */
			ret = mm_camcorder_get_attributes(handle, &err_name,
			                                  MMCAM_CAMERA_FPS_AUTO, &fps_auto,
			                                  MMCAM_CAMERA_FOCUS_MODE, &focus_mode,
			                                  NULL);
			if (ret != MM_ERROR_NONE) {
				_mmcam_dbg_warn("Get attrs fail. (%s:%x)", err_name, ret);
				SAFE_FREE (err_name);
			}

			MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst, "fps-auto", fps_auto);

			/* set focus mode */
			mm_camcorder_set_attributes(handle, NULL,
			                            MMCAM_CAMERA_FOCUS_MODE, focus_mode,
			                            NULL);
		}
	} else {
		/* check if resolution need to rollback */
		mm_camcorder_get_attributes(handle, &err_name,
		                            MMCAM_CAMERA_WIDTH, &width,
		                            MMCAM_CAMERA_HEIGHT, &height,
		                            MMCAM_CAMERA_FPS, &fps,
		                            MMCAM_CAMERA_FPS_AUTO, &fps_auto,
		                            MMCAM_CAMERA_ROTATION, &rotation,
		                            MMCAM_CAPTURE_WIDTH, &cap_width,
		                            MMCAM_CAPTURE_HEIGHT, &cap_height,
		                            NULL);

		if (current_state == MM_CAMCORDER_STATE_CAPTURING) {
			switch (rotation) {
			case MM_VIDEO_INPUT_ROTATION_90:
				set_width = height;
				set_height = width;
				set_rotate = 90;
				break;
			case MM_VIDEO_INPUT_ROTATION_180:
				set_width = width;
				set_height = height;
				set_rotate = 180;
				break;
			case MM_VIDEO_INPUT_ROTATION_270:
				set_width = height;
				set_height = width;
				set_rotate = 270;
				break;
			case MM_VIDEO_INPUT_ROTATION_NONE:
			default:
				set_width = width;
				set_height = height;
				set_rotate = 0;
				break;
			}

			caps = gst_caps_new_simple("video/x-raw",
                                       "format", G_TYPE_STRING, sc->format_name,
			                           "width", G_TYPE_INT, set_width,
			                           "height", G_TYPE_INT,set_height,
			                           "framerate", GST_TYPE_FRACTION, fps, 1,
			                           "rotate", G_TYPE_INT, set_rotate,
			                           NULL);
			if (caps != NULL) {
				MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSRC_FILT].gst, "caps", caps);
				gst_caps_unref(caps);
				_mmcam_dbg_log("Rollback to original resolution(%d, %d)", width, height);
			} else {
				_mmcam_dbg_err("failed to create caps");
				ret = MM_ERROR_CAMCORDER_LOW_MEMORY;
				goto cmd_error;
			}
		}
	}

	gst_element_get_state(pipeline, &state, NULL, -1);

	if (state == GST_STATE_PLAYING) {
		if (!strcmp(videosrc_name, "avsysvideosrc") || !strcmp(videosrc_name, "camerasrc")) {
			int try_count = 0;

			if (info->preview_format != MM_PIXEL_FORMAT_ITLV_JPEG_UYVY) {
				mmf_attrs_t *attr = (mmf_attrs_t *)MMF_CAMCORDER_ATTRS(handle);

				/* Set strobe mode - strobe mode can not be set to driver while captuing */
				if (attr) {
					__ta__("    Set strobe mode after capture",
					mmf_attribute_set_modified(&(attr->items[MM_CAM_STROBE_MODE]));
					if (mmf_attrs_commit((MMHandleType) attr) == -1) {
						_mmcam_dbg_warn("Failed to set strobe mode");
					}
					);
				}

				__ta__("    Wait preview frame after capture",
				while (current_framecount >= sc->kpi.video_framecount &&
				       try_count++ < _MMCAMCORDER_CAPTURE_STOP_CHECK_COUNT) {
					usleep(_MMCAMCORDER_CAPTURE_STOP_CHECK_INTERVAL);
				}
				);
			}

			if (info->type == _MMCamcorder_MULTI_SHOT) {
				MMCAMCORDER_G_OBJECT_SET( sc->element[_MMCAMCORDER_VIDEOSINK_QUE].gst, "empty-buffers", FALSE);
			}

			_mmcam_dbg_log("Wait Frame Done. count before[%d],after[%d], try_count[%d]",
			               current_framecount, sc->kpi.video_framecount, try_count);
		} else {
			MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSINK_QUE].gst, "empty-buffers", TRUE);
			ret = _mmcamcorder_gst_set_state(handle, pipeline, GST_STATE_READY);
			MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSINK_QUE].gst, "empty-buffers", FALSE);
			MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_ENCSINK_ENCBIN].gst, "block", FALSE);
			if (ret != MM_ERROR_NONE) {
				goto cmd_error;
			}

			/* remove handoff signal handler */
			_mmcamcorder_remove_all_handlers(handle, _MMCAMCORDER_HANDLER_STILLSHOT);

			/* remove encodesinkbin after capture */
			if (!sc->element[_MMCAMCORDER_ENCSINK_BIN].gst) {
				_mmcam_dbg_log("ENCSINK_BIN is already removed");
			} else {
				GstPad *reqpad0 = NULL;
				GstPad *srcpad = NULL;
				GstPad *sinkpad = NULL;

				/* release requested pad */
				reqpad0 = gst_element_get_static_pad(sc->element[_MMCAMCORDER_ENCSINK_ENCBIN].gst, "image");
				gst_element_release_request_pad(sc->element[_MMCAMCORDER_ENCSINK_ENCBIN].gst, reqpad0);
				gst_object_unref(reqpad0);

				/* Unlink each element */
				srcpad = gst_element_get_static_pad(sc->element[_MMCAMCORDER_VIDEOSRC_BIN].gst, "src1");
				sinkpad = gst_element_get_static_pad(sc->element[_MMCAMCORDER_ENCSINK_BIN].gst, "image_sink0");
				_MM_GST_PAD_UNLINK_UNREF(srcpad, sinkpad);

				/* make state of encodesinkbin as NULL */
				_mmcamcorder_gst_set_state(handle, sc->element[_MMCAMCORDER_ENCSINK_BIN].gst, GST_STATE_NULL);

				gst_bin_remove(GST_BIN(sc->element[_MMCAMCORDER_MAIN_PIPE].gst),
				               sc->element[_MMCAMCORDER_ENCSINK_BIN].gst);

				_mmcamcorder_remove_element_handle(handle, _MMCAMCORDER_ENCSINK_BIN, _MMCAMCORDER_ENCSINK_SINK);
			}

			ret = _mmcamcorder_gst_set_state(handle, pipeline, GST_STATE_PLAYING);
			if (ret != MM_ERROR_NONE) {
				goto cmd_error;
			}
		}

		/* sound finalize */
		if (info->type == _MMCamcorder_MULTI_SHOT) {
			__ta__("    _mmcamcorder_sound_finalize",
			_mmcamcorder_sound_finalize(handle);
			);
		}
	} else {
		int cap_count = 0;
		int sound_ret = FALSE;

		MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSINK_QUE].gst, "empty-buffers", FALSE);

		__ta__("        _MMCamcorder_CMD_PREVIEW_START:GST_STATE_PLAYING",
		ret = _mmcamcorder_gst_set_state(handle, pipeline, GST_STATE_PLAYING);
		);
		if (ret != MM_ERROR_NONE) {
			goto cmd_error;
		}

		/* get sound status/volume level and register changed_cb */
		if (hcamcorder->shutter_sound_policy == VCONFKEY_CAMERA_SHUTTER_SOUND_POLICY_OFF &&
		    info->sound_status == _SOUND_STATUS_INIT) {
			_mmcam_dbg_log("get sound status/volume level and register vconf changed_cb");

			/* get sound status */
			__ta__("        GET:VCONFKEY_SETAPPL_SOUND_STATUS_BOOL",
			vconf_get_bool(VCONFKEY_SETAPPL_SOUND_STATUS_BOOL, &(info->sound_status));
			);

			_mmcam_dbg_log("sound status %d", info->sound_status);

			/* get volume level */
			__ta__("        mm_sound_volume_get_value",
			mm_sound_volume_get_value(VOLUME_TYPE_MEDIA, &info->volume_level);
			);

			_mmcam_dbg_log("volume level %d", info->volume_level);

			/* register changed_cb */
			vconf_notify_key_changed(VCONFKEY_SETAPPL_SOUND_STATUS_BOOL, __sound_status_changed_cb, hcamcorder);
			mm_sound_volume_add_callback(VOLUME_TYPE_MEDIA, __volume_level_changed_cb, hcamcorder);
		}

		mm_camcorder_get_attributes(handle, NULL, MMCAM_CAPTURE_COUNT, &cap_count, NULL);
		if (cap_count > 1) {
			__ta__("_mmcamcorder_sound_init",
#ifdef _MMCAMCORDER_UPLOAD_SAMPLE
			sound_ret = _mmcamcorder_sound_init(handle, _MMCAMCORDER_FILEPATH_CAPTURE2_SND);
#else /* _MMCAMCORDER_UPLOAD_SAMPLE */
			sound_ret = _mmcamcorder_sound_init(handle);
#endif /* _MMCAMCORDER_UPLOAD_SAMPLE */
			);
		}
	}

cmd_error:
	return ret;
}


int _mmcamcorder_image_cmd_preview_stop(MMHandleType handle)
{
	int ret = MM_ERROR_NONE;

	GstElement *pipeline = NULL;

	_MMCamcorderSubContext *sc = NULL;

	_mmcam_dbg_log("");

	mmf_return_val_if_fail(handle, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	mmf_return_val_if_fail(sc && sc->element, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	pipeline = sc->element[_MMCAMCORDER_MAIN_PIPE].gst;

	MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSINK_QUE].gst, "empty-buffers", TRUE);
	ret = _mmcamcorder_gst_set_state(handle, pipeline, GST_STATE_READY);
	MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSINK_QUE].gst, "empty-buffers", FALSE);

	/* deregister sound status callback */
	if (sc->info_image->sound_status != _SOUND_STATUS_INIT) {
		_mmcam_dbg_log("deregister sound status callback");

		__ta__("    vconf_ignore_key_changed",
		vconf_ignore_key_changed(VCONFKEY_SETAPPL_SOUND_STATUS_BOOL, __sound_status_changed_cb);
		);
		__ta__("    mm_sound_volume_remove_callback",
		mm_sound_volume_remove_callback(VOLUME_TYPE_MEDIA);
		);

		sc->info_image->sound_status = _SOUND_STATUS_INIT;
	}

	return ret;
}


int _mmcamcorder_video_capture_command(MMHandleType handle, int command)
{
	int ret = MM_ERROR_NONE;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	mmf_return_val_if_fail(sc && sc->element, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	_mmcam_dbg_log("command %d", command);

	switch (command) {
	case _MMCamcorder_CMD_CAPTURE:
		ret = _mmcamcorder_image_cmd_capture(handle);
		break;
	case _MMCamcorder_CMD_PREVIEW_START:
		ret = _mmcamcorder_image_cmd_preview_start(handle);

		/* I place this function last because it miscalculates a buffer that sents in GST_STATE_PAUSED */
		_mmcamcorder_video_current_framerate_init(handle);
		break;
	case _MMCamcorder_CMD_PREVIEW_STOP:
		ret = _mmcamcorder_image_cmd_preview_stop(handle);
		break;
	case _MMCamcorder_CMD_RECORD:
	case _MMCamcorder_CMD_PAUSE:
	case _MMCamcorder_CMD_CANCEL:
	case _MMCamcorder_CMD_COMMIT:
		/* video recording command */
		ret = _mmcamcorder_video_command(handle, command);
		break;
	default:
		ret =  MM_ERROR_CAMCORDER_INVALID_ARGUMENT;
		break;
	}

	if (ret != MM_ERROR_NONE && sc->element && sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst) {
		int op_status = 0;
		MMCAMCORDER_G_OBJECT_GET (sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst, "operation-status", &op_status);
		_mmcam_dbg_err("Current Videosrc status[0x%x]", op_status);
	}

	return ret;
}


void __mmcamcorder_init_stillshot_info (MMHandleType handle)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;
	_MMCamcorderImageInfo *info = NULL;

	mmf_return_if_fail(hcamcorder);

	sc = MMF_CAMCORDER_SUBCONTEXT(hcamcorder);
	mmf_return_if_fail(sc && sc->info_image);

	info = sc->info_image;

	_mmcam_dbg_log("capture type[%d], capture send count[%d]", info->type, info->capture_send_count);

	if (info->type ==_MMCamcorder_SINGLE_SHOT || info->capture_send_count == info->count) {
		info->capture_cur_count = 0;
		info->capture_send_count = 0;
		info->multi_shot_stop = TRUE;
		info->next_shot_time = 0;

		/* capturing flag set to FALSE here */
		info->capturing = FALSE;
	}

	return;
}


int __mmcamcorder_capture_save_exifinfo(MMHandleType handle, MMCamcorderCaptureDataType *original, MMCamcorderCaptureDataType *thumbnail)
{
	int ret = MM_ERROR_NONE;
	unsigned char *data = NULL;
	unsigned int datalen = 0;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);

	mmf_return_val_if_fail(hcamcorder, FALSE);

	_mmcam_dbg_log("");

	if (!original || original->data == NULL || original->length == 0) {
		_mmcam_dbg_err("original=%p, data=%p, length=%d", original, original->data, original->length);
		return MM_ERROR_CAMCORDER_INVALID_ARGUMENT;
	} else {
		/* original is input/output param. save original values to local var. */
		data = original->data;
		datalen = original->length;
	}

	if (thumbnail) {
		if (thumbnail->data && thumbnail->length > 0) {
			_mmcam_dbg_log("thumbnail is added!thumbnail->data=%p thumbnail->width=%d ,thumbnail->height=%d",
			               thumbnail->data, thumbnail->width, thumbnail->height);

			/* add thumbnail exif info */
			__ta__("                    mm_exif_add_thumbnail_info",
			ret = mm_exif_add_thumbnail_info(hcamcorder->exif_info,
			                                 thumbnail->data,
			                                 thumbnail->width,
			                                 thumbnail->height,
			                                 thumbnail->length);
			);
		} else {
			_mmcam_dbg_err("Skip adding thumbnail (data=%p, length=%d)",
			               thumbnail->data, thumbnail->length);
		}
	}

	if (ret == MM_ERROR_NONE) {
		/* write jpeg with exif */
		__ta__("                    mm_exif_write_exif_jpeg_to_memory",
		ret = mm_exif_write_exif_jpeg_to_memory(&original->data, &original->length ,hcamcorder->exif_info,  data, datalen);
		);
		if (ret != MM_ERROR_NONE) {
			_mmcam_dbg_err("mm_exif_write_exif_jpeg_to_memory error! [0x%x]",ret);
		}
	}

	_mmcam_dbg_log("END ret 0x%x", ret);

	return ret;
}


void __mmcamcorder_get_capture_data_from_buffer(MMCamcorderCaptureDataType *capture_data, int pixtype, GstSample *sample)
{
	GstCaps *caps = NULL;
    GstMapInfo mapinfo = GST_MAP_INFO_INIT;
	const GstStructure *structure;

	mmf_return_if_fail(capture_data && sample);

    caps = gst_sample_get_caps(sample);
	if (caps == NULL) {
		_mmcam_dbg_err("failed to get caps");
		goto GET_FAILED;
	}

	structure = gst_caps_get_structure(caps, 0);
	if (caps == NULL) {
		_mmcam_dbg_err("failed to get structure");
		goto GET_FAILED;
	}

    gst_buffer_map(gst_sample_get_buffer(sample), &mapinfo, GST_MAP_READ);
    capture_data->data = mapinfo.data;
	capture_data->format = pixtype;
	gst_structure_get_int(structure, "width", &capture_data->width);
	gst_structure_get_int(structure, "height", &capture_data->height);
    capture_data->length = mapinfo.size;
    gst_buffer_unmap(gst_sample_get_buffer(sample), &mapinfo);

	 _mmcam_dbg_warn("buffer data[%p],size[%dx%d],length[%d],format[%d]",
	                 capture_data->data, capture_data->width, capture_data->height,
	                 capture_data->length, capture_data->format);
	return;

GET_FAILED:
	capture_data->data = NULL;
	capture_data->format = MM_PIXEL_FORMAT_INVALID;
	capture_data->length = 0;

	return;
}


int __mmcamcorder_set_jpeg_data(MMHandleType handle, MMCamcorderCaptureDataType *dest, MMCamcorderCaptureDataType *thumbnail)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;

	mmf_return_val_if_fail(hcamcorder && dest, MM_ERROR_CAMCORDER_INVALID_ARGUMENT);

	sc = MMF_CAMCORDER_SUBCONTEXT(hcamcorder);
	mmf_return_val_if_fail(sc, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	_mmcam_dbg_log("");

	/* if tag enable and doesn't provide exif, we make it */
	_mmcam_dbg_log("Add exif information if existed(thumbnail[%p])", thumbnail);
	if (thumbnail && thumbnail->data) {
		return __mmcamcorder_capture_save_exifinfo(handle, dest, thumbnail);
	} else {
		return __mmcamcorder_capture_save_exifinfo(handle, dest, NULL);
	}
}


void __mmcamcorder_release_jpeg_data(MMHandleType handle, MMCamcorderCaptureDataType *dest)
{
	int tag_enable = 0;
	int provide_exif = FALSE;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;

	mmf_return_if_fail(hcamcorder);
	mmf_return_if_fail(dest);

	sc = MMF_CAMCORDER_SUBCONTEXT(hcamcorder);
	mmf_return_if_fail(sc);

	_mmcam_dbg_log("");

	__ta__( "                  Get:MMCAM_TAG_ENABLE and \"provide-exif\"",
	mm_camcorder_get_attributes(handle, NULL, MMCAM_TAG_ENABLE, &tag_enable, NULL);
	MMCAMCORDER_G_OBJECT_GET(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst, "provide-exif", &provide_exif);
	);

	/* if dest->data is allocated in MSL, release it */
	if (tag_enable && !provide_exif) {
		if (dest->data) {
			__ta__( "                  Free data",
			free(dest->data);
			);
			dest->length = 0;
			dest->data = NULL;
			_mmcam_dbg_log("Jpeg is released!");
		}
	} else {
		_mmcam_dbg_log("No need to release");
	}

	return;
}


static void __mmcamcorder_image_capture_cb(GstElement *element, GstSample *sample1, GstSample *sample2, GstSample *sample3, gpointer u_data)
{
	int ret = MM_ERROR_NONE;
	int pixtype = MM_PIXEL_FORMAT_INVALID;
	int pixtype_sub = MM_PIXEL_FORMAT_INVALID;
	int codectype = MM_IMAGE_CODEC_JPEG;
	int attr_index = 0;
	int count = 0;
	int stop_cont_shot = FALSE;
	int tag_enable = FALSE;
	int provide_exif = FALSE;
	int capture_quality = 0;
	int try_lock_count = 0;
	gboolean send_captured_message = FALSE;
	unsigned char *exif_raw_data = NULL;

	MMCamcorderStateType current_state = MM_CAMCORDER_STATE_NONE;
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(u_data);
	_MMCamcorderImageInfo *info = NULL;
	_MMCamcorderSubContext *sc = NULL;
	MMCamcorderCaptureDataType dest = {0,};
	MMCamcorderCaptureDataType thumb = {0,};
	MMCamcorderCaptureDataType scrnail = {0,};
    GstMapInfo mapinfo1;
    GstMapInfo mapinfo2;
    GstMapInfo mapinfo3;

	mmf_attrs_t *attrs = NULL;
	mmf_attribute_t *item_screennail = NULL;
	mmf_attribute_t *item_exif_raw_data = NULL;

	mmf_return_if_fail(hcamcorder);

	sc = MMF_CAMCORDER_SUBCONTEXT(hcamcorder);
	mmf_return_if_fail(sc && sc->info_image);

	info = sc->info_image;

	/* get current state */
	current_state = _mmcamcorder_get_state((MMHandleType)hcamcorder);

	_mmcam_dbg_err("START - current state %d", current_state);

	MMTA_ACUM_ITEM_BEGIN("            MSL capture callback", FALSE);

	/* check capture state */
	if (info->type == _MMCamcorder_MULTI_SHOT && info->capture_send_count > 0) {
		mm_camcorder_get_attributes((MMHandleType)hcamcorder, NULL, "capture-break-cont-shot", &stop_cont_shot, NULL);
	}

	if (!info->capturing || stop_cont_shot) {
		_mmcam_dbg_warn("stop command[%d] or not capturing state[%d]. skip this...",
		                stop_cont_shot, info->capturing);

		MMTA_ACUM_ITEM_END("            MSL capture callback", FALSE);

		/*free GstBuffer*/
		if (sample1) {
		    gst_caps_unref(gst_sample_get_caps(sample1));
		    gst_buffer_unref(gst_sample_get_buffer(sample1));
			gst_sample_unref(sample1);
		}
		if (sample2) {
            gst_caps_unref(gst_sample_get_caps(sample2));
            gst_buffer_unref(gst_sample_get_buffer(sample2));
            gst_sample_unref(sample2);
		}
		if (sample3) {
            gst_caps_unref(gst_sample_get_caps(sample3));
            gst_buffer_unref(gst_sample_get_buffer(sample3));
            gst_sample_unref(sample3);
		}

		return;
	}

	/* check command lock to block capture callback if capture start API is not returned
	   wait for 2 seconds at worst case */
	try_lock_count = 0;
	do {
		_mmcam_dbg_log("Try command LOCK");
		if (_MMCAMCORDER_TRYLOCK_CMD(hcamcorder)) {
			_mmcam_dbg_log("command LOCK OK");
			_MMCAMCORDER_UNLOCK_CMD(hcamcorder);
			break;
		}

		if (try_lock_count++ < TRY_LOCK_MAX_COUNT) {
			_mmcam_dbg_warn("command LOCK Failed, retry...[count %d]", try_lock_count);
			usleep(TRY_LOCK_TIME);
		} else {
			_mmcam_dbg_err("failed to lock command LOCK");
			break;
		}
	} while (TRUE);

	if (current_state < MM_CAMCORDER_STATE_RECORDING) {
		/* play capture sound here if multi capture
		   or preview format is ITLV(because of AF and flash control in plugin) */
		if (info->type == _MMCamcorder_MULTI_SHOT) {
			__ta__("                _mmcamcorder_sound_play",
			_mmcamcorder_sound_play((MMHandleType)hcamcorder);
			);
		} else if (!info->played_capture_sound) {
			__ta__("                _mmcamcorder_sound_solo_play",
			_mmcamcorder_sound_solo_play((MMHandleType)hcamcorder, _MMCAMCORDER_FILEPATH_CAPTURE_SND, FALSE);
			);
		}
	}

	/* init capture data */
	memset((void *)&dest, 0x0, sizeof(MMCamcorderCaptureDataType));
	memset((void *)&thumb, 0x0, sizeof(MMCamcorderCaptureDataType));
	memset((void *)&scrnail, 0x0, sizeof(MMCamcorderCaptureDataType));

	/* Prepare main, thumbnail buffer */
    pixtype = _mmcamcorder_get_pixel_format(gst_sample_get_caps(sample1));
	if (pixtype == MM_PIXEL_FORMAT_INVALID) {
		_mmcam_dbg_err("Unsupported pixel type");

		MMCAM_SEND_MESSAGE(hcamcorder, MM_MESSAGE_CAMCORDER_ERROR, MM_ERROR_CAMCORDER_INTERNAL);

		goto error;
	}

	/* Main image buffer */
    gst_buffer_map(gst_sample_get_buffer(sample1), &mapinfo1, GST_MAP_READ);
    if (sample1 && mapinfo1.data && (mapinfo1.size !=0)) {
        __mmcamcorder_get_capture_data_from_buffer(&dest, pixtype, sample1);
	} else {
		_mmcam_dbg_err("Sample1 has wrong pointer. (sample1=%p)",sample1);
		MMCAM_SEND_MESSAGE(hcamcorder, MM_MESSAGE_CAMCORDER_ERROR, MM_ERROR_CAMCORDER_INTERNAL);
        gst_buffer_unmap(gst_sample_get_buffer(sample1), &mapinfo1);
		goto error;
	}

	/* Encode JPEG */
	if (sc->internal_encode) {
#ifdef _MMCAMCORDER_MAKE_THUMBNAIL_INTERNAL_ENCODE
		int thumb_width = 0;
		int thumb_height = 0;
		int thumb_length = 0;
		unsigned char *thumb_raw_data = NULL;
#endif /* _MMCAMCORDER_MAKE_THUMBNAIL_INTERNAL_ENCODE */

		mm_camcorder_get_attributes((MMHandleType)hcamcorder, NULL,
		                            MMCAM_IMAGE_ENCODER_QUALITY, &capture_quality,
		                            NULL);
		_mmcam_dbg_log("Start Internal Encode - capture_quality %d", capture_quality);

		__ta__("                _mmcamcorder_encode_jpeg:MAIN",
        ret = _mmcamcorder_encode_jpeg(mapinfo1.data, dest.width, dest.height,
		                               pixtype, dest.length, capture_quality, &(dest.data), &(dest.length));
		);

        if (!ret) {
			_mmcam_dbg_err("_mmcamcorder_encode_jpeg failed");

			MMCAM_SEND_MESSAGE(hcamcorder, MM_MESSAGE_CAMCORDER_ERROR, MM_ERROR_CAMCORDER_INTERNAL);

			goto error;
		}

		/* set format */
		dest.format = MM_PIXEL_FORMAT_ENCODED;

		_mmcam_dbg_log("Done Internal Encode - data %p, length %d", dest.data, dest.length);

#ifdef _MMCAMCORDER_MAKE_THUMBNAIL_INTERNAL_ENCODE
		/* make thumbnail image */
		_mmcam_dbg_log("Make thumbnail image");

		if (dest.width > THUMBNAIL_WIDTH) {
			float img_ratio = (float)dest.width / (float)dest.height;

			_mmcam_dbg_log("image ratio %f , default ratio %f", img_ratio, THUMBNAIL_DEFAULT_RATIO);

			if (img_ratio > THUMBNAIL_DEFAULT_RATIO) {
				thumb_height = THUMBNAIL_HEIGHT;
				thumb_width = (thumb_height * dest.width) / dest.height;
			} else {
				thumb_width = THUMBNAIL_WIDTH;
				thumb_height = (thumb_width * dest.height) / dest.width;
			}

			_mmcam_dbg_log("need to resize : thumbnail size %dx%d", thumb_width, thumb_height);

            if (!_mmcamcorder_resize_frame(mapinfo1.data, dest.width, dest.height, GST_BUFFER_SIZE(buffer1), pixtype,
			                               &thumb_raw_data, &thumb_width, &thumb_height, &thumb_length)) {
				thumb_raw_data = NULL;
				_mmcam_dbg_warn("_mmcamcorder_resize_frame failed. skip thumbnail making...");
			}
		} else {
			thumb_width = dest.width;
			thumb_height = dest.height;

			_mmcam_dbg_log("NO need to resize : thumbnail size %dx%d", thumb_width, thumb_height);

            thumb_raw_data = mapinfo1.data;
		}

		if (thumb_raw_data) {
			__ta__("                _mmcamcorder_encode_jpeg:THUMBNAIL",
			ret = _mmcamcorder_encode_jpeg(thumb_raw_data, thumb_width, thumb_height,
			                               pixtype, thumb_length, THUMBNAIL_JPEG_QUALITY, &(thumb.data), &(thumb.length));
			);
			if (ret) {
				_mmcam_dbg_log("encode THUMBNAIL done - data %p, length %d", thumb.data, thumb.length);

				thumb.width = thumb_width;
				thumb.height = thumb_height;
				thumb.format = MM_PIXEL_FORMAT_ENCODED;
			} else {
				_mmcam_dbg_warn("failed to encode THUMBNAIL");
			}

            if (thumb_raw_data != mapinfo1.data) {
				free(thumb_raw_data);
				thumb_raw_data = NULL;
				_mmcam_dbg_log("release thumb_raw_data");
			}
		} else {
			_mmcam_dbg_warn("thumb_raw_data is NULL");
		}
#endif /* _MMCAMCORDER_MAKE_THUMBNAIL_INTERNAL_ENCODE */
        gst_buffer_unmap(gst_sample_get_buffer(sample1), &mapinfo1);
	} else {
        gst_buffer_map(gst_sample_get_buffer(sample2), &mapinfo2, GST_MAP_READ);
		/* Thumbnail image buffer */
        if (sample2 && mapinfo2.data && (mapinfo2.size !=0)) {
			_mmcam_dbg_log("Thumnail (sample2=%p)",sample2);

            pixtype_sub = _mmcamcorder_get_pixel_format(gst_sample_get_caps(sample2));
            __mmcamcorder_get_capture_data_from_buffer(&thumb, pixtype_sub, sample2);
		} else {
			_mmcam_dbg_log("Sample2 has wrong pointer. Not Error. (sample2=%p)",sample2);
		}
        gst_buffer_unmap(gst_sample_get_buffer(sample2), &mapinfo2);
	}

	/* Screennail image buffer */
	attrs = (mmf_attrs_t*)MMF_CAMCORDER_ATTRS(hcamcorder);
	mm_attrs_get_index((MMHandleType)attrs, MMCAM_CAPTURED_SCREENNAIL, &attr_index);
	item_screennail = &attrs->items[attr_index];

    gst_buffer_map(gst_sample_get_buffer(sample3), &mapinfo3, GST_MAP_READ);
    if (sample3 && mapinfo3.data && mapinfo3.size != 0) {
        _mmcam_dbg_log("Screennail (sample3=%p,size=%d)", sample3, mapinfo3.size);

        pixtype_sub = _mmcamcorder_get_pixel_format(gst_sample_get_caps(sample3));
        __mmcamcorder_get_capture_data_from_buffer(&scrnail, pixtype_sub, sample3);

		/* Set screennail attribute for application */
		ret = mmf_attribute_set_data(item_screennail, &scrnail, sizeof(scrnail));
		_mmcam_dbg_log("Screennail set attribute data %p, size %d, ret %x", &scrnail, sizeof(scrnail), ret);
	} else {
		_mmcam_dbg_log("Sample3 has wrong pointer. Not Error. (sample3=%p)",sample3);
		mmf_attribute_set_data(item_screennail, NULL, 0);
	}
    gst_buffer_unmap(gst_sample_get_buffer(sample3), &mapinfo3);

	/* commit screennail data */
	mmf_attribute_commit(item_screennail);

	/* create EXIF info */
	__ta__("                    mm_exif_create_exif_info",
	ret = mm_exif_create_exif_info(&(hcamcorder->exif_info));
	);
	if (ret != MM_ERROR_NONE) {
		_mmcam_dbg_err("Failed to create exif_info [%x], but keep going...", ret);
	} else {
		/* add basic exif info */
		_mmcam_dbg_log("add basic exif info");
		__ta__("                    __mmcamcorder_set_exif_basic_info",
		ret = __mmcamcorder_set_exif_basic_info((MMHandleType)hcamcorder, dest.width, dest.height);
		);
		if (ret != MM_ERROR_NONE) {
			_mmcam_dbg_warn("Failed set_exif_basic_info [%x], but keep going...", ret);
			ret = MM_ERROR_NONE;
		}
	}

	/* get attribute item for EXIF data */
	mm_attrs_get_index((MMHandleType)attrs, MMCAM_CAPTURED_EXIF_RAW_DATA, &attr_index);
	item_exif_raw_data = &attrs->items[attr_index];

	/* set EXIF data to attribute */
	if (hcamcorder->exif_info && hcamcorder->exif_info->data) {
		exif_raw_data = (unsigned char *)malloc(hcamcorder->exif_info->size);
		if (exif_raw_data) {
			memcpy(exif_raw_data, hcamcorder->exif_info->data, hcamcorder->exif_info->size);
			mmf_attribute_set_data(item_exif_raw_data, exif_raw_data, hcamcorder->exif_info->size);
			_mmcam_dbg_log("set EXIF raw data %p, size %d", exif_raw_data, hcamcorder->exif_info->size);
		} else {
			_mmcam_dbg_warn("failed to alloc for EXIF, size %d", hcamcorder->exif_info->size);
		}
	} else {
		_mmcam_dbg_warn("failed to create EXIF. set EXIF as NULL");
		mmf_attribute_set_data(item_exif_raw_data, NULL, 0);
	}

	/* commit EXIF data */
	mmf_attribute_commit(item_exif_raw_data);

	/* get tag-enable and provide-exif */
	mm_camcorder_get_attributes((MMHandleType)hcamcorder, NULL, MMCAM_TAG_ENABLE, &tag_enable, NULL);
	MMCAMCORDER_G_OBJECT_GET(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst, "provide-exif", &provide_exif);

	/* Set extra data for JPEG if tag enabled and doesn't provide EXIF */
	if (dest.format == MM_PIXEL_FORMAT_ENCODED &&
	    tag_enable && !provide_exif) {
		mm_camcorder_get_attributes((MMHandleType)hcamcorder, NULL,
		                            MMCAM_IMAGE_ENCODER, &codectype,
		                            NULL);
		_mmcam_dbg_log("codectype %d", codectype);

		switch (codectype) {
		case MM_IMAGE_CODEC_JPEG:
		case MM_IMAGE_CODEC_SRW:
		case MM_IMAGE_CODEC_JPEG_SRW:
			__ta__( "                __mmcamcorder_set_jpeg_data",
			ret = __mmcamcorder_set_jpeg_data((MMHandleType)hcamcorder, &dest, &thumb);
			);
			if (ret != MM_ERROR_NONE) {
				_mmcam_dbg_err("Error on setting extra data to jpeg");
				MMCAM_SEND_MESSAGE(hcamcorder, MM_MESSAGE_CAMCORDER_ERROR, ret);
				goto error;
			}
			break;
		default:
			_mmcam_dbg_err("The codectype is not supported. (%d)", codectype);

			MMCAM_SEND_MESSAGE(hcamcorder, MM_MESSAGE_CAMCORDER_ERROR, MM_ERROR_CAMCORDER_INTERNAL);

			goto error;
		}
	}

	/* Handle Capture Callback */
	_MMCAMCORDER_LOCK_VCAPTURE_CALLBACK(hcamcorder);

	if (hcamcorder->vcapture_cb) {
		_mmcam_dbg_log("APPLICATION CALLBACK START");
		MMTA_ACUM_ITEM_BEGIN("                Application capture callback", 0);
		if (thumb.data) {
			ret = hcamcorder->vcapture_cb(&dest, &thumb, hcamcorder->vcapture_cb_param);
		} else {
			ret = hcamcorder->vcapture_cb(&dest, NULL, hcamcorder->vcapture_cb_param);
		}
		MMTA_ACUM_ITEM_END("                Application capture callback", 0);
		_mmcam_dbg_log("APPLICATION CALLBACK END");
	} else {
		_mmcam_dbg_err("Capture callback is NULL.");

		MMCAM_SEND_MESSAGE(hcamcorder, MM_MESSAGE_CAMCORDER_ERROR, MM_ERROR_CAMCORDER_INVALID_ARGUMENT);

		goto err_release_exif;
	}

	/* Set capture count */
	count = ++(info->capture_send_count);
	send_captured_message = TRUE;

err_release_exif:
	_MMCAMCORDER_UNLOCK_VCAPTURE_CALLBACK(hcamcorder);

	/* init screennail and EXIF raw data */
	__ta__("                init attributes:scrnl and EXIF",
	mmf_attribute_set_data(item_screennail, NULL, 0);
	mmf_attribute_commit(item_screennail);
	if (exif_raw_data) {
		free(exif_raw_data);
		exif_raw_data = NULL;

		mmf_attribute_set_data(item_exif_raw_data, NULL, 0);
		mmf_attribute_commit(item_exif_raw_data);
	}
	);

	/* Release jpeg data */
	if (pixtype == MM_PIXEL_FORMAT_ENCODED) {
		__ta__("                __mmcamcorder_release_jpeg_data",
		__mmcamcorder_release_jpeg_data((MMHandleType)hcamcorder, &dest);
		);
	}

error:
	/* Check end condition and set proper value */
	if (info->hdr_capture_mode != MM_CAMCORDER_HDR_ON_AND_ORIGINAL ||
	    (info->hdr_capture_mode == MM_CAMCORDER_HDR_ON_AND_ORIGINAL && count == 2)) {
		__mmcamcorder_init_stillshot_info((MMHandleType)hcamcorder);
	}

	/* release internal allocated data */
	if (sc->internal_encode) {
		if (dest.data) {
			free(dest.data);
			dest.data = NULL;
			dest.length = 0;
			_mmcam_dbg_log("release internal encoded data MAIN");
		}
		if (thumb.data) {
			free(thumb.data);
			thumb.data = NULL;
			thumb.length = 0;
			_mmcam_dbg_log("release internal encoded data THUMB");
		}
	}

	/*free GstBuffer*/
    if (sample1) {
        gst_caps_unref(gst_sample_get_caps(sample1));
        gst_buffer_unref(gst_sample_get_buffer(sample1));
        gst_sample_unref(sample1);
    }
    if (sample2) {
        gst_caps_unref(gst_sample_get_caps(sample2));
        gst_buffer_unref(gst_sample_get_buffer(sample2));
        gst_sample_unref(sample2);
    }
    if (sample3) {
        gst_caps_unref(gst_sample_get_caps(sample3));
        gst_buffer_unref(gst_sample_get_buffer(sample3));
        gst_sample_unref(sample3);
    }

	/* destroy exif info */
	__ta__("                mm_exif_destory_exif_info",
	mm_exif_destory_exif_info(hcamcorder->exif_info);
	);
	hcamcorder->exif_info = NULL;

	/* send captured message */
	if (send_captured_message) {
		MMTA_ACUM_ITEM_BEGIN("                CAPTURED MESSAGE DELAY", FALSE);

		if (info->hdr_capture_mode != MM_CAMCORDER_HDR_ON_AND_ORIGINAL) {
			MMCamcorderStateType current_state = MM_CAMCORDER_STATE_NONE;

			mm_camcorder_get_state((MMHandleType)hcamcorder, &current_state);

			/* Send CAPTURED message and count - capture success */
			if (current_state >= MM_CAMCORDER_STATE_RECORDING) {
				MMCAM_SEND_MESSAGE(hcamcorder, MM_MESSAGE_CAMCORDER_VIDEO_SNAPSHOT_CAPTURED, count);
			} else {
				MMCAM_SEND_MESSAGE(hcamcorder, MM_MESSAGE_CAMCORDER_CAPTURED, count);
			}
		} else if (info->hdr_capture_mode == MM_CAMCORDER_HDR_ON_AND_ORIGINAL && count == 2) {
			/* send captured message only once in HDR and Original Capture mode */
			MMCAM_SEND_MESSAGE(hcamcorder, MM_MESSAGE_CAMCORDER_CAPTURED, 1);
		}
	}

	MMTA_ACUM_ITEM_END( "            MSL capture callback", FALSE );

	_mmcam_dbg_err("END");

	return;
}


static gboolean __mmcamcorder_encodesink_handoff_callback(GstElement *fakesink, GstBuffer *buffer, GstPad *pad, gpointer u_data)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(u_data);
	_MMCamcorderSubContext *sc = NULL;

	mmf_return_val_if_fail(hcamcorder, FALSE);

	sc = MMF_CAMCORDER_SUBCONTEXT(hcamcorder);
	mmf_return_val_if_fail(sc && sc->element, FALSE);

	_mmcam_dbg_log("");
	/* FIXME. How could you get a thumbnail? */

    __mmcamcorder_image_capture_cb(fakesink, gst_sample_new(buffer, gst_pad_get_current_caps(pad), NULL, NULL), NULL, NULL, u_data);

	if (sc->element[_MMCAMCORDER_ENCSINK_ENCBIN].gst) {
		MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_ENCSINK_SINK].gst, "signal-handoffs", FALSE);
		MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_ENCSINK_ENCBIN].gst, "block", TRUE);
	}

	return TRUE;
}


/* Take a picture with capture mode */
int _mmcamcorder_set_resize_property(MMHandleType handle, int capture_width, int capture_height)
{
	int ELEMENT_CROP = 0;
	int ELEMENT_FILTER = 0;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;

	_mmcam_dbg_log("");

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	mmf_return_val_if_fail(sc && sc->element, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	ELEMENT_CROP = _MMCAMCORDER_ENCSINK_ICROP;
	ELEMENT_FILTER = _MMCAMCORDER_ENCSINK_IFILT;

	/*TODO: this is not needed now. */

	return MM_ERROR_NONE;
}


int __mmcamcorder_set_exif_basic_info(MMHandleType handle, int image_width, int image_height)
{
	int ret = MM_ERROR_NONE;
	int value;
	int str_val_len = 0;
	int gps_enable = TRUE;
	int cntl = 0;
	int cnts = 0;
	double f_latitude = INVALID_GPS_VALUE;
	double f_longitude = INVALID_GPS_VALUE;
	double f_altitude = INVALID_GPS_VALUE;
	char *str_value = NULL;
#ifdef WRITE_EXIF_MAKER_INFO /* FIXME */
	char *maker = NULL;
#endif
	char *user_comment = NULL;
	char *err_name = NULL;
	ExifData *ed = NULL;
	ExifLong config;
	ExifLong ExifVersion;
	static ExifShort eshort[20];
	static ExifLong elong[10];

	GstCameraControl *control = NULL;
	GstCameraControlExifInfo avsys_exif_info;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;

	_mmcam_dbg_log("");

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	mmf_return_val_if_fail(sc && sc->element, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	CLEAR(avsys_exif_info);

	if (!GST_IS_CAMERA_CONTROL(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst)) {
		_mmcam_dbg_err("Can't cast Video source into camera control. Skip camera control values...");
	} else {
		control = GST_CAMERA_CONTROL(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst);
		/* get device information */
		__ta__("                        gst_camera_control_get_exif_info",
		gst_camera_control_get_exif_info(control, &avsys_exif_info);
		);
	}

	/* get ExifData from exif info */
	__ta__("                        mm_exif_get_exif_from_info",
	ed = mm_exif_get_exif_from_info(hcamcorder->exif_info);
	);
	if (ed == NULL || ed->ifd == NULL) {
		_mmcam_dbg_err("get exif data error!!(%p, %p)", ed, (ed ? ed->ifd : NULL));
		return MM_ERROR_INVALID_HANDLE;
	}

	/* Receive attribute info */

	/* START INSERT IFD_0 */

	/*0. EXIF_TAG_EXIF_VERSION */
	ExifVersion = MM_EXIF_VERSION;
	ret = mm_exif_set_add_entry(ed, EXIF_IFD_EXIF, EXIF_TAG_EXIF_VERSION,
	                            EXIF_FORMAT_UNDEFINED, 4, (unsigned char *)&ExifVersion);
	if (ret != MM_ERROR_NONE) {
		EXIF_SET_ERR(ret, EXIF_TAG_EXIF_VERSION);
	}

	/*1. EXIF_TAG_IMAGE_WIDTH */ /*EXIF_TAG_PIXEL_X_DIMENSION*/
	value = image_width;

	exif_set_long((unsigned char *)&elong[cntl], exif_data_get_byte_order(ed), value);
	ret = mm_exif_set_add_entry(ed, EXIF_IFD_0, EXIF_TAG_IMAGE_WIDTH,
	                            EXIF_FORMAT_LONG, 1, (unsigned char *)&elong[cntl]);
	if (ret != MM_ERROR_NONE) {
		EXIF_SET_ERR(ret, EXIF_TAG_IMAGE_WIDTH);
	}

	ret = mm_exif_set_add_entry(ed, EXIF_IFD_EXIF, EXIF_TAG_PIXEL_X_DIMENSION,
	                            EXIF_FORMAT_LONG, 1, (unsigned char *)&elong[cntl++]);
	if (ret != MM_ERROR_NONE) {
		EXIF_SET_ERR(ret, EXIF_TAG_PIXEL_X_DIMENSION);
	}
	_mmcam_dbg_log("width[%d]", value);

	/*2. EXIF_TAG_IMAGE_LENGTH*/ /*EXIF_TAG_PIXEL_Y_DIMENSION*/
	value = image_height;

	exif_set_long((unsigned char *)&elong[cntl], exif_data_get_byte_order (ed), value);
	ret = mm_exif_set_add_entry(ed, EXIF_IFD_0, EXIF_TAG_IMAGE_LENGTH,
	                            EXIF_FORMAT_LONG, 1, (unsigned char *)&elong[cntl]);
	if (ret != MM_ERROR_NONE) {
		EXIF_SET_ERR(ret, EXIF_TAG_IMAGE_LENGTH);
	}

	ret = mm_exif_set_add_entry(ed, EXIF_IFD_EXIF, EXIF_TAG_PIXEL_Y_DIMENSION,
	                            EXIF_FORMAT_LONG, 1, (unsigned char *)&elong[cntl++]);
	if (ret != MM_ERROR_NONE) {
		EXIF_SET_ERR(ret, EXIF_TAG_PIXEL_Y_DIMENSION);
	}
	_mmcam_dbg_log("height[%d]", value);

	/*4. EXIF_TAG_DATE_TIME */

	/*12. EXIF_TAG_DATE_TIME_ORIGINAL */

	/*13. EXIF_TAG_DATE_TIME_DIGITIZED*/
	{
		unsigned char *b;
		time_t t;
		struct tm tm;

		b = malloc(20 * sizeof(unsigned char));
		if (b == NULL) {
			_mmcam_dbg_err("failed to alloc b");
			ret = MM_ERROR_CAMCORDER_LOW_MEMORY;
			EXIF_SET_ERR(ret, EXIF_TAG_DATE_TIME);
		}

		memset(b, '\0', 20);

		t = time(NULL);
		tzset();
		localtime_r(&t, &tm);

		snprintf((char *)b, 20, "%04i:%02i:%02i %02i:%02i:%02i",
		         tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		         tm.tm_hour, tm.tm_min, tm.tm_sec);

		ret = mm_exif_set_add_entry(ed, EXIF_IFD_0, EXIF_TAG_DATE_TIME, EXIF_FORMAT_ASCII, 20, b);
		if (ret != MM_ERROR_NONE) {
			if (ret == MM_ERROR_CAMCORDER_LOW_MEMORY) {
				free(b);
			}
			EXIF_SET_ERR(ret, EXIF_TAG_DATE_TIME);
		}

		ret = mm_exif_set_add_entry(ed, EXIF_IFD_EXIF, EXIF_TAG_DATE_TIME_ORIGINAL, EXIF_FORMAT_ASCII, 20, b);
		if (ret != MM_ERROR_NONE) {
			if (ret == MM_ERROR_CAMCORDER_LOW_MEMORY) {
				free(b);
			}
			EXIF_SET_ERR(ret, EXIF_TAG_DATE_TIME_ORIGINAL);
		}

		ret = mm_exif_set_add_entry(ed, EXIF_IFD_EXIF, EXIF_TAG_DATE_TIME_DIGITIZED, EXIF_FORMAT_ASCII, 20, b);
		if (ret != MM_ERROR_NONE) {
			if (ret == MM_ERROR_CAMCORDER_LOW_MEMORY) {
				free(b);
			}
			EXIF_SET_ERR(ret, EXIF_TAG_DATE_TIME_DIGITIZED);
		}

		free(b);
	}

#ifdef WRITE_EXIF_MAKER_INFO /* FIXME */
	/*5. EXIF_TAG_MAKE */
	maker = strdup(MM_MAKER_NAME);
	if (maker) {
		_mmcam_dbg_log("maker [%s]", maker);
		ret = mm_exif_set_add_entry(ed, EXIF_IFD_0, EXIF_TAG_MAKE,
		                            EXIF_FORMAT_ASCII, strlen(maker), (unsigned char *)maker);
		free(maker);
		if (ret != MM_ERROR_NONE) {
			EXIF_SET_ERR(ret, EXIF_TAG_MAKE);
		}
	} else {
		ret = MM_ERROR_CAMCORDER_LOW_MEMORY;
		EXIF_SET_ERR(ret, EXIF_TAG_MAKE);
	}

	/*6. EXIF_TAG_MODEL */
	_mmcamcorder_conf_get_value_string(hcamcorder->conf_main,
	                                   CONFIGURE_CATEGORY_MAIN_GENERAL,
	                                   "ModelName",
	                                   &str_value);
	_mmcam_dbg_log("model_name [%s]", str_value);
	if (str_value) {
		char *model = strdup(str_value);
		mm_exif_set_add_entry(ed,EXIF_IFD_0,EXIF_TAG_MODEL,EXIF_FORMAT_ASCII,strlen(model)+1, (unsigned char*)model);
		free(model);
		str_value = NULL;
		if (ret != MM_ERROR_NONE) {
			EXIF_SET_ERR(ret, EXIF_TAG_MODEL);
		}
	} else {
		_mmcam_dbg_warn("failed to get model name");
	}
#endif
	/*6. EXIF_TAG_IMAGE_DESCRIPTION */
	mm_camcorder_get_attributes(handle, NULL, MMCAM_TAG_IMAGE_DESCRIPTION, &str_value, &str_val_len, NULL);
	_mmcam_dbg_log("desctiption [%s]", str_value);
	if (str_value && str_val_len > 0) {
		char *description = strdup(str_value);
		ret = mm_exif_set_add_entry(ed, EXIF_IFD_0, EXIF_TAG_IMAGE_DESCRIPTION,
		                            EXIF_FORMAT_ASCII, strlen(description), (unsigned char *)description);
		free(description);
		str_value = NULL;
		str_val_len = 0;
		if (ret != MM_ERROR_NONE) {
			EXIF_SET_ERR(ret, EXIF_TAG_IMAGE_DESCRIPTION);
		}
	} else {
		_mmcam_dbg_warn("failed to get description");
	}

	/*7. EXIF_TAG_SOFTWARE*/
/*
	if (control != NULL) {
		char software[50] = {0,};
		unsigned int len = 0;

		len = snprintf(software, sizeof(software), "%x.%x ", avsys_exif_info.software_used>>8,(avsys_exif_info.software_used & 0xff));
		_mmcam_dbg_log("software [%s], len [%d]", software, len);
		ret = mm_exif_set_add_entry(ed, EXIF_IFD_0, EXIF_TAG_SOFTWARE,
		                            EXIF_FORMAT_ASCII, len, software);
		if (ret != MM_ERROR_NONE) {
			EXIF_SET_ERR(ret, EXIF_TAG_SOFTWARE);
		}
	}
*/

	/*8. EXIF_TAG_ORIENTATION */
	mm_camcorder_get_attributes(handle, NULL, MMCAM_TAG_ORIENTATION, &value, NULL);
	_mmcam_dbg_log("get orientation [%d]",value);
	if (value == 0) {
		value = MM_EXIF_ORIENTATION;
	}

	exif_set_short((unsigned char *)&eshort[cnts], exif_data_get_byte_order(ed), value);
	ret = mm_exif_set_add_entry(ed, EXIF_IFD_0, EXIF_TAG_ORIENTATION,
	                            EXIF_FORMAT_SHORT, 1, (unsigned char*)&eshort[cnts++]);
	if (ret != MM_ERROR_NONE) {
		EXIF_SET_ERR(ret, EXIF_TAG_ORIENTATION);
	}

	/* START INSERT EXIF_IFD */

	/*3. User Comment*/
	/*FIXME : get user comment from real user */
	user_comment = strdup(MM_USER_COMMENT);
	if (user_comment) {
		_mmcam_dbg_log("user_comment=%s",user_comment);
		ret = mm_exif_set_add_entry(ed, EXIF_IFD_EXIF, EXIF_TAG_USER_COMMENT,
		                            EXIF_FORMAT_ASCII, strlen(user_comment), (unsigned char *)user_comment);
		free(user_comment);
		if (ret != MM_ERROR_NONE) {
			EXIF_SET_ERR(ret, EXIF_TAG_USER_COMMENT);
		}
	} else {
		ret = MM_ERROR_CAMCORDER_LOW_MEMORY;
		EXIF_SET_ERR(ret, EXIF_TAG_USER_COMMENT);
	}

	/*9. EXIF_TAG_COLOR_SPACE */
	if (control != NULL) {
		exif_set_short((unsigned char *)&eshort[cnts], exif_data_get_byte_order(ed), avsys_exif_info.colorspace);
		ret = mm_exif_set_add_entry(ed, EXIF_IFD_EXIF, EXIF_TAG_COLOR_SPACE,
		                            EXIF_FORMAT_SHORT, 1, (unsigned char *)&eshort[cnts++]);
		if (ret != MM_ERROR_NONE) {
			EXIF_SET_ERR(ret, EXIF_TAG_COLOR_SPACE);
		}
	}

	/*10. EXIF_TAG_COMPONENTS_CONFIGURATION */
	if (control != NULL) {
		config = avsys_exif_info.component_configuration;
		_mmcam_dbg_log("EXIF_TAG_COMPONENTS_CONFIGURATION [%4x] ",config);
		ret = mm_exif_set_add_entry(ed, EXIF_IFD_EXIF, EXIF_TAG_COMPONENTS_CONFIGURATION,
		                            EXIF_FORMAT_UNDEFINED, 4, (unsigned char *)&config);
		if (ret != MM_ERROR_NONE) {
			EXIF_SET_ERR(ret, EXIF_TAG_COMPONENTS_CONFIGURATION);
		}
	}

	/*11. EXIF_TAG_COMPRESSED_BITS_PER_PIXEL */
	/* written  the device_info */

	/*12. EXIF_TAG_DATE_TIME_ORIGINAL */
	/*13. EXIF_TAG_DATE_TIME_DIGITIZED*/

	/*14. EXIF_TAG_EXPOSURE_TIME*/
	if (avsys_exif_info.exposure_time_numerator && avsys_exif_info.exposure_time_denominator) {
		unsigned char *b = NULL;
		ExifRational rData;

		_mmcam_dbg_log("EXIF_TAG_EXPOSURE_TIME numerator [%d], denominator [%d]",
		               avsys_exif_info.exposure_time_numerator, avsys_exif_info.exposure_time_denominator)

		b = malloc(sizeof(ExifRational));
		if (b) {
			rData.numerator = avsys_exif_info.exposure_time_numerator;
			rData.denominator = avsys_exif_info.exposure_time_denominator;

			exif_set_rational(b, exif_data_get_byte_order(ed), rData);
			ret = mm_exif_set_add_entry(ed, EXIF_IFD_EXIF, EXIF_TAG_EXPOSURE_TIME,
			                            EXIF_FORMAT_RATIONAL, 1, b);
			free(b);
			if (ret != MM_ERROR_NONE) {
				EXIF_SET_ERR(ret, EXIF_TAG_EXPOSURE_TIME);
			}
		} else {
			_mmcam_dbg_warn("malloc failed.");
		}
	} else {
		_mmcam_dbg_log("Skip set EXIF_TAG_EXPOSURE_TIME numerator [%d], denominator [%d]",
		               avsys_exif_info.exposure_time_numerator, avsys_exif_info.exposure_time_denominator);
	}

	/*15. EXIF_TAG_FNUMBER */
	if (avsys_exif_info.aperture_f_num_numerator && avsys_exif_info.aperture_f_num_denominator) {
		unsigned char *b = NULL;
		ExifRational rData;

		_mmcam_dbg_log("EXIF_TAG_FNUMBER numerator [%d], denominator [%d]",
		               avsys_exif_info.aperture_f_num_numerator, avsys_exif_info.aperture_f_num_denominator);

		b = malloc(sizeof(ExifRational));
		if (b) {
			rData.numerator = avsys_exif_info.aperture_f_num_numerator;
			rData.denominator = avsys_exif_info.aperture_f_num_denominator;
			exif_set_rational(b, exif_data_get_byte_order(ed), rData);
			ret = mm_exif_set_add_entry(ed, EXIF_IFD_EXIF, EXIF_TAG_FNUMBER,
			                            EXIF_FORMAT_RATIONAL, 1, b);
			free(b);
			if(ret != MM_ERROR_NONE) {
				EXIF_SET_ERR(ret, EXIF_TAG_FNUMBER);
			}
		} else {
			_mmcam_dbg_warn( "malloc failed." );
		}
	} else {
		_mmcam_dbg_log("Skip set EXIF_TAG_FNUMBER numerator [%d], denominator [%d]",
		               avsys_exif_info.aperture_f_num_numerator, avsys_exif_info.aperture_f_num_denominator);
	}

	/*16. EXIF_TAG_EXPOSURE_PROGRAM*/
	/*FIXME*/
	value = MM_EXPOSURE_PROGRAM;
	exif_set_short((unsigned char *)&eshort[cnts], exif_data_get_byte_order(ed), value);
	ret = mm_exif_set_add_entry(ed, EXIF_IFD_EXIF, EXIF_TAG_EXPOSURE_PROGRAM,
	                            EXIF_FORMAT_SHORT, 1, (unsigned char *)&eshort[cnts++]);
	if (ret != MM_ERROR_NONE) {
		EXIF_SET_ERR(ret, EXIF_TAG_EXPOSURE_PROGRAM);
	}

	/*17. EXIF_TAG_ISO_SPEED_RATINGS*/
	if (avsys_exif_info.iso) {
		_mmcam_dbg_log("EXIF_TAG_ISO_SPEED_RATINGS [%d]", avsys_exif_info.iso);
		exif_set_short((unsigned char *)&eshort[cnts], exif_data_get_byte_order(ed), avsys_exif_info.iso);
		ret = mm_exif_set_add_entry(ed, EXIF_IFD_EXIF, EXIF_TAG_ISO_SPEED_RATINGS,
		                            EXIF_FORMAT_SHORT, 1, (unsigned char *)&eshort[cnts++]);
		if (ret != MM_ERROR_NONE) {
			EXIF_SET_ERR(ret, EXIF_TAG_ISO_SPEED_RATINGS);
		}
	}

	/*18. EXIF_TAG_SHUTTER_SPEED_VALUE*/
	if (avsys_exif_info.shutter_speed_numerator && avsys_exif_info.shutter_speed_denominator) {
		unsigned char *b = NULL;
		ExifSRational rsData;

		_mmcam_dbg_log("EXIF_TAG_SHUTTER_SPEED_VALUE numerator [%d], denominator [%d]",
		               avsys_exif_info.shutter_speed_numerator, avsys_exif_info.shutter_speed_denominator);

		b = malloc(sizeof(ExifSRational));
		if (b) {
			rsData.numerator = avsys_exif_info.shutter_speed_numerator;
			rsData.denominator = avsys_exif_info.shutter_speed_denominator;
			exif_set_srational(b, exif_data_get_byte_order(ed), rsData);
			ret = mm_exif_set_add_entry(ed, EXIF_IFD_EXIF, EXIF_TAG_SHUTTER_SPEED_VALUE,
			                            EXIF_FORMAT_SRATIONAL, 1, b);
			free(b);
			if (ret != MM_ERROR_NONE) {
				EXIF_SET_ERR(ret, EXIF_TAG_SHUTTER_SPEED_VALUE);
			}
		} else {
			_mmcam_dbg_warn("malloc failed.");
		}
	} else {
		_mmcam_dbg_log("Skip set EXIF_TAG_SHUTTER_SPEED_VALUE numerator [%d], denominator [%d]",
		               avsys_exif_info.shutter_speed_numerator, avsys_exif_info.shutter_speed_denominator);
	}

	/*19. EXIF_TAG_APERTURE_VALUE*/
	if (avsys_exif_info.aperture_in_APEX) {
		unsigned char *b = NULL;
		ExifRational rData;

		_mmcam_dbg_log("EXIF_TAG_APERTURE_VALUE [%d]", avsys_exif_info.aperture_in_APEX);

		b = malloc(sizeof(ExifRational));
		if (b) {
			rData.numerator = avsys_exif_info.aperture_in_APEX;
			rData.denominator = 1;
			exif_set_rational(b, exif_data_get_byte_order(ed), rData);
			ret = mm_exif_set_add_entry(ed, EXIF_IFD_EXIF, EXIF_TAG_APERTURE_VALUE,
			                            EXIF_FORMAT_RATIONAL, 1, b);
			free(b);
			if(ret != MM_ERROR_NONE) {
				EXIF_SET_ERR(ret, EXIF_TAG_APERTURE_VALUE);
			}
		} else {
			_mmcam_dbg_warn("malloc failed.");
		}
	} else {
		_mmcam_dbg_log("Skip set EXIF_TAG_APERTURE_VALUE [%d]", avsys_exif_info.aperture_in_APEX);
	}

	/*20. EXIF_TAG_BRIGHTNESS_VALUE*/
	if (avsys_exif_info.brigtness_numerator && avsys_exif_info.brightness_denominator) {
		unsigned char *b = NULL;
		ExifSRational rsData;

		_mmcam_dbg_log("EXIF_TAG_BRIGHTNESS_VALUE numerator [%d], denominator [%d]",
		               avsys_exif_info.brigtness_numerator, avsys_exif_info.brightness_denominator);

		b = malloc(sizeof(ExifSRational));
		if (b) {
			rsData.numerator = avsys_exif_info.brigtness_numerator;
			rsData.denominator = avsys_exif_info.brightness_denominator;
			exif_set_srational(b, exif_data_get_byte_order(ed), rsData);
			ret = mm_exif_set_add_entry(ed, EXIF_IFD_EXIF, EXIF_TAG_BRIGHTNESS_VALUE,
			                            EXIF_FORMAT_SRATIONAL, 1, b);
			free(b);
			if (ret != MM_ERROR_NONE) {
				EXIF_SET_ERR(ret, EXIF_TAG_BRIGHTNESS_VALUE);
			}
		} else {
			_mmcam_dbg_warn( "malloc failed." );
		}
	} else {
		_mmcam_dbg_log("Skip set EXIF_TAG_BRIGHTNESS_VALUE numerator [%d], denominatorr [%d]",
		               avsys_exif_info.brigtness_numerator, avsys_exif_info.brightness_denominator);
	}

	/*21. EXIF_TAG_EXPOSURE_BIAS_VALUE*/
	value = 0;
	MMCamAttrsInfo info;
	mm_camcorder_get_attribute_info(handle, MMCAM_FILTER_BRIGHTNESS, &info);

	ret = mm_camcorder_get_attributes(handle, NULL, MMCAM_FILTER_BRIGHTNESS, &value, NULL);
	if (ret == MM_ERROR_NONE) {
		unsigned char *b = NULL;
		ExifSRational rsData;

		_mmcam_dbg_log("EXIF_TAG_BRIGHTNESS_VALUE %d",value);

		b = malloc(sizeof(ExifSRational));
		if (b) {
			if( info.int_range.min < info.int_range.max) {
				rsData.numerator = (value - 4) * 5;
				rsData.denominator = 10;
			}
			else {
				rsData.numerator = 0;
				rsData.denominator = 10;
			}
			exif_set_srational(b, exif_data_get_byte_order(ed), rsData);
			ret = mm_exif_set_add_entry(ed, EXIF_IFD_EXIF, EXIF_TAG_EXPOSURE_BIAS_VALUE,
			                            EXIF_FORMAT_SRATIONAL, 1, b);
			free(b);
			if (ret != MM_ERROR_NONE) {
				EXIF_SET_ERR(ret, EXIF_TAG_EXPOSURE_BIAS_VALUE);
			}
		} else {
			_mmcam_dbg_warn("malloc failed.");
		}
	} else {
		_mmcam_dbg_log("failed to get MMCAM_FILTER_BRIGHTNESS [%x]", ret);
	}

	/*22  EXIF_TAG_MAX_APERTURE_VALUE*/
/*
	if (avsys_exif_info.max_lens_aperture_in_APEX) {
		unsigned char *b = NULL;
		ExifRational rData;

		_mmcam_dbg_log("EXIF_TAG_MAX_APERTURE_VALUE [%d]", avsys_exif_info.max_lens_aperture_in_APEX);

		b = malloc(sizeof(ExifRational));
		if (b) {
			rData.numerator = avsys_exif_info.max_lens_aperture_in_APEX;
			rData.denominator = 1;
			exif_set_rational(b, exif_data_get_byte_order(ed), rData);
			ret = mm_exif_set_add_entry(ed, EXIF_IFD_EXIF, EXIF_TAG_MAX_APERTURE_VALUE,
			                            EXIF_FORMAT_RATIONAL, 1, b);
			free(b);
			if (ret != MM_ERROR_NONE) {
				EXIF_SET_ERR(ret, EXIF_TAG_MAX_APERTURE_VALUE);
			}
		} else {
			_mmcam_dbg_warn("failed to alloc for MAX aperture value");
		}
	}
*/

	/*23. EXIF_TAG_SUBJECT_DISTANCE*/
	/* defualt : none */

	/*24. EXIF_TAG_METERING_MODE */
	if (control != NULL) {
		exif_set_short((unsigned char *)&eshort[cnts], exif_data_get_byte_order(ed),avsys_exif_info.metering_mode);
		_mmcam_dbg_log("EXIF_TAG_METERING_MODE [%d]", avsys_exif_info.metering_mode);
		ret = mm_exif_set_add_entry(ed, EXIF_IFD_EXIF, EXIF_TAG_METERING_MODE,
		                            EXIF_FORMAT_SHORT, 1, (unsigned char *)&eshort[cnts++]);
		if (ret != MM_ERROR_NONE) {
			EXIF_SET_ERR(ret, EXIF_TAG_METERING_MODE);
		}
	}

	/*25. EXIF_TAG_LIGHT_SOURCE*/

	/*26. EXIF_TAG_FLASH*/
	if (control != NULL) {
		exif_set_short((unsigned char *)&eshort[cnts], exif_data_get_byte_order (ed),avsys_exif_info.flash);
		_mmcam_dbg_log("EXIF_TAG_FLASH [%d]", avsys_exif_info.flash);
		ret = mm_exif_set_add_entry(ed, EXIF_IFD_EXIF, EXIF_TAG_FLASH,
		                            EXIF_FORMAT_SHORT, 1, (unsigned char *)&eshort[cnts++]);
		if (ret != MM_ERROR_NONE) {
			EXIF_SET_ERR(ret, EXIF_TAG_FLASH);
		}
	}

	/*27. EXIF_TAG_FOCAL_LENGTH*/
	if (avsys_exif_info.focal_len_numerator && avsys_exif_info.focal_len_denominator) {
		unsigned char *b = NULL;
		ExifRational rData;

		_mmcam_dbg_log("EXIF_TAG_FOCAL_LENGTH numerator [%d], denominator [%d]",
		               avsys_exif_info.focal_len_numerator, avsys_exif_info.focal_len_denominator);

		b = malloc(sizeof(ExifRational));
		if (b) {
			rData.numerator = avsys_exif_info.focal_len_numerator;
			rData.denominator = avsys_exif_info.focal_len_denominator;
			exif_set_rational(b, exif_data_get_byte_order(ed), rData);
			ret = mm_exif_set_add_entry(ed, EXIF_IFD_EXIF, EXIF_TAG_FOCAL_LENGTH,
			                            EXIF_FORMAT_RATIONAL, 1, b);
			free(b);
			if (ret != MM_ERROR_NONE) {
				EXIF_SET_ERR(ret, EXIF_TAG_FOCAL_LENGTH);
			}
		} else {
			_mmcam_dbg_warn("malloc failed.");
		}
	} else {
		_mmcam_dbg_log("Skip set EXIF_TAG_FOCAL_LENGTH numerator [%d], denominator [%d]",
		               avsys_exif_info.focal_len_numerator, avsys_exif_info.focal_len_denominator);
	}

	/*28. EXIF_TAG_SENSING_METHOD*/
	/*FIXME*/
	value = MM_SENSING_MODE;
	exif_set_short((unsigned char *)&eshort[cnts], exif_data_get_byte_order (ed),value);
	ret = mm_exif_set_add_entry(ed, EXIF_IFD_EXIF, EXIF_TAG_SENSING_METHOD,
	                            EXIF_FORMAT_SHORT, 1, (unsigned char *)&eshort[cnts++]);
	if (ret != MM_ERROR_NONE) {
		EXIF_SET_ERR(ret, EXIF_TAG_SENSING_METHOD);
	}

	/*29. EXIF_TAG_FILE_SOURCE*/
/*
	value = MM_FILE_SOURCE;
	exif_set_long(&elong[cntl], exif_data_get_byte_order(ed),value);
	ret = mm_exif_set_add_entry(ed, EXIF_IFD_EXIF, EXIF_TAG_FILE_SOURCE,
	                            EXIF_FORMAT_UNDEFINED, 4, (unsigned char *)&elong[cntl++]);
	if (ret != MM_ERROR_NONE) {
		EXIF_SET_ERR(ret, EXIF_TAG_FILE_SOURCE);
	}
*/

	/*30. EXIF_TAG_SCENE_TYPE*/
/*
	value = MM_SCENE_TYPE;
	exif_set_long(&elong[cntl], exif_data_get_byte_order(ed),value);
	ret = mm_exif_set_add_entry(ed, EXIF_IFD_EXIF, EXIF_TAG_SCENE_TYPE,
	                            EXIF_FORMAT_UNDEFINED, 4, (unsigned char *)&elong[cntl++]);
	if (ret != MM_ERROR_NONE) {
		EXIF_SET_ERR(ret, EXIF_TAG_SCENE_TYPE);
	}
*/

	/*31. EXIF_TAG_EXPOSURE_MODE*/
	/*FIXME*/
	value = MM_EXPOSURE_MODE;
	exif_set_short((unsigned char *)&eshort[cnts], exif_data_get_byte_order(ed),value);
	ret = mm_exif_set_add_entry(ed, EXIF_IFD_EXIF, EXIF_TAG_EXPOSURE_MODE,
	                            EXIF_FORMAT_SHORT, 1, (unsigned char *)&eshort[cnts++]);
	if (ret != MM_ERROR_NONE) {
		EXIF_SET_ERR(ret, EXIF_TAG_EXPOSURE_MODE);
	}


	/*32. EXIF_TAG_WHITE_BALANCE*/
	ret = mm_camcorder_get_attributes(handle, NULL, MMCAM_FILTER_WB, &value, NULL);
	if (ret == MM_ERROR_NONE) {
		int set_value = 0;
		_mmcam_dbg_log("WHITE BALANCE [%d]", value);
	
		if (value == MM_CAMCORDER_WHITE_BALANCE_AUTOMATIC) {
			set_value = 0;
		} else {
			set_value = 1;
		}

		exif_set_short((unsigned char *)&eshort[cnts], exif_data_get_byte_order(ed), set_value);
		ret = mm_exif_set_add_entry(ed, EXIF_IFD_EXIF, EXIF_TAG_WHITE_BALANCE,
		                            EXIF_FORMAT_SHORT, 1, (unsigned char *)&eshort[cnts++]);
		if (ret != MM_ERROR_NONE) {
			EXIF_SET_ERR(ret, EXIF_TAG_WHITE_BALANCE);
		}
	} else {
		_mmcam_dbg_warn("failed to get white balance [%x]", ret);
	}

	/*33. EXIF_TAG_DIGITAL_ZOOM_RATIO*/
/*
	ret = mm_camcorder_get_attributes(handle, NULL, MMCAM_CAMERA_DIGITAL_ZOOM, &value, NULL);
	if (ret == MM_ERROR_NONE) {
		_mmcam_dbg_log("DIGITAL ZOOM [%d]", value);

		exif_set_long(&elong[cntl], exif_data_get_byte_order(ed), value);
		ret = mm_exif_set_add_entry(ed, EXIF_IFD_EXIF, EXIF_TAG_DIGITAL_ZOOM_RATIO,
		                            EXIF_FORMAT_LONG, 1, (unsigned char *)&elong[cntl++]);
		if (ret != MM_ERROR_NONE) {
			EXIF_SET_ERR(ret, EXIF_TAG_DIGITAL_ZOOM_RATIO);
		}
	} else {
		_mmcam_dbg_warn("failed to get digital zoom [%x]", ret);
	}
*/

	/*34. EXIF_TAG_FOCAL_LENGTH_IN_35MM_FILM*/
	/*FIXME*/
/*
	value = MM_FOCAL_LENGTH_35MMFILM;
	exif_set_short(&eshort[cnts], exif_data_get_byte_order(ed),value);
	ret = mm_exif_set_add_entry(ed, EXIF_IFD_EXIF, EXIF_TAG_FOCAL_LENGTH_IN_35MM_FILM,
	                            EXIF_FORMAT_SHORT, 1, (unsigned char *)&eshort[cnts++]);
	if (ret != MM_ERROR_NONE) {
		EXIF_SET_ERR(ret, EXIF_TAG_FOCAL_LENGTH_IN_35MM_FILM);
	}
*/

	/*35. EXIF_TAG_SCENE_CAPTURE_TYPE*/
	{
		int scene_capture_type;

		ret = mm_camcorder_get_attributes(handle, NULL, MMCAM_FILTER_SCENE_MODE, &value, NULL);
		if (ret == MM_ERROR_NONE) {
			_mmcam_dbg_log("Scene mode(program mode) [%d]", value);

			if (value == MM_CAMCORDER_SCENE_MODE_NORMAL) {
				scene_capture_type = 0; /* standard */
			} else if (value == MM_CAMCORDER_SCENE_MODE_PORTRAIT) {
				scene_capture_type = 2; /* portrait */
			} else if (value == MM_CAMCORDER_SCENE_MODE_LANDSCAPE) {
				scene_capture_type = 1; /* landscape */
			} else if (value == MM_CAMCORDER_SCENE_MODE_NIGHT_SCENE) {
				scene_capture_type = 3; /* night scene */
			} else {
				scene_capture_type = 4; /* Others */
			}

			exif_set_short((unsigned char *)&eshort[cnts], exif_data_get_byte_order(ed), scene_capture_type);
			ret = mm_exif_set_add_entry(ed, EXIF_IFD_EXIF, EXIF_TAG_SCENE_CAPTURE_TYPE,
			                            EXIF_FORMAT_SHORT, 1, (unsigned char *)&eshort[cnts++]);
			if (ret != MM_ERROR_NONE) {
				EXIF_SET_ERR(ret, EXIF_TAG_SCENE_CAPTURE_TYPE);
			}
		} else {
			_mmcam_dbg_warn("failed to get scene mode [%x]", ret);
		}
	}

	/*36. EXIF_TAG_GAIN_CONTROL*/
	/*FIXME*/
/*
	value = MM_GAIN_CONTROL;
	exif_set_long(&elong[cntl], exif_data_get_byte_order(ed), value);
	ret = mm_exif_set_add_entry(ed, EXIF_IFD_EXIF, EXIF_TAG_GAIN_CONTROL,
	                            EXIF_FORMAT_LONG, 1, (unsigned char *)&elong[cntl++]);
	if (ret != MM_ERROR_NONE) {
		EXIF_SET_ERR(ret, EXIF_TAG_GAIN_CONTROL);
	}
*/


	/*37. EXIF_TAG_CONTRAST */
	{
		type_int_range *irange = NULL;
		int level = 0;

		_mmcamcorder_conf_get_value_int_range(hcamcorder->conf_ctrl,
		                                      CONFIGURE_CATEGORY_CTRL_EFFECT,
		                                      "Contrast",
		                                      &irange);
		if (irange != NULL) {
			mm_camcorder_get_attributes(handle, NULL, MMCAM_FILTER_CONTRAST, &value, NULL);

			_mmcam_dbg_log("CONTRAST currentt [%d], default [%d]", value, irange->default_value);

			if (value == irange->default_value) {
				level = MM_VALUE_NORMAL;
			} else if (value < irange->default_value) {
				level = MM_VALUE_LOW;
			} else {
				level = MM_VALUE_HARD;
			}

			exif_set_short((unsigned char *)&eshort[cnts], exif_data_get_byte_order(ed), level);
			ret = mm_exif_set_add_entry(ed, EXIF_IFD_EXIF, EXIF_TAG_CONTRAST,
			                            EXIF_FORMAT_SHORT, 1, (unsigned char *)&eshort[cnts++]);
			if (ret != MM_ERROR_NONE) {
				EXIF_SET_ERR(ret, EXIF_TAG_CONTRAST);
			}
		} else {
			_mmcam_dbg_warn("failed to get range of contrast");
		}
	}

	/*38. EXIF_TAG_SATURATION*/
	{
		type_int_range *irange = NULL;
		int level = 0;

		_mmcamcorder_conf_get_value_int_range(hcamcorder->conf_ctrl,
		                                      CONFIGURE_CATEGORY_CTRL_EFFECT,
		                                      "Saturation",
		                                      &irange);
		if (irange != NULL) {
			mm_camcorder_get_attributes(handle, NULL, MMCAM_FILTER_SATURATION, &value, NULL);

			_mmcam_dbg_log("SATURATION current [%d], default [%d]", value, irange->default_value);

			if (value == irange->default_value) {
				level = MM_VALUE_NORMAL;
			} else if (value < irange->default_value) {
				level = MM_VALUE_LOW;
			} else {
				level=MM_VALUE_HARD;
			}

			exif_set_short((unsigned char *)&eshort[cnts], exif_data_get_byte_order(ed), level);
			ret = mm_exif_set_add_entry(ed, EXIF_IFD_EXIF, EXIF_TAG_SATURATION,
			                            EXIF_FORMAT_SHORT, 1, (unsigned char *)&eshort[cnts++]);
			if (ret != MM_ERROR_NONE) {
				EXIF_SET_ERR(ret, EXIF_TAG_SATURATION);
			}
		} else {
			_mmcam_dbg_warn("failed to get range of saturation");
		}
	}

	/*39. EXIF_TAG_SHARPNESS*/
	{
		type_int_range *irange = NULL;
		int level = 0;

		_mmcamcorder_conf_get_value_int_range(hcamcorder->conf_ctrl,
		                                      CONFIGURE_CATEGORY_CTRL_EFFECT,
		                                      "Sharpness",
		                                      &irange);
		if (irange != NULL) {
			mm_camcorder_get_attributes(handle, NULL, MMCAM_FILTER_SHARPNESS, &value, NULL);

			_mmcam_dbg_log("SHARPNESS current [%d], default [%d]", value, irange->default_value);

			if (value == irange->default_value) {
				level = MM_VALUE_NORMAL;
			} else if (value < irange->default_value) {
				level = MM_VALUE_LOW;
			} else {
				level = MM_VALUE_HARD;
			}

			exif_set_short((unsigned char *)&eshort[cnts], exif_data_get_byte_order(ed), level);
			ret = mm_exif_set_add_entry(ed, EXIF_IFD_EXIF, EXIF_TAG_SHARPNESS,
			                            EXIF_FORMAT_SHORT, 1, (unsigned char *)&eshort[cnts++]);
			if (ret != MM_ERROR_NONE) {
				EXIF_SET_ERR(ret, EXIF_TAG_SHARPNESS);
			}
		} else {
			_mmcam_dbg_warn("failed to get range of sharpness");
		}
	}

	/*40. EXIF_TAG_SUBJECT_DISTANCE_RANGE*/
	/*FIXME*/
	value = MM_SUBJECT_DISTANCE_RANGE;
	_mmcam_dbg_log("DISTANCE_RANGE [%d]", value);
	exif_set_short((unsigned char *)&eshort[cnts], exif_data_get_byte_order(ed), value);
	ret = mm_exif_set_add_entry(ed, EXIF_IFD_EXIF, EXIF_TAG_SUBJECT_DISTANCE_RANGE,
	                            EXIF_FORMAT_SHORT, 1, (unsigned char *)&eshort[cnts++]);
	if (ret != MM_ERROR_NONE) {
		EXIF_SET_ERR(ret, EXIF_TAG_SUBJECT_DISTANCE_RANGE);
	}

	/* GPS information */
	ret = mm_camcorder_get_attributes(handle, NULL, MMCAM_TAG_GPS_ENABLE, &gps_enable, NULL);
	if (ret == MM_ERROR_NONE && gps_enable) {
		ExifByte GpsVersion[4]={2,2,0,0};

		_mmcam_dbg_log("Tag for GPS is ENABLED.");

		ret = mm_exif_set_add_entry(ed, EXIF_IFD_GPS, EXIF_TAG_GPS_VERSION_ID,
		                            EXIF_FORMAT_BYTE, 4, (unsigned char *)&GpsVersion);
		if (ret != MM_ERROR_NONE) {
			EXIF_SET_ERR(ret, EXIF_TAG_GPS_VERSION_ID);
		}

		/*41. Latitude*/
		ret = mm_camcorder_get_attributes(handle, &err_name,
		                                  MMCAM_TAG_LATITUDE, &f_latitude,
		                                  MMCAM_TAG_LONGITUDE, &f_longitude,
		                                  MMCAM_TAG_ALTITUDE, &f_altitude, NULL);
		if (ret != MM_ERROR_NONE) {
			_mmcam_dbg_err("failed to get gps info [%x][%s]", ret, err_name);
			SAFE_FREE(err_name);
			goto exit;
		}

		_mmcam_dbg_log("f_latitude [%f]", f_latitude);
		if (f_latitude != INVALID_GPS_VALUE) {
			unsigned char *b = NULL;
			unsigned int deg;
			unsigned int min;
			unsigned int sec;
			ExifRational rData;

			if (f_latitude < 0) {
				ret = mm_exif_set_add_entry(ed, EXIF_IFD_GPS, EXIF_TAG_GPS_LATITUDE_REF,
				                            EXIF_FORMAT_ASCII, 2, (unsigned char *)"S");
				if (ret != MM_ERROR_NONE) {
					EXIF_SET_ERR(ret, EXIF_TAG_GPS_LATITUDE_REF);
				}
				f_latitude = -f_latitude;
			} else if (f_latitude > 0) {
				ret = mm_exif_set_add_entry(ed, EXIF_IFD_GPS, EXIF_TAG_GPS_LATITUDE_REF,
				                            EXIF_FORMAT_ASCII, 2, (unsigned char *)"N");
				if (ret != MM_ERROR_NONE) {
					EXIF_SET_ERR(ret, EXIF_TAG_GPS_LATITUDE_REF);
				}
			}

			deg = (unsigned int)(f_latitude);
			min = (unsigned int)((f_latitude-deg)*60);
			sec = (unsigned int)(((f_latitude-deg)*3600)-min*60);

			_mmcam_dbg_log("f_latitude deg[%d], min[%d], sec[%d]", deg, min, sec);
			b = malloc(3 * sizeof(ExifRational));
			if (b) {
				rData.numerator = deg;
				rData.denominator = 1;
				exif_set_rational(b, exif_data_get_byte_order(ed), rData);
				rData.numerator = min;
				exif_set_rational(b+8, exif_data_get_byte_order(ed), rData);
				rData.numerator = sec;
				exif_set_rational(b+16, exif_data_get_byte_order(ed), rData);

				ret = mm_exif_set_add_entry(ed, EXIF_IFD_GPS, EXIF_TAG_GPS_LATITUDE,
				                            EXIF_FORMAT_RATIONAL, 3, (unsigned char *)b);
				free(b);
				if (ret != MM_ERROR_NONE) {
					EXIF_SET_ERR(ret, EXIF_TAG_GPS_LATITUDE);
				}
			} else {
				_mmcam_dbg_warn("malloc failed");
			}
		}

		/*42. Longitude*/
		_mmcam_dbg_log("f_longitude [%f]", f_longitude);
		if (f_longitude != INVALID_GPS_VALUE) {
			unsigned char *b = NULL;
			unsigned int deg;
			unsigned int min;
			unsigned int sec;
			ExifRational rData;

			if (f_longitude < 0) {
				ret = mm_exif_set_add_entry(ed, EXIF_IFD_GPS, EXIF_TAG_GPS_LONGITUDE_REF,
				                            EXIF_FORMAT_ASCII, 2, (unsigned char *)"W");
				if (ret != MM_ERROR_NONE) {
					EXIF_SET_ERR(ret, EXIF_TAG_GPS_LONGITUDE_REF);
				}
				f_longitude = -f_longitude;
			} else if (f_longitude > 0) {
				ret = mm_exif_set_add_entry(ed, EXIF_IFD_GPS, EXIF_TAG_GPS_LONGITUDE_REF,
				                            EXIF_FORMAT_ASCII, 2, (unsigned char *)"E");
				if (ret != MM_ERROR_NONE) {
					EXIF_SET_ERR(ret, EXIF_TAG_GPS_LONGITUDE_REF);
				}
			}

			deg = (unsigned int)(f_longitude);
			min = (unsigned int)((f_longitude-deg)*60);
			sec = (unsigned int)(((f_longitude-deg)*3600)-min*60);

			_mmcam_dbg_log("f_longitude deg[%d], min[%d], sec[%d]", deg, min, sec);
			b = malloc(3 * sizeof(ExifRational));
			if (b) {
				rData.numerator = deg;
				rData.denominator = 1;
				exif_set_rational(b, exif_data_get_byte_order(ed), rData);
				rData.numerator = min;
				exif_set_rational(b+8, exif_data_get_byte_order(ed), rData);
				rData.numerator = sec;
				exif_set_rational(b+16, exif_data_get_byte_order(ed), rData);
				ret = mm_exif_set_add_entry(ed, EXIF_IFD_GPS, EXIF_TAG_GPS_LONGITUDE,
				                            EXIF_FORMAT_RATIONAL, 3, (unsigned char *)b);
				free(b);
				if (ret != MM_ERROR_NONE) {
					EXIF_SET_ERR(ret, EXIF_TAG_GPS_LONGITUDE);
				}
			} else {
				_mmcam_dbg_warn("malloc failed");
			}
		}

		/*43. Altitude*/
		_mmcam_dbg_log("f_altitude [%f]", f_altitude);
		if (f_altitude != INVALID_GPS_VALUE) {
			ExifByte alt_ref = 0;
			unsigned char *b = NULL;
			ExifRational rData;
			b = malloc(sizeof(ExifRational));
			if (b) {
				if (f_altitude < 0) {
					alt_ref = 1;
					f_altitude = -f_altitude;
				}

				ret = mm_exif_set_add_entry(ed, EXIF_IFD_GPS, EXIF_TAG_GPS_ALTITUDE_REF,
				                            EXIF_FORMAT_BYTE, 1, (unsigned char *)&alt_ref);
				if (ret != MM_ERROR_NONE) {
					_mmcam_dbg_err("error [%x], tag [%x]", ret, EXIF_TAG_GPS_ALTITUDE_REF);
					if (ret == MM_ERROR_CAMCORDER_LOW_MEMORY) {
						free(b);
						b = NULL;
						goto exit;
					}
				}

				rData.numerator = (unsigned int)(f_altitude + 0.5)*100;
				rData.denominator = 100;
				exif_set_rational(b, exif_data_get_byte_order(ed), rData);
				ret = mm_exif_set_add_entry(ed, EXIF_IFD_GPS, EXIF_TAG_GPS_ALTITUDE,
				                            EXIF_FORMAT_RATIONAL, 1, (unsigned char *)b);
				free(b);
				if (ret != MM_ERROR_NONE) {
					EXIF_SET_ERR(ret, EXIF_TAG_GPS_ALTITUDE);
				}
			} else {
				_mmcam_dbg_warn("malloc failed");
			}
		}

		/*44. EXIF_TAG_GPS_TIME_STAMP*/
		{
			double gps_timestamp = INVALID_GPS_VALUE;
			mm_camcorder_get_attributes(handle, NULL, "tag-gps-time-stamp", &gps_timestamp, NULL);
			_mmcam_dbg_log("Gps timestamp [%f]", gps_timestamp);
			if (gps_timestamp > 0.0) {
				unsigned char *b = NULL;
				unsigned int hour;
				unsigned int min;
				unsigned int microsec;
				ExifRational rData;

				hour = (unsigned int)(gps_timestamp / 3600);
				min = (unsigned int)((gps_timestamp - 3600 * hour) / 60);
				microsec = (unsigned int)(((double)((double)gps_timestamp -(double)(3600 * hour)) -(double)(60 * min)) * 1000000);

				_mmcam_dbg_log("Gps timestamp hour[%d], min[%d], microsec[%d]", hour, min, microsec);
				b = malloc(3 * sizeof(ExifRational));
				if (b) {
					rData.numerator = hour;
					rData.denominator = 1;
					exif_set_rational(b, exif_data_get_byte_order(ed), rData);

					rData.numerator = min;
					rData.denominator = 1;
					exif_set_rational(b + 8, exif_data_get_byte_order(ed), rData);

					rData.numerator = microsec;
					rData.denominator = 1000000;
					exif_set_rational(b + 16, exif_data_get_byte_order(ed), rData);

					ret = mm_exif_set_add_entry(ed, EXIF_IFD_GPS, EXIF_TAG_GPS_TIME_STAMP,
					                            EXIF_FORMAT_RATIONAL, 3, b);
					free(b);
					if (ret != MM_ERROR_NONE) {
						EXIF_SET_ERR(ret, EXIF_TAG_GPS_TIME_STAMP);
					}
				} else {
					_mmcam_dbg_warn( "malloc failed." );
				}
			}
		}

		/*45. EXIF_TAG_GPS_DATE_STAMP*/
		{
			unsigned char *date_stamp = NULL;
			int date_stamp_len = 0;

			mm_camcorder_get_attributes(handle, NULL, "tag-gps-date-stamp", &date_stamp, &date_stamp_len, NULL);

			if (date_stamp) {
				_mmcam_dbg_log("Date stamp [%s]", date_stamp);

				/* cause it should include NULL char */
				ret = mm_exif_set_add_entry(ed, EXIF_IFD_GPS, EXIF_TAG_GPS_DATE_STAMP,
				                            EXIF_FORMAT_ASCII, date_stamp_len + 1, date_stamp);
				if (ret != MM_ERROR_NONE) {
					EXIF_SET_ERR(ret, EXIF_TAG_GPS_DATE_STAMP);
				}
			}
		}

		/*46. EXIF_TAG_GPS_PROCESSING_METHOD */
		{
			unsigned char *processing_method = NULL;
			int processing_method_len = 0;

			mm_camcorder_get_attributes(handle, NULL, "tag-gps-processing-method", &processing_method, &processing_method_len, NULL);

			if (processing_method) {
				_mmcam_dbg_log("Processing method [%s]", processing_method);

				ret = mm_exif_set_add_entry(ed, EXIF_IFD_GPS, EXIF_TAG_GPS_PROCESSING_METHOD,
				                            EXIF_FORMAT_UNDEFINED, processing_method_len, processing_method);
				if (ret != MM_ERROR_NONE) {
					EXIF_SET_ERR(ret, EXIF_TAG_GPS_PROCESSING_METHOD);
				}
			}
		}
	} else {
		_mmcam_dbg_log( "Tag for GPS is DISABLED." );
	}


	/*47. EXIF_TAG_MAKER_NOTE*/
	ret = mm_exif_set_add_entry(ed, EXIF_IFD_EXIF, EXIF_TAG_MAKER_NOTE,
	                            EXIF_FORMAT_UNDEFINED, 8, (unsigned char *)"SAMSUNG");
	if (ret != MM_ERROR_NONE) {
		EXIF_SET_ERR(ret, EXIF_TAG_MAKER_NOTE);
	}

	/* create and link samsung maker note */
	ret = mm_exif_mnote_create(ed);
	if (ret != MM_ERROR_NONE){
		EXIF_SET_ERR(ret, EXIF_TAG_MAKER_NOTE);
	} else {
		_mmcam_dbg_log("Samsung makernote created");

		/* add samsung maker note entries (param : data, tag, index, subtag index1, subtag index2) */
		ret = mm_exif_mnote_set_add_entry(ed, MNOTE_SAMSUNG_TAG_MNOTE_VERSION, 0, _MNOTE_VALUE_NONE, _MNOTE_VALUE_NONE);
		if (ret != MM_ERROR_NONE) {
			_mmcam_dbg_err("mm_exif_mnote_set_add_entry error! [%x]", ret);
		}
	/*
		ret = mm_exif_mnote_set_add_entry(ed, MNOTE_SAMSUNG_TAG_DEVICE_ID, 2, _MNOTE_VALUE_NONE, _MNOTE_VALUE_NONE);
		if (ret != MM_ERROR_NONE) {
			_mmcam_dbg_err("mm_exif_mnote_set_add_entry error! [%x]", ret);
		}

		ret = mm_exif_mnote_set_add_entry(ed, MNOTE_SAMSUNG_TAG_SERIAL_NUM, _MNOTE_VALUE_NONE, _MNOTE_VALUE_NONE, _MNOTE_VALUE_NONE);
		if (ret != MM_ERROR_NONE) {
			_mmcam_dbg_err("mm_exif_mnote_set_add_entry error! [%x]", ret);
		}

		ret = mm_exif_mnote_set_add_entry(ed, MNOTE_SAMSUNG_TAG_COLOR_SPACE, 1, _MNOTE_VALUE_NONE, _MNOTE_VALUE_NONE);
		if (ret != MM_ERROR_NONE) {
			_mmcam_dbg_err("mm_exif_mnote_set_add_entry error! [%x]", ret);
		}
	*/
		ret = mm_exif_mnote_set_add_entry(ed, MNOTE_SAMSUNG_TAG_FACE_DETECTION, 0, _MNOTE_VALUE_NONE, _MNOTE_VALUE_NONE);
		if (ret != MM_ERROR_NONE) {
			_mmcam_dbg_err("mm_exif_mnote_set_add_entry error! ret=%x", ret);
		}
	/*
		ret = mm_exif_mnote_set_add_entry(ed, MNOTE_SAMSUNG_TAG_MODEL_ID, _MNOTE_VALUE_NONE, 3, 2);
		if (ret != MM_ERROR_NONE) {
			_mmcam_dbg_err("mm_exif_mnote_set_add_entry error! [%x]", ret);
		}
	*/
	}

	_mmcam_dbg_log("");

	ret = mm_exif_set_exif_to_info(hcamcorder->exif_info, ed);
	if (ret != MM_ERROR_NONE) {
		_mmcam_dbg_err("mm_exif_set_exif_to_info err!! [%x]", ret);
	}

exit:
	_mmcam_dbg_log("finished!! [%x]", ret);

	if (ed) {
		exif_data_unref (ed);
	}

	return ret;
}


static void __sound_status_changed_cb(keynode_t* node, void *data)
{
	mmf_camcorder_t *hcamcorder = (mmf_camcorder_t *)data;
	_MMCamcorderImageInfo *info = NULL;

	mmf_return_if_fail(hcamcorder && hcamcorder->sub_context && hcamcorder->sub_context->info_image);

	_mmcam_dbg_log("START");

	info = hcamcorder->sub_context->info_image;

	__ta__("        GET_IN_CB:VCONFKEY_SETAPPL_SOUND_STATUS_BOOL",
	vconf_get_bool(VCONFKEY_SETAPPL_SOUND_STATUS_BOOL, &(info->sound_status));
	);

	_mmcam_dbg_log("DONE : sound status %d", info->sound_status);

	return;
}


static void __volume_level_changed_cb(void *data)
{
	mmf_camcorder_t *hcamcorder = (mmf_camcorder_t *)data;
	_MMCamcorderImageInfo *info = NULL;

	mmf_return_if_fail(hcamcorder && hcamcorder->sub_context && hcamcorder->sub_context->info_image);

	_mmcam_dbg_log("START");

	info = hcamcorder->sub_context->info_image;

	__ta__("        GET_IN_CB:mm_sound_volume_get_value",
	mm_sound_volume_get_value(VOLUME_TYPE_MEDIA, &info->volume_level);
	);

	_mmcam_dbg_log("DONE : volume level %d", info->volume_level);

	return;
}
