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
#include <gst/interfaces/cameracontrol.h>
#include "mm_camcorder_internal.h"
#include "mm_camcorder_videorec.h"

/*---------------------------------------------------------------------------------------
|    GLOBAL VARIABLE DEFINITIONS for internal						|
---------------------------------------------------------------------------------------*/
#define _MMCAMCORDER_LOCATION_INFO // for add gps information
#define MAX_ERROR_MESSAGE_LEN         128

/*---------------------------------------------------------------------------------------
|    LOCAL VARIABLE DEFINITIONS for internal						|
---------------------------------------------------------------------------------------*/
#define _MMCAMCORDER_MINIMUM_FRAME              3
#define _MMCAMCORDER_RETRIAL_COUNT              10
#define _MMCAMCORDER_FRAME_WAIT_TIME            200000 /* ms */
#define _MMCAMCORDER_FREE_SPACE_CHECK_INTERVAL  5
#define _OFFSET_COMPOSITION_MATRIX              40L

/*---------------------------------------------------------------------------------------
|    LOCAL FUNCTION PROTOTYPES:								|
---------------------------------------------------------------------------------------*/
/* STATIC INTERNAL FUNCTION */
static gboolean __mmcamcorder_audio_dataprobe_check(GstPad *pad, GstBuffer *buffer, gpointer u_data);
static gboolean __mmcamcorder_video_dataprobe_record(GstPad *pad, GstBuffer *buffer, gpointer u_data);
static gboolean __mmcamcorder_audioque_dataprobe(GstPad *pad, GstBuffer *buffer, gpointer u_data);
static gboolean __mmcamcorder_video_dataprobe_audio_disable(GstPad *pad, GstBuffer *buffer, gpointer u_data);
static gboolean __mmcamcorder_audio_dataprobe_audio_mute(GstPad *pad, GstBuffer *buffer, gpointer u_data);
static gboolean __mmcamcorder_add_locationinfo(MMHandleType handle, int fileformat);
static gboolean __mmcamcorder_add_locationinfo_mp4(MMHandleType handle);
static gboolean __mmcamcorder_eventprobe_monitor(GstPad *pad, GstEvent *event, gpointer u_data);

/*=======================================================================================
|  FUNCTION DEFINITIONS									|
=======================================================================================*/
/*---------------------------------------------------------------------------------------
|    GLOBAL FUNCTION DEFINITIONS:							|
---------------------------------------------------------------------------------------*/
int _mmcamcorder_add_recorder_pipeline(MMHandleType handle)
{
	int err = MM_ERROR_NONE;
	int audio_disable = FALSE;
	char* gst_element_rsink_name = NULL;

	GstPad *srcpad = NULL;
	GstPad *sinkpad = NULL;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;

	type_element *RecordsinkElement = NULL;

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	mmf_return_val_if_fail(sc, MM_ERROR_CAMCORDER_NOT_INITIALIZED);
	mmf_return_val_if_fail(sc->element, MM_ERROR_CAMCORDER_NOT_INITIALIZED);
	
	_mmcam_dbg_log("");

	err = _mmcamcorder_check_videocodec_fileformat_compatibility( handle );
	if( err != MM_ERROR_NONE )
	{
		return err;
	}

	/* Create gstreamer element */
	/* Check main pipeline */
	if (!sc->element[_MMCAMCORDER_MAIN_PIPE].gst) {
		err = MM_ERROR_CAMCORDER_RESOURCE_CREATION;
		goto pipeline_creation_error;
	}

	/* get audio disable */
	mm_camcorder_get_attributes(handle, NULL,
	                            MMCAM_AUDIO_DISABLE, &audio_disable,
	                            NULL);

	if (sc->is_modified_rate || audio_disable) {
		sc->audio_disable = TRUE;
	} else {
		sc->audio_disable = FALSE;
	}
	_mmcam_dbg_log("AUDIO DISABLE : %d (is_modified_rate %d, audio_disable %d)",
	               sc->audio_disable, sc->is_modified_rate, audio_disable);

	if (sc->audio_disable == FALSE) {
		/* Sub pipeline */
		__ta__("        __mmcamcorder_create_audiosrc_bin",
		err = _mmcamcorder_create_audiosrc_bin((MMHandleType)hcamcorder);
		);
		if (err != MM_ERROR_NONE) {
			return err;
		}

		gst_bin_add(GST_BIN(sc->element[_MMCAMCORDER_MAIN_PIPE].gst),
		            sc->element[_MMCAMCORDER_AUDIOSRC_BIN].gst);
	}

	__ta__("        _mmcamcorder_create_encodesink_bin",
	err = _mmcamcorder_create_encodesink_bin((MMHandleType)hcamcorder, MM_CAMCORDER_ENCBIN_PROFILE_VIDEO);
	);
	if (err != MM_ERROR_NONE) {
		return err;
	}

	gst_bin_add(GST_BIN(sc->element[_MMCAMCORDER_MAIN_PIPE].gst),
	            sc->element[_MMCAMCORDER_ENCSINK_BIN].gst);

	/* Link each element */
	srcpad = gst_element_get_static_pad(sc->element[_MMCAMCORDER_VIDEOSRC_BIN].gst, "src1");
	sinkpad = gst_element_get_static_pad(sc->element[_MMCAMCORDER_ENCSINK_BIN].gst, "video_sink0");
	_MM_GST_PAD_LINK_UNREF(srcpad, sinkpad, err, pipeline_creation_error);

	if (sc->audio_disable == FALSE) {
		srcpad = gst_element_get_static_pad (sc->element[_MMCAMCORDER_AUDIOSRC_BIN].gst, "src");
		sinkpad = gst_element_get_static_pad (sc->element[_MMCAMCORDER_ENCSINK_BIN].gst, "audio_sink0");
		_MM_GST_PAD_LINK_UNREF(srcpad, sinkpad, err, pipeline_creation_error);
	}

	_mmcamcorder_conf_get_element(hcamcorder->conf_main,
	                              CONFIGURE_CATEGORY_MAIN_RECORD,
	                              "RecordsinkElement",
	                              &RecordsinkElement);
	_mmcamcorder_conf_get_value_element_name(RecordsinkElement, &gst_element_rsink_name);

	/* set data probe function */

	/* register message cb */

	/* set data probe function for audio */

	if (sc->audio_disable == FALSE) {
		sinkpad = gst_element_get_static_pad(sc->element[_MMCAMCORDER_ENCSINK_AENC].gst, "sink");
		MMCAMCORDER_ADD_BUFFER_PROBE(sinkpad, _MMCAMCORDER_HANDLER_VIDEOREC,
		                             __mmcamcorder_audioque_dataprobe, hcamcorder);
		gst_object_unref(sinkpad);
		sinkpad = NULL;
		
		/* for voice mute */
		srcpad = gst_element_get_static_pad(sc->element[_MMCAMCORDER_AUDIOSRC_SRC].gst, "src");
		MMCAMCORDER_ADD_BUFFER_PROBE(srcpad, _MMCAMCORDER_HANDLER_VIDEOREC,
		                             __mmcamcorder_audio_dataprobe_audio_mute, hcamcorder);
		gst_object_unref(srcpad);
		srcpad = NULL;

		if (sc->element[_MMCAMCORDER_ENCSINK_AENC_QUE].gst) {
			srcpad = gst_element_get_static_pad(sc->element[_MMCAMCORDER_ENCSINK_AENC_QUE].gst, "src");
			MMCAMCORDER_ADD_EVENT_PROBE(srcpad, _MMCAMCORDER_HANDLER_VIDEOREC,
			                            __mmcamcorder_eventprobe_monitor, hcamcorder);
			gst_object_unref(srcpad);
			srcpad = NULL;
		}
	}

	if (sc->element[_MMCAMCORDER_ENCSINK_VENC_QUE].gst) {
		srcpad = gst_element_get_static_pad(sc->element[_MMCAMCORDER_ENCSINK_VENC_QUE].gst, "src");
		MMCAMCORDER_ADD_EVENT_PROBE(srcpad, _MMCAMCORDER_HANDLER_VIDEOREC,
		                            __mmcamcorder_eventprobe_monitor, hcamcorder);
		gst_object_unref(srcpad);
		srcpad = NULL;
	}

	if (sc->audio_disable) {
		sinkpad = gst_element_get_static_pad(sc->element[_MMCAMCORDER_ENCSINK_VENC].gst, "sink");
		MMCAMCORDER_ADD_BUFFER_PROBE(sinkpad, _MMCAMCORDER_HANDLER_VIDEOREC,
		                             __mmcamcorder_video_dataprobe_audio_disable, hcamcorder);
		gst_object_unref(sinkpad);
		sinkpad = NULL;
	}

	if (!strcmp(gst_element_rsink_name, "filesink")) {
		srcpad = gst_element_get_static_pad(sc->element[_MMCAMCORDER_ENCSINK_VENC].gst, "src");
		MMCAMCORDER_ADD_BUFFER_PROBE(srcpad, _MMCAMCORDER_HANDLER_VIDEOREC,
		                             __mmcamcorder_video_dataprobe_record, hcamcorder);
		gst_object_unref(srcpad);
		srcpad = NULL;

		srcpad = gst_element_get_static_pad(sc->element[_MMCAMCORDER_ENCSINK_AENC].gst, "src");
		MMCAMCORDER_ADD_BUFFER_PROBE(srcpad, _MMCAMCORDER_HANDLER_VIDEOREC,
		                             __mmcamcorder_audio_dataprobe_check, hcamcorder);
		gst_object_unref(srcpad);
		srcpad = NULL;
	}

	return MM_ERROR_NONE;

pipeline_creation_error:
	return err;
}


int _mmcamcorder_remove_audio_pipeline(MMHandleType handle)
{
	int ret = MM_ERROR_NONE;
	GstPad *srcpad = NULL;
	GstPad *sinkpad = NULL;
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	mmf_return_val_if_fail(sc, MM_ERROR_CAMCORDER_NOT_INITIALIZED);
	mmf_return_val_if_fail(sc->element, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	_mmcam_dbg_log("");

	if (sc->element[_MMCAMCORDER_AUDIOSRC_BIN].gst != NULL) {
		__ta__( "        AudiosrcBin Set NULL",
		ret = _mmcamcorder_gst_set_state(handle, sc->element[_MMCAMCORDER_AUDIOSRC_BIN].gst, GST_STATE_NULL);
		);
		if (ret != MM_ERROR_NONE) {
			_mmcam_dbg_err("Faile to change audio source state[%d]", ret);
			return ret;
		}

		srcpad = gst_element_get_static_pad(sc->element[_MMCAMCORDER_AUDIOSRC_BIN].gst, "src");
		sinkpad = gst_element_get_static_pad(sc->element[_MMCAMCORDER_ENCSINK_BIN].gst, "audio_sink0");
		_MM_GST_PAD_UNLINK_UNREF(srcpad, sinkpad);

		gst_bin_remove(GST_BIN(sc->element[_MMCAMCORDER_MAIN_PIPE].gst),
		               sc->element[_MMCAMCORDER_AUDIOSRC_BIN].gst);

		/*
			To avoid conflicting between old elements and newly created elements,
			I clean element handles here. Real elements object will be finalized as the 'unref' process goes on.
			This is a typical problem of unref. Even though I unref bin here, it takes much time to finalize each elements.
			So I clean handles first, make them unref later. Audio recording, however, isn't needed this process.
			It's because the pipeline of audio recording destroys at the same time,
			and '_mmcamcorder_element_release_noti' will perfom removing handle.
		*/
		_mmcamcorder_remove_element_handle(handle, _MMCAMCORDER_AUDIOSRC_BIN, _MMCAMCORDER_AUDIOSRC_NS);

		_mmcam_dbg_log("Audio pipeline removed");
	}

	return MM_ERROR_NONE;
}


int _mmcamcorder_remove_encoder_pipeline(MMHandleType handle)
{
	int ret = MM_ERROR_NONE;
	GstPad *srcpad = NULL;
	GstPad *sinkpad = NULL;
	GstPad *reqpad = NULL;
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;
	
	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	mmf_return_val_if_fail(sc, MM_ERROR_CAMCORDER_NOT_INITIALIZED);
	mmf_return_val_if_fail(sc->element, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	_mmcam_dbg_log("");

	if (sc->element[_MMCAMCORDER_ENCSINK_BIN].gst != NULL) {
		__ta__( "        EncodeBin Set NULL",
		ret = _mmcamcorder_gst_set_state(handle, sc->element[_MMCAMCORDER_ENCSINK_BIN].gst, GST_STATE_NULL);
		);
		if (ret != MM_ERROR_NONE) {
			_mmcam_dbg_err("Faile to change encode bin state[%d]", ret);
			return ret;
		}

		srcpad = gst_element_get_static_pad(sc->element[_MMCAMCORDER_VIDEOSRC_BIN].gst, "src1");
		sinkpad = gst_element_get_static_pad(sc->element[_MMCAMCORDER_ENCSINK_BIN].gst, "video_sink0");
		_MM_GST_PAD_UNLINK_UNREF(srcpad, sinkpad);

		/* release request pad */
		reqpad = gst_element_get_static_pad(sc->element[_MMCAMCORDER_ENCSINK_ENCBIN].gst, "audio");
		if (reqpad) {
			gst_element_release_request_pad(sc->element[_MMCAMCORDER_ENCSINK_ENCBIN].gst, reqpad);
			gst_object_unref(reqpad);
			reqpad = NULL;
		}

		reqpad = gst_element_get_static_pad(sc->element[_MMCAMCORDER_ENCSINK_ENCBIN].gst, "video");
		if (reqpad) {
			gst_element_release_request_pad(sc->element[_MMCAMCORDER_ENCSINK_ENCBIN].gst, reqpad);
			gst_object_unref(reqpad);
			reqpad = NULL;
		}

		gst_bin_remove(GST_BIN(sc->element[_MMCAMCORDER_MAIN_PIPE].gst),
		               sc->element[_MMCAMCORDER_ENCSINK_BIN].gst);

		/*
			To avoid conflicting between old elements and newly created elements,
			I clean element handles here. Real elements object will be finalized as the 'unref' process goes on.
			This is a typical problem of unref. Even though I unref bin here, it takes much time to finalize each elements.
			So I clean handles first, make them unref later. Audio recording, however, isn't needed this process.
			It's because the pipeline of audio recording destroys at the same time,
			and '_mmcamcorder_element_release_noti' will perfom removing handle.
		*/
		_mmcamcorder_remove_element_handle(handle, _MMCAMCORDER_AUDIOSRC_QUE, _MMCAMCORDER_AUDIOSRC_ENC); /* Encode bin has audio encoder too. */
		_mmcamcorder_remove_element_handle(handle, _MMCAMCORDER_ENCSINK_BIN, _MMCAMCORDER_ENCSINK_SINK);

		_mmcam_dbg_log("Encoder pipeline removed");
	}

	return MM_ERROR_NONE;
}


int _mmcamcorder_remove_recorder_pipeline(MMHandleType handle)
{
	int ret = MM_ERROR_NONE;
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);
	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	mmf_return_val_if_fail(sc, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	_mmcam_dbg_log("");

	if (!sc->element[_MMCAMCORDER_MAIN_PIPE].gst) {
		_mmcam_dbg_warn("pipeline is not existed.");
		return MM_ERROR_NONE;
	}

	_mmcamcorder_remove_all_handlers((MMHandleType)hcamcorder, _MMCAMCORDER_HANDLER_VIDEOREC);

	/* remove audio pipeline first */
	ret = _mmcamcorder_remove_audio_pipeline(handle);
	if (ret != MM_ERROR_NONE) {
		_mmcam_dbg_err("Fail to remove audio pipeline");
		return ret;
	}

	ret = _mmcamcorder_remove_encoder_pipeline(handle);
	if (ret != MM_ERROR_NONE) {
		_mmcam_dbg_err("Fail to remove encoder pipeline");
		return ret;
	}

	return ret;
}


void _mmcamcorder_destroy_video_pipeline(MMHandleType handle)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;
	GstPad *reqpad1 = NULL;
	GstPad *reqpad2 = NULL;
	
	mmf_return_if_fail(hcamcorder);
	sc = MMF_CAMCORDER_SUBCONTEXT(handle);

	mmf_return_if_fail(sc);
	mmf_return_if_fail(sc->element);
	
	_mmcam_dbg_log("");

	if (sc->element[_MMCAMCORDER_MAIN_PIPE].gst) {
		MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSINK_QUE].gst, "empty-buffers", TRUE);
		_mmcamcorder_gst_set_state(handle, sc->element[_MMCAMCORDER_MAIN_PIPE].gst, GST_STATE_NULL);

		_mmcamcorder_remove_all_handlers((MMHandleType)hcamcorder, _MMCAMCORDER_HANDLER_CATEGORY_ALL);

		reqpad1 = gst_element_get_static_pad(sc->element[_MMCAMCORDER_VIDEOSRC_TEE].gst, "src0");
		reqpad2 = gst_element_get_static_pad(sc->element[_MMCAMCORDER_VIDEOSRC_TEE].gst, "src1");
		gst_element_release_request_pad(sc->element[_MMCAMCORDER_VIDEOSRC_TEE].gst, reqpad1);
		gst_element_release_request_pad(sc->element[_MMCAMCORDER_VIDEOSRC_TEE].gst, reqpad2);
		gst_object_unref(reqpad1);
		gst_object_unref(reqpad2);

		/* object disposing problem happen. */
		_mmcam_dbg_log("Reference count of pipeline(%d)", GST_OBJECT_REFCOUNT_VALUE(sc->element[_MMCAMCORDER_MAIN_PIPE].gst));
		gst_object_unref(sc->element[_MMCAMCORDER_MAIN_PIPE].gst);
	}
}


int _mmcamcorder_video_command(MMHandleType handle, int command)
{
	int size = 0;
	int fileformat = 0;
	int ret = MM_ERROR_NONE;
	double motion_rate = _MMCAMCORDER_DEFAULT_RECORDING_MOTION_RATE;
	char *temp_filename = NULL;
	char *err_name = NULL;

	gint fps = 0;
	GstElement *pipeline = NULL;
	GstPad *pad = NULL;

	_MMCamcorderVideoInfo *info = NULL;
	_MMCamcorderSubContext *sc = NULL;
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	mmf_return_val_if_fail(sc, MM_ERROR_CAMCORDER_NOT_INITIALIZED);
	mmf_return_val_if_fail(sc->info_video, MM_ERROR_CAMCORDER_NOT_INITIALIZED);
	mmf_return_val_if_fail(sc->element, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	info = sc->info_video;

	_mmcam_dbg_log("Command(%d)", command);

	pipeline = sc->element[_MMCAMCORDER_MAIN_PIPE].gst;

	switch (command) {
	case _MMCamcorder_CMD_RECORD:
	{
		if (_mmcamcorder_get_state((MMHandleType)hcamcorder) != MM_CAMCORDER_STATE_PAUSED) {
			guint imax_size = 0;
			guint imax_time = 0;

			/* Play record start sound */
			_mmcamcorder_sound_solo_play(handle, _MMCAMCORDER_FILEPATH_REC_START_SND, TRUE);

			/* Recording */
			_mmcam_dbg_log("Record Start");

			/* set hybrid mode when ITLV format */
			if (sc->info_image->preview_format == MM_PIXEL_FORMAT_ITLV_JPEG_UYVY) {
				_mmcam_dbg_log("flush cache TRUE");
				MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst, "flush-cache", TRUE);
			}

			ret = _mmcamcorder_gst_set_state(handle, pipeline, GST_STATE_READY);
			if (ret != MM_ERROR_NONE) {
				goto _ERR_CAMCORDER_VIDEO_COMMAND;
			}

			_mmcamcorder_conf_get_value_int(hcamcorder->conf_main,
			                                CONFIGURE_CATEGORY_MAIN_RECORD,
			                                "DropVideoFrame",
			                                &(sc->drop_vframe));

			_mmcamcorder_conf_get_value_int(hcamcorder->conf_main,
			                                CONFIGURE_CATEGORY_MAIN_RECORD,
			                                "PassFirstVideoFrame",
			                                &(sc->pass_first_vframe));

			_mmcam_dbg_log("Drop video frame count[%d], Pass fisrt video frame count[%d]",
			               sc->drop_vframe, sc->pass_first_vframe);

			ret = mm_camcorder_get_attributes(handle, &err_name,
			                                  MMCAM_CAMERA_FPS, &fps,
			                                  MMCAM_CAMERA_RECORDING_MOTION_RATE, &motion_rate,
			                                  MMCAM_FILE_FORMAT, &fileformat,
			                                  MMCAM_TARGET_FILENAME, &temp_filename, &size,
			                                  MMCAM_TARGET_MAX_SIZE, &imax_size,
			                                  MMCAM_TARGET_TIME_LIMIT, &imax_time,
			                                  MMCAM_FILE_FORMAT, &(info->fileformat),
			                                  NULL);
			if (ret != MM_ERROR_NONE) {
				_mmcam_dbg_warn("Get attrs fail. (%s:%x)", err_name, ret);
				SAFE_FREE (err_name);
				goto _ERR_CAMCORDER_VIDEO_COMMAND;
			}

			/* set max size */
			if (imax_size <= 0) {
				info->max_size = 0; /* do not check */
			} else {
				info->max_size = ((guint64)imax_size) << 10; /* to byte */
			}

			/* set max time */
			if (imax_time <= 0) {
				info->max_time = 0; /* do not check */
			} else {
				info->max_time = ((guint64)imax_time) * 1000; /* to millisecond */
			}

			if (sc->is_modified_rate) {
				info->record_timestamp_ratio = (_MMCAMCORDER_DEFAULT_RECORDING_MOTION_RATE/motion_rate);
				_mmcam_dbg_log("high speed recording fps:%d, slow_rate:%f, timestamp_ratio:%f",
				               fps, motion_rate, info->record_timestamp_ratio);
			} else {
				info->record_timestamp_ratio = _MMCAMCORDER_DEFAULT_RECORDING_MOTION_RATE;
				_mmcam_dbg_log("normal recording");
			}

			MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst, "hold-af-after-capturing", TRUE);
			MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst, "req-negotiation", TRUE);

			ret =_mmcamcorder_add_recorder_pipeline((MMHandleType)hcamcorder);
			if (ret != MM_ERROR_NONE) {
				goto _ERR_CAMCORDER_VIDEO_COMMAND;
			}

			info->filename = strdup(temp_filename);
			if (!info->filename) {
				_mmcam_dbg_err("strdup was failed");
				goto _ERR_CAMCORDER_VIDEO_COMMAND;
			}

			_mmcam_dbg_log("Record start : set file name using attribute - %s ",info->filename);

			MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_ENCSINK_SINK].gst, "location", info->filename);
			MMCAMCORDER_G_OBJECT_SET( sc->element[_MMCAMCORDER_ENCSINK_ENCBIN].gst, "block", 0);

			/* Adjust display FPS */
			sc->display_interval = 0;
			sc->previous_slot_time = 0;

			/* gst_element_set_base_time(GST_ELEMENT(pipeline), (GstClockTime)0);
			   if you want to use audio clock, enable this block
			   for change recorder_pipeline state to paused. */
			__ta__("        _MMCamcorder_CMD_RECORD:GST_STATE_PAUSED2",
			ret = _mmcamcorder_gst_set_state(handle, pipeline, GST_STATE_PAUSED);
			);
			if (ret != MM_ERROR_NONE) {
				/* Remove recorder pipeline and recording file which size maybe zero */
				__ta__("        record fail:remove_recorder_pipeline",
				_mmcamcorder_remove_recorder_pipeline((MMHandleType)hcamcorder);
				);
				if (info->filename) {
					_mmcam_dbg_log("file delete(%s)", info->filename);
					unlink(info->filename);
					g_free(info->filename);
					info->filename = NULL;
				}
				goto _ERR_CAMCORDER_VIDEO_COMMAND;
			}

			/**< To fix video recording hanging
				1. use gst_element_set_start_time() instead of gst_pipeline_set_new_stream_time()
				2. Set (GstClockTime)1 instead of (GstClockTime)0. Because of strict check in gstreamer 0.25,
				 basetime wouldn't change if you set (GstClockTime)0.
				3. Move set start time position below PAUSED of pipeline.
			*/
			gst_element_set_start_time(GST_ELEMENT(pipeline), (GstClockTime)1);
			info->video_frame_count = 0;
			info->audio_frame_count = 0;
			info->filesize = 0;
			sc->ferror_send = FALSE;
			sc->ferror_count = 0;
			sc->error_occurs = FALSE;
			sc->bget_eos = FALSE;

			__ta__("        _MMCamcorder_CMD_RECORD:GST_STATE_PLAYING2",
			ret = _mmcamcorder_gst_set_state(handle, pipeline, GST_STATE_PLAYING);
			);
			if (ret != MM_ERROR_NONE) {
				/* Remove recorder pipeline and recording file which size maybe zero */
				__ta__("        record fail:remove_recorder_pipeline",
				_mmcamcorder_remove_recorder_pipeline((MMHandleType)hcamcorder);
				);
				if (info->filename) {
					_mmcam_dbg_log("file delete(%s)", info->filename);
					unlink(info->filename);
					g_free(info->filename);
					info->filename = NULL;
				}
				goto _ERR_CAMCORDER_VIDEO_COMMAND;
			}
		} else {
			/* Resume case */
			int video_enc = 0;

			MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_ENCSINK_ENCBIN].gst, "runtime-pause", FALSE);

			mm_camcorder_get_attributes(handle, NULL, MMCAM_VIDEO_ENCODER, &video_enc, NULL);
			if (video_enc == MM_VIDEO_CODEC_MPEG4) {
				MMCAMCORDER_G_OBJECT_SET( sc->element[_MMCAMCORDER_ENCSINK_VENC].gst, "force-intra", TRUE);
			}

			_mmcam_dbg_log("Object property settings done");
		}
	}
		break;
	case _MMCamcorder_CMD_PAUSE:
	{
		int count = 0;

		if (info->b_commiting) {
			_mmcam_dbg_warn("now on commiting previous file!!(command : %d)", command);
			return MM_ERROR_CAMCORDER_CMD_IS_RUNNING;
		}

		for (count = 0 ; count <= _MMCAMCORDER_RETRIAL_COUNT ; count++) {
			if (sc->audio_disable) {
				/* check only video frame */
				if (info->video_frame_count >= _MMCAMCORDER_MINIMUM_FRAME) {
					break;
				} else if (count == _MMCAMCORDER_RETRIAL_COUNT) {
					_mmcam_dbg_err("Pause fail, frame count %" G_GUINT64_FORMAT "",
					               info->video_frame_count);
					return MM_ERROR_CAMCORDER_INVALID_CONDITION;
				} else {
					_mmcam_dbg_warn("Waiting for enough video frame, retrial[%d], frame %" G_GUINT64_FORMAT "",
					                count, info->video_frame_count);
				}

				usleep(_MMCAMCORDER_FRAME_WAIT_TIME);
			} else {
				/* check both of video and audio frame */
				if (info->video_frame_count >= _MMCAMCORDER_MINIMUM_FRAME && info->audio_frame_count) {
					break;
				} else if (count == _MMCAMCORDER_RETRIAL_COUNT) {
					_mmcam_dbg_err("Pause fail, frame count VIDEO[%" G_GUINT64_FORMAT "], AUDIO [%" G_GUINT64_FORMAT "]",
					               info->video_frame_count, info->audio_frame_count);
					return MM_ERROR_CAMCORDER_INVALID_CONDITION;
				} else {
					_mmcam_dbg_warn("Waiting for enough frames, retrial [%d], VIDEO[%" G_GUINT64_FORMAT "], AUDIO [%" G_GUINT64_FORMAT "]",
					                count, info->video_frame_count, info->audio_frame_count);
				}

				usleep(_MMCAMCORDER_FRAME_WAIT_TIME);
			}
		}
		/* tee block */
		MMCAMCORDER_G_OBJECT_SET( sc->element[_MMCAMCORDER_ENCSINK_ENCBIN].gst, "runtime-pause", TRUE);

		break;
	}
	case _MMCamcorder_CMD_CANCEL:
	{
		if (info->b_commiting) {
			_mmcam_dbg_warn("now on commiting previous file!!(command : %d)", command);
			return MM_ERROR_CAMCORDER_CMD_IS_RUNNING;
		}

		MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSINK_QUE].gst, "empty-buffers", TRUE);
		MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst, "hold-af-after-capturing", FALSE);

		__ta__("        _MMCamcorder_CMD_CANCEL:GST_STATE_READY",
		ret =_mmcamcorder_gst_set_state(handle, pipeline, GST_STATE_READY);
		);
		MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSINK_QUE].gst, "empty-buffers", FALSE);
		if (ret != MM_ERROR_NONE) {
			goto _ERR_CAMCORDER_VIDEO_COMMAND;
		}

		__ta__("        __mmcamcorder_remove_recorder_pipeline",
		_mmcamcorder_remove_recorder_pipeline((MMHandleType)hcamcorder);
		);

		/* remove target file */
		if (info->filename) {
			_mmcam_dbg_log("file delete(%s)", info->filename);
			unlink(info->filename);
			g_free(info->filename);
			info->filename = NULL;
		}

		sc->isMaxsizePausing = FALSE;
		sc->isMaxtimePausing = FALSE;

		sc->display_interval = 0;
		sc->previous_slot_time = 0;
		info->video_frame_count = 0;
		info->audio_frame_count = 0;
		info->filesize =0;

		/* set flush cache as FALSE */
		if (sc->info_image->preview_format == MM_PIXEL_FORMAT_ITLV_JPEG_UYVY) {
			MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst, "flush-cache", FALSE);
		}

		__ta__("        _MMCamcorder_CMD_CANCEL:GST_STATE_PLAYING",
		ret =_mmcamcorder_gst_set_state(handle, pipeline, GST_STATE_PLAYING);
		);
		if (ret != MM_ERROR_NONE) {
			goto _ERR_CAMCORDER_VIDEO_COMMAND;
		}
		break;
	}
	case _MMCamcorder_CMD_COMMIT:
	{
		int count = 0;

		if (info->b_commiting) {
			_mmcam_dbg_err("now on commiting previous file!!(command : %d)", command);
			return MM_ERROR_CAMCORDER_CMD_IS_RUNNING;
		} else {
			_mmcam_dbg_log("_MMCamcorder_CMD_COMMIT : start");
			info->b_commiting = TRUE;
		}

		for (count = 0 ; count <= _MMCAMCORDER_RETRIAL_COUNT ; count++) {
			if (sc->audio_disable) {
				/* check only video frame */
				if (info->video_frame_count >= _MMCAMCORDER_MINIMUM_FRAME) {
					break;
				} else if (count == _MMCAMCORDER_RETRIAL_COUNT) {
					_mmcam_dbg_err("Commit fail, frame count is %" G_GUINT64_FORMAT "",
					               info->video_frame_count);
					info->b_commiting = FALSE;
					return MM_ERROR_CAMCORDER_INVALID_CONDITION;
				} else {
					_mmcam_dbg_warn("Waiting for enough video frame, retrial [%d], frame %" G_GUINT64_FORMAT "",
					                count, info->video_frame_count);
				}

				usleep(_MMCAMCORDER_FRAME_WAIT_TIME);
			} else {
				/* check both of video and audio frame */
				if (info->video_frame_count >= _MMCAMCORDER_MINIMUM_FRAME && info->audio_frame_count) {
					break;
				} else if (count == _MMCAMCORDER_RETRIAL_COUNT) {
					_mmcam_dbg_err("Commit fail, VIDEO[%" G_GUINT64_FORMAT "], AUDIO [%" G_GUINT64_FORMAT "]",
					               info->video_frame_count, info->audio_frame_count);

					info->b_commiting = FALSE;
					return MM_ERROR_CAMCORDER_INVALID_CONDITION;
				} else {
					_mmcam_dbg_warn("Waiting for enough frames, retrial [%d], VIDEO[%" G_GUINT64_FORMAT "], AUDIO [%" G_GUINT64_FORMAT "]",
					                count, info->video_frame_count, info->audio_frame_count);
				}

				usleep(_MMCAMCORDER_FRAME_WAIT_TIME);
			}
		}

		MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst, "hold-af-after-capturing", FALSE);

		if (sc->error_occurs) {
			GstPad *video= NULL;
			GstPad *audio = NULL;
			int ret = 0;

			_mmcam_dbg_err("Committing Error case");

			video = gst_element_get_static_pad(sc->element[_MMCAMCORDER_VIDEOSINK_SINK].gst, "sink");
			ret = gst_pad_send_event (video, gst_event_new_eos());
			_mmcam_dbg_err("Sending EOS video sink  : %d", ret);
			gst_object_unref(video);

			video = gst_element_get_static_pad(sc->element[_MMCAMCORDER_ENCSINK_VENC].gst, "src");
			gst_pad_push_event (video, gst_event_new_flush_start());
			gst_pad_push_event (video, gst_event_new_flush_stop());
			ret = gst_pad_push_event (video, gst_event_new_eos());
			_mmcam_dbg_err("Sending EOS video encoder src pad  : %d", ret);
			gst_object_unref(video);

			if (sc->audio_disable == FALSE) {
				audio = gst_element_get_static_pad(sc->element[_MMCAMCORDER_ENCSINK_AENC].gst, "src");
				gst_pad_push_event (audio, gst_event_new_flush_start());
				gst_pad_push_event (audio, gst_event_new_flush_stop());
				ret = gst_element_send_event(sc->element[_MMCAMCORDER_AUDIOSRC_SRC].gst, gst_event_new_eos());
				_mmcam_dbg_err("Sending EOS audio encoder src pad  : %d", ret);
				gst_object_unref(audio);
			}
		} else {
			if (sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst != NULL) {
				ret = gst_element_send_event(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst, gst_event_new_eos());
				_mmcam_dbg_warn("send eos to videosrc result : %d", ret);
			}

			if (sc->element[_MMCAMCORDER_AUDIOSRC_SRC].gst != NULL) {
				pad = gst_element_get_static_pad(sc->element[_MMCAMCORDER_AUDIOSRC_SRC].gst, "src");
				ret = gst_element_send_event(sc->element[_MMCAMCORDER_AUDIOSRC_SRC].gst, gst_event_new_eos());
				gst_object_unref(pad);
				pad = NULL;

				_mmcam_dbg_warn("send eos to audiosrc result : %d", ret);
			}
		}

		if (hcamcorder->quick_device_close) {
			_mmcam_dbg_warn("quick_device_close");
			/* close device quickly */
			MMCAMCORDER_G_OBJECT_SET( sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst, "quick-device-close",TRUE);
		}

		/* sc */
		sc->display_interval = 0;
		sc->previous_slot_time = 0;

		/* Wait EOS */
		_mmcam_dbg_log("Start to wait EOS");
		ret =_mmcamcorder_get_eos_message(handle);

		/* set flush cache as FALSE */
		if (sc->info_image->preview_format == MM_PIXEL_FORMAT_ITLV_JPEG_UYVY) {
			MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst, "flush-cache", FALSE);
		}

		if (ret != MM_ERROR_NONE) {
			info->b_commiting = FALSE;
			goto _ERR_CAMCORDER_VIDEO_COMMAND;
		}
	}
		break;
	default:
		ret =  MM_ERROR_CAMCORDER_INVALID_ARGUMENT;
		goto _ERR_CAMCORDER_VIDEO_COMMAND;
	}

	return MM_ERROR_NONE;

_ERR_CAMCORDER_VIDEO_COMMAND:
	if (ret != MM_ERROR_NONE && sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst != NULL) {
		int op_status = 0;
		MMCAMCORDER_G_OBJECT_GET(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst, "operation-status", &op_status);
		_mmcam_dbg_err("Current Videosrc status[0x%x]", op_status);
	}

	return ret;
}


int _mmcamcorder_video_handle_eos(MMHandleType handle)
{
	int ret = MM_ERROR_NONE;
	int enabletag = 0;
	guint64 file_size = 0;

	GstPad *pad = NULL;
	GstElement *pipeline = NULL;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;
	_MMCamcorderVideoInfo *info = NULL;
	_MMCamcorderMsgItem msg;
	MMCamRecordingReport *report = NULL;

	mmf_return_val_if_fail(hcamcorder, FALSE);

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	mmf_return_val_if_fail(sc, FALSE);
	mmf_return_val_if_fail(sc->info_video, FALSE);

	info = sc->info_video;

	_mmcam_dbg_err("");

	MMTA_ACUM_ITEM_BEGIN("    _mmcamcorder_video_handle_eos", 0);

	pipeline = sc->element[_MMCAMCORDER_MAIN_PIPE].gst;

	/* remove blocking part */
	MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_ENCSINK_ENCBIN].gst, "block", FALSE);
	MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSINK_QUE].gst, "empty-buffers", TRUE);

	mm_camcorder_get_attributes(handle, NULL,
	                            MMCAM_TAG_ENABLE, &enabletag,
	                            NULL);

	_mmcam_dbg_log("Set state of pipeline as PAUSED");
	__ta__("        _MMCamcorder_CMD_COMMIT:GST_STATE_PAUSED",
	ret = _mmcamcorder_gst_set_state(handle, pipeline, GST_STATE_PAUSED);
	);
	if (ret != MM_ERROR_NONE) {
		_mmcam_dbg_warn("_MMCamcorder_CMD_COMMIT:GST_STATE_READY or PAUSED failed. error[%x]", ret);
	}

	MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSINK_QUE].gst, "empty-buffers", FALSE);

	__ta__("        _MMCamcorder_CMD_COMMIT:__mmcamcorder_remove_recorder_pipeline",
	ret = _mmcamcorder_remove_recorder_pipeline((MMHandleType)hcamcorder);
	);
	if (ret != MM_ERROR_NONE) {
		_mmcam_dbg_warn("_MMCamcorder_CMD_COMMIT:__mmcamcorder_remove_recorder_pipeline failed. error[%x]", ret);
	}

	if (enabletag && !(sc->ferror_send)) {
		__ta__( "        _MMCamcorder_CMD_COMMIT:__mmcamcorder_add_locationinfo",
		ret = __mmcamcorder_add_locationinfo((MMHandleType)hcamcorder, info->fileformat);
		);
		if (ret) {
			_mmcam_dbg_log("Writing location information SUCCEEDED !!");
		} else {
			_mmcam_dbg_err("Writing location information FAILED !!");
		}
	}

	/* Check file size */
	if (info->max_size > 0) {
		_mmcamcorder_get_file_size(info->filename, &file_size);
		_mmcam_dbg_log("MAX size %lld byte - created filesize %lld byte",
		               info->max_size, file_size);

		if (file_size > info->max_size) {
			_MMCamcorderMsgItem message;
			_mmcam_dbg_err("File size is greater than max size !!");
			message.id = MM_MESSAGE_CAMCORDER_ERROR;
			message.param.code = MM_ERROR_CAMCORDER_FILE_SIZE_OVER;
			_mmcamcroder_send_message((MMHandleType)hcamcorder, &message);
		}
	}

	/* Flush EOS event to avoid pending pipeline */
	_mmcam_dbg_log("Flush EOS event");
	pad = gst_element_get_static_pad(sc->element[_MMCAMCORDER_VIDEOSINK_SINK].gst, "sink");
	gst_pad_push_event(pad, gst_event_new_flush_start());
	gst_pad_push_event(pad, gst_event_new_flush_stop());
	gst_object_unref(pad);
	pad = NULL;

	pad = gst_element_get_static_pad(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst, "src");
	gst_pad_push_event(pad, gst_event_new_flush_start());
	gst_pad_push_event(pad, gst_event_new_flush_stop());
	gst_object_unref(pad);
	pad = NULL;

	_mmcam_dbg_log("Set state as PLAYING");
	__ta__("        _MMCamcorder_CMD_COMMIT:GST_STATE_PLAYING",
	ret =_mmcamcorder_gst_set_state(handle, pipeline, GST_STATE_PLAYING);
	);
	/* Do not return when error is occurred.
	   Recording file was created successfully, but starting pipeline failed */
	if (ret != MM_ERROR_NONE) {
		msg.id = MM_MESSAGE_CAMCORDER_ERROR;
		msg.param.code = ret;
		_mmcamcroder_send_message((MMHandleType)hcamcorder, &msg);
		_mmcam_dbg_err("Failed to set state PLAYING[%x]", ret);
	}

	if (hcamcorder->state_change_by_system != _MMCAMCORDER_STATE_CHANGE_BY_ASM) {
		/* Play record stop sound */
		__ta__("        _MMCamcorder_CMD_COMMIT:_mmcamcorder_sound_solo_play",
		_mmcamcorder_sound_solo_play(handle, _MMCAMCORDER_FILEPATH_REC_STOP_SND, TRUE);
		);
	} else {
		_mmcam_dbg_log("Do NOT play recording stop sound because of ASM stop");
	}

	/* Send recording report to application */
	msg.id = MM_MESSAGE_CAMCORDER_VIDEO_CAPTURED;
	report = (MMCamRecordingReport *)malloc(sizeof(MMCamRecordingReport));
	if (!report) {
		_mmcam_dbg_err("Recording report fail(%s). Out of memory.", info->filename);
	} else {
		report->recording_filename = strdup(info->filename);
		msg.param.data= report;
		msg.param.code = 1;
		_mmcamcroder_send_message((MMHandleType)hcamcorder, &msg);
	}

	/* Finishing */
	sc->pipeline_time = 0;
	sc->pause_time = 0;
	sc->isMaxsizePausing = FALSE; /*In async function, this variable should set in callback function. */
	sc->isMaxtimePausing = FALSE;
	sc->error_occurs = FALSE;

	info->video_frame_count = 0;
	info->audio_frame_count = 0;
	info->filesize = 0;
	g_free(info->filename);
	info->filename = NULL;
	info->b_commiting = FALSE;

	MMTA_ACUM_ITEM_END("    _mmcamcorder_video_handle_eos", 0);
	MMTA_ACUM_ITEM_END("Real Commit Time", 0);

	_mmcam_dbg_err("_MMCamcorder_CMD_COMMIT : end");

	return TRUE;
}


/**
 * This function is record video data probing function.
 * If this function is linked with certain pad by gst_pad_add_buffer_probe(),
 * this function will be called when data stream pass through the pad.
 *
 * @param[in]	pad		probing pad which calls this function.
 * @param[in]	buffer		buffer which contains stream data.
 * @param[in]	u_data		user data.
 * @return	This function returns true on success, or false value with error
 * @remarks
 * @see
 */
static gboolean __mmcamcorder_eventprobe_monitor(GstPad *pad, GstEvent *event, gpointer u_data)
{
	switch (GST_EVENT_TYPE(event)) {
	case GST_EVENT_UNKNOWN:
	/* upstream events */
	case GST_EVENT_QOS:
	case GST_EVENT_SEEK:
	case GST_EVENT_NAVIGATION:
	case GST_EVENT_LATENCY:
	/* downstream serialized events */
	case GST_EVENT_NEWSEGMENT :
	case GST_EVENT_TAG:
	case GST_EVENT_BUFFERSIZE:
		_mmcam_dbg_log("[%s:%s] gots %s", GST_DEBUG_PAD_NAME(pad), GST_EVENT_TYPE_NAME(event));
		break;
	case GST_EVENT_EOS:
		_mmcam_dbg_warn("[%s:%s] gots %s", GST_DEBUG_PAD_NAME(pad), GST_EVENT_TYPE_NAME(event));
		break;
	/* bidirectional events */
	case GST_EVENT_FLUSH_START:
	case GST_EVENT_FLUSH_STOP:
		_mmcam_dbg_err("[%s:%s] gots %s", GST_DEBUG_PAD_NAME(pad), GST_EVENT_TYPE_NAME(event));
		break;
	default:
		_mmcam_dbg_log("[%s:%s] gots %s", GST_DEBUG_PAD_NAME(pad), GST_EVENT_TYPE_NAME(event));
		break;
	}

	return TRUE;
}


static gboolean __mmcamcorder_audio_dataprobe_check(GstPad *pad, GstBuffer *buffer, gpointer u_data)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(u_data);
	_MMCamcorderSubContext *sc = NULL;
	_MMCamcorderVideoInfo *info = NULL;

	mmf_return_val_if_fail(hcamcorder, TRUE);
	mmf_return_val_if_fail(buffer, FALSE);
	sc = MMF_CAMCORDER_SUBCONTEXT(hcamcorder);

	mmf_return_val_if_fail(sc && sc->info_video, TRUE);
	info = sc->info_video;

	/*_mmcam_dbg_err("[%" GST_TIME_FORMAT "]", GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(buffer)));*/

	if (info->audio_frame_count == 0) {
		info->filesize += (guint64)GST_BUFFER_SIZE(buffer);
		info->audio_frame_count++;
		return TRUE;
	}

	if (sc->ferror_send || sc->isMaxsizePausing) {
		_mmcam_dbg_warn("Recording is paused, drop frames");
		return FALSE;
	}

	info->filesize += (guint64)GST_BUFFER_SIZE(buffer);
	info->audio_frame_count++;

	return TRUE;
}


static gboolean __mmcamcorder_video_dataprobe_record(GstPad *pad, GstBuffer *buffer, gpointer u_data)
{
	static int count = 0;
	gint ret = 0;
	guint vq_size = 0;
	guint aq_size = 0;
	guint64 free_space = 0;
	guint64 buffer_size = 0;
	guint64 trailer_size = 0;
	guint64 queued_buffer = 0;
	char *filename = NULL;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(u_data);
	_MMCamcorderMsgItem msg;
	_MMCamcorderSubContext *sc = NULL;
	_MMCamcorderVideoInfo *info = NULL;

	mmf_return_val_if_fail(hcamcorder, TRUE);
	mmf_return_val_if_fail(buffer, FALSE);

	sc = MMF_CAMCORDER_SUBCONTEXT(hcamcorder);
	mmf_return_val_if_fail(sc && sc->info_video, TRUE);
	info = sc->info_video;

	/*_mmcam_dbg_log("[%" GST_TIME_FORMAT "]", GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(buffer)));*/
	if (sc->ferror_send) {
		_mmcam_dbg_warn("file write error, drop frames");
		return FALSE;
	}

	info->video_frame_count++;
	if (info->video_frame_count <= (guint64)_MMCAMCORDER_MINIMUM_FRAME) {
		/* _mmcam_dbg_log("Pass minimum frame: info->video_frame_count: %" G_GUINT64_FORMAT " ",
		                info->video_frame_count); */
		info->filesize += (guint64)GST_BUFFER_SIZE(buffer);
		return TRUE;
	}

	buffer_size = GST_BUFFER_SIZE(buffer);

	/* get trailer size */
	if (info->fileformat == MM_FILE_FORMAT_3GP || info->fileformat == MM_FILE_FORMAT_MP4) {
		MMCAMCORDER_G_OBJECT_GET(sc->element[_MMCAMCORDER_ENCSINK_MUX].gst, "expected-trailer-size", &trailer_size);
	} else {
		trailer_size = 0;
	}

	/* to minimizing free space check overhead */
	count = count % _MMCAMCORDER_FREE_SPACE_CHECK_INTERVAL;
	if (count++ == 0) {
		filename = info->filename;
		ret = _mmcamcorder_get_freespace(filename, &free_space);

		/*_mmcam_dbg_log("check free space for recording");*/

		switch (ret) {
		case -2: /* file not exist */
		case -1: /* failed to get free space */
			_mmcam_dbg_err("Error occured. [%d]", ret);
			if (sc->ferror_count == 2 && sc->ferror_send == FALSE) {
				sc->ferror_send = TRUE;
				msg.id = MM_MESSAGE_CAMCORDER_ERROR;
				if (ret == -2) {
					msg.param.code = MM_ERROR_FILE_NOT_FOUND;
				} else {
					msg.param.code = MM_ERROR_FILE_READ;
				}
				_mmcamcroder_send_message((MMHandleType)hcamcorder, &msg);
			} else {
				sc->ferror_count++;
			}

			return FALSE; /* skip this buffer */
			break;
		default: /* succeeded to get free space */
			/* check free space for recording */
			/* get queued buffer size */
			if (sc->element[_MMCAMCORDER_ENCSINK_AENC_QUE].gst) {
				MMCAMCORDER_G_OBJECT_GET(sc->element[_MMCAMCORDER_ENCSINK_AENC_QUE].gst, "current-level-bytes", &aq_size);
			}
			if (sc->element[_MMCAMCORDER_ENCSINK_VENC_QUE].gst) {
				MMCAMCORDER_G_OBJECT_GET(sc->element[_MMCAMCORDER_ENCSINK_VENC_QUE].gst, "current-level-bytes", &vq_size);
			}

			queued_buffer = aq_size + vq_size;

			/* check free space */
			if (free_space < (_MMCAMCORDER_MINIMUM_SPACE + buffer_size + trailer_size + queued_buffer)) {
				_mmcam_dbg_warn("No more space for recording!!! Recording is paused.");
				_mmcam_dbg_warn("Free Space : [%" G_GUINT64_FORMAT "], trailer size : [%" G_GUINT64_FORMAT "]," \
				                " buffer size : [%" G_GUINT64_FORMAT "], queued buffer size : [%" G_GUINT64_FORMAT "]", \
				                free_space, trailer_size, buffer_size, queued_buffer);

				if (!sc->isMaxsizePausing) {
					MMCAMCORDER_G_OBJECT_SET( sc->element[_MMCAMCORDER_ENCSINK_ENCBIN].gst, "block", TRUE);
					sc->isMaxsizePausing = TRUE;

					msg.id = MM_MESSAGE_CAMCORDER_NO_FREE_SPACE;
					_mmcamcroder_send_message((MMHandleType)hcamcorder, &msg);
				}

				return FALSE;
			}
			break;
		}
	}

	/* check max size of recorded file */
	if (info->max_size > 0 &&
	    info->max_size < info->filesize + buffer_size + trailer_size + _MMCAMCORDER_MMS_MARGIN_SPACE) {
		GstState pipeline_state = GST_STATE_VOID_PENDING;
		GstElement *pipeline = sc->element[_MMCAMCORDER_MAIN_PIPE].gst;
		_mmcam_dbg_warn("Max size!!! Recording is paused.");
		_mmcam_dbg_warn("Max size : [%" G_GUINT64_FORMAT "], current file size : [%" G_GUINT64_FORMAT "]," \
		                " buffer size : [%" G_GUINT64_FORMAT "], trailer size : [%" G_GUINT64_FORMAT "]",
		                info->max_size, info->filesize, buffer_size, trailer_size);

		if (!sc->isMaxsizePausing) {
			gst_element_get_state(pipeline, &pipeline_state, NULL, -1) ;
			if (pipeline_state == GST_STATE_PLAYING) {
				MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_ENCSINK_ENCBIN].gst, "block", TRUE);
				sc->isMaxsizePausing = TRUE;
			}

			msg.id = MM_MESSAGE_CAMCORDER_MAX_SIZE;
			_mmcamcroder_send_message((MMHandleType)hcamcorder, &msg);
		}

		return FALSE;
	}

	info->filesize += (guint64)buffer_size;

	_mmcam_dbg_log("filesize %lld Byte, ", info->filesize);

	return TRUE;
}


static gboolean __mmcamcorder_video_dataprobe_audio_disable(GstPad *pad, GstBuffer *buffer, gpointer u_data)
{
	guint64 trailer_size = 0;
	guint64 rec_pipe_time = 0;
	unsigned int remained_time = 0;

	GstClockTime b_time;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(u_data);
	_MMCamcorderMsgItem msg;
	_MMCamcorderSubContext *sc = NULL;
	_MMCamcorderVideoInfo *info = NULL;

	mmf_return_val_if_fail(buffer, FALSE);
	mmf_return_val_if_fail(hcamcorder, TRUE);

	sc = MMF_CAMCORDER_SUBCONTEXT(u_data);
	mmf_return_val_if_fail(sc, TRUE);
	mmf_return_val_if_fail(sc->info_video, TRUE);

	info = sc->info_video;

	b_time = GST_BUFFER_TIMESTAMP(buffer);

	rec_pipe_time = GST_TIME_AS_MSECONDS(b_time);

	if (info->fileformat == MM_FILE_FORMAT_3GP || info->fileformat == MM_FILE_FORMAT_MP4) {
		MMCAMCORDER_G_OBJECT_GET(sc->element[_MMCAMCORDER_ENCSINK_MUX].gst, "expected-trailer-size", &trailer_size);
	} else {
		trailer_size = 0;
	}

	/* check max time */
	if (info->max_time > 0 && rec_pipe_time > info->max_time) {
		_mmcam_dbg_warn("Current time : [%" G_GUINT64_FORMAT "], Maximum time : [%" G_GUINT64_FORMAT "]", \
		                rec_pipe_time, info->max_time);

		if (!sc->isMaxtimePausing) {
			MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_ENCSINK_ENCBIN].gst, "block", TRUE);

			sc->isMaxtimePausing = TRUE;

			msg.id = MM_MESSAGE_CAMCORDER_RECORDING_STATUS;
			msg.param.recording_status.elapsed = (unsigned long long)rec_pipe_time;
			msg.param.recording_status.filesize = (unsigned long long)((info->filesize + trailer_size) >> 10);
			msg.param.recording_status.remained_time = 0;
			_mmcamcroder_send_message((MMHandleType)hcamcorder, &msg);

			msg.id = MM_MESSAGE_CAMCORDER_TIME_LIMIT;
			_mmcamcroder_send_message((MMHandleType)hcamcorder, &msg);
		}

		return FALSE;
	}

	/* calculate remained time can be recorded */
	if (info->max_time > 0 && info->max_time < (remained_time + rec_pipe_time)) {
		remained_time = info->max_time - rec_pipe_time;
	} else if (info->max_size > 0) {
		long double max_size = (long double)info->max_size;
		long double current_size = (long double)(info->filesize + trailer_size);

		remained_time = (unsigned int)((long double)rec_pipe_time * (max_size/current_size)) - rec_pipe_time;
	}

	msg.id = MM_MESSAGE_CAMCORDER_RECORDING_STATUS;
	msg.param.recording_status.elapsed = (unsigned long long)rec_pipe_time;
	msg.param.recording_status.filesize = (unsigned long long)((info->filesize + trailer_size) >> 10);
	msg.param.recording_status.remained_time = remained_time;
	_mmcamcroder_send_message((MMHandleType)hcamcorder, &msg);

	_mmcam_dbg_log("time [%" GST_TIME_FORMAT "], size [%d]",
	               GST_TIME_ARGS(rec_pipe_time), msg.param.recording_status.filesize);

	if (info->record_timestamp_ratio != _MMCAMCORDER_DEFAULT_RECORDING_MOTION_RATE) {
		GST_BUFFER_TIMESTAMP(buffer) = b_time * (info->record_timestamp_ratio);
	}

	return TRUE;
}


static gboolean __mmcamcorder_audioque_dataprobe(GstPad *pad, GstBuffer *buffer, gpointer u_data)
{
	_MMCamcorderMsgItem msg;
	guint64 trailer_size = 0;
	guint64 rec_pipe_time = 0;
	_MMCamcorderSubContext *sc = NULL;
	GstElement *pipeline = NULL;
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(u_data);
	_MMCamcorderVideoInfo *info = NULL;
	unsigned int remained_time = 0;

	mmf_return_val_if_fail(buffer, FALSE);
	mmf_return_val_if_fail(hcamcorder, TRUE);
	sc = MMF_CAMCORDER_SUBCONTEXT(u_data);

	mmf_return_val_if_fail(sc, TRUE);
	mmf_return_val_if_fail(sc->info_video, TRUE);
	mmf_return_val_if_fail(sc->element, TRUE);

	info = sc->info_video;
	pipeline = sc->element[_MMCAMCORDER_MAIN_PIPE].gst;

	if (!GST_CLOCK_TIME_IS_VALID(GST_BUFFER_TIMESTAMP(buffer))) {
		_mmcam_dbg_err( "Buffer timestamp is invalid, check it");
		return TRUE;
	}

	rec_pipe_time = GST_TIME_AS_MSECONDS(GST_BUFFER_TIMESTAMP(buffer));

	if (info->fileformat == MM_FILE_FORMAT_3GP || info->fileformat == MM_FILE_FORMAT_MP4) {
		MMCAMCORDER_G_OBJECT_GET(sc->element[_MMCAMCORDER_ENCSINK_MUX].gst, "expected-trailer-size", &trailer_size);
	} else {
		trailer_size = 0;
	}

	/* calculate remained time can be recorded */
	if (info->max_time > 0 && info->max_time < (remained_time + rec_pipe_time)) {
		remained_time = info->max_time - rec_pipe_time;
	} else if (info->max_size > 0) {
		long double max_size = (long double)info->max_size;
		long double current_size = (long double)(info->filesize + trailer_size);

		remained_time = (unsigned long long)((long double)rec_pipe_time * (max_size/current_size)) - rec_pipe_time;
	}

	if (info->max_time > 0 && rec_pipe_time > info->max_time) {
		_mmcam_dbg_warn("Current time : [%" G_GUINT64_FORMAT "], Maximum time : [%" G_GUINT64_FORMAT "]", \
		                rec_pipe_time, info->max_time);

		if (!sc->isMaxtimePausing) {
			MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_ENCSINK_ENCBIN].gst, "block", TRUE);

			sc->isMaxtimePausing = TRUE;

			msg.id = MM_MESSAGE_CAMCORDER_RECORDING_STATUS;
			msg.param.recording_status.elapsed = (unsigned long long)rec_pipe_time;
			msg.param.recording_status.filesize = (unsigned long long)((info->filesize + trailer_size) >> 10);
			msg.param.recording_status.remained_time = 0;
			_mmcamcroder_send_message((MMHandleType)hcamcorder, &msg);

			msg.id = MM_MESSAGE_CAMCORDER_TIME_LIMIT;
			_mmcamcroder_send_message((MMHandleType)hcamcorder, &msg);
		}

		return FALSE;
	}

	msg.id = MM_MESSAGE_CAMCORDER_RECORDING_STATUS;
	msg.param.recording_status.elapsed = (unsigned long long)rec_pipe_time;
	msg.param.recording_status.filesize = (unsigned long long)((info->filesize + trailer_size) >> 10);
	msg.param.recording_status.remained_time = remained_time;
	_mmcamcroder_send_message((MMHandleType)hcamcorder, &msg);

	_mmcam_dbg_log("audio data probe :: time [%" GST_TIME_FORMAT "], size [%lld KB]",
	               GST_TIME_ARGS(rec_pipe_time), msg.param.recording_status.filesize);

	return TRUE;
}


static gboolean __mmcamcorder_audio_dataprobe_audio_mute(GstPad *pad, GstBuffer *buffer, gpointer u_data)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(u_data);
	double volume = 0.0;
	int format = 0;
	int channel = 0;
	int err = MM_ERROR_UNKNOWN;
	char *err_name = NULL;

	mmf_return_val_if_fail(buffer, FALSE);
	mmf_return_val_if_fail(hcamcorder, FALSE);

	/*_mmcam_dbg_log("AUDIO SRC time stamp : [%" GST_TIME_FORMAT "] \n", GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(buffer)));*/
	err = mm_camcorder_get_attributes((MMHandleType)hcamcorder, &err_name,
	                                  MMCAM_AUDIO_VOLUME, &volume,
	                                  MMCAM_AUDIO_FORMAT, &format,
	                                  MMCAM_AUDIO_CHANNEL, &channel,
	                                  NULL);
	if (err != MM_ERROR_NONE) {
		_mmcam_dbg_warn("Get attrs fail. (%s:%x)", err_name, err);
		SAFE_FREE(err_name);
		return err;
	}

	/* Set audio stream NULL */
	if (volume == 0.0) {
		memset(GST_BUFFER_DATA(buffer), 0, GST_BUFFER_SIZE(buffer));
	}

	/* CALL audio stream callback */
	if (hcamcorder->astream_cb && buffer && GST_BUFFER_DATA(buffer)) {
		MMCamcorderAudioStreamDataType stream;

		if (_mmcamcorder_get_state((MMHandleType)hcamcorder) < MM_CAMCORDER_STATE_PREPARE) {
			_mmcam_dbg_warn("Not ready for stream callback");
			return TRUE;
		}

		/*_mmcam_dbg_log("Call video steramCb, data[%p], Width[%d],Height[%d], Format[%d]",
		               GST_BUFFER_DATA(buffer), width, height, format);*/

		stream.data = (void *)GST_BUFFER_DATA(buffer);
		stream.format = format;
		stream.channel = channel;
		stream.length = GST_BUFFER_SIZE(buffer);
		stream.timestamp = (unsigned int)(GST_BUFFER_TIMESTAMP(buffer)/1000000); /* nano -> milli second */

		_MMCAMCORDER_LOCK_ASTREAM_CALLBACK(hcamcorder);

		if (hcamcorder->astream_cb) {
			hcamcorder->astream_cb(&stream, hcamcorder->astream_cb_param);
		}

		_MMCAMCORDER_UNLOCK_ASTREAM_CALLBACK(hcamcorder);
	}

	return TRUE;
}


static gboolean __mmcamcorder_add_locationinfo(MMHandleType handle, int fileformat)
{
	gboolean bret = FALSE;

	switch (fileformat) {
	case MM_FILE_FORMAT_3GP:
	case MM_FILE_FORMAT_MP4:
		bret = __mmcamcorder_add_locationinfo_mp4(handle);
		break;
	default:
		_mmcam_dbg_warn("Unsupported fileformat to insert location info (%d)", fileformat);
		break;
	}

	return bret;
}


static gboolean __mmcamcorder_add_locationinfo_mp4(MMHandleType handle)
{
	FILE *f = NULL;
	guchar buf[4];
	guint64 udta_size = 0;
	gint64 current_pos = 0;
	gint64 moov_pos = 0;
	gint64 udta_pos = 0;
	gdouble longitude = 0;
	gdouble latitude = 0;
	gdouble altitude = 0;
	int err = 0;
	int orientation = 0;
	char *err_name = NULL;
	char err_msg[MAX_ERROR_MESSAGE_LEN] = {'\0',};
	_MMCamcorderLocationInfo location_info = {0,};

	_MMCamcorderVideoInfo *info = NULL;
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);
	sc = MMF_CAMCORDER_SUBCONTEXT(handle);

	mmf_return_val_if_fail(sc, MM_ERROR_CAMCORDER_NOT_INITIALIZED);
	mmf_return_val_if_fail(sc->info_video, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	_mmcam_dbg_log("");

	info = sc->info_video;

	f = fopen(info->filename, "rb+");
	if (f == NULL) {
		strerror_r(errno, err_msg, MAX_ERROR_MESSAGE_LEN);
		_mmcam_dbg_err("file open failed [%s]", err_msg);
		return FALSE;
	}

	mm_camcorder_get_attributes(handle, &err_name,
	                            MMCAM_TAG_LATITUDE, &latitude,
	                            MMCAM_TAG_LONGITUDE, &longitude,
	                            MMCAM_TAG_ALTITUDE, &altitude,
	                            MMCAM_TAG_ORIENTATION, &orientation,
	                            NULL);
	if (err_name) {
		_mmcam_dbg_warn("Get tag attrs fail. (%s:%x)", err_name, err);
		SAFE_FREE (err_name);
	}

	location_info.longitude = _mmcamcorder_double_to_fix(longitude);
	location_info.latitude = _mmcamcorder_double_to_fix(latitude);
	location_info.altitude = _mmcamcorder_double_to_fix(altitude);

	/* find udta container.
	   if, there are udta container, write loci box after that
	   else, make udta container and write loci box. */
	if (_mmcamcorder_find_tag(f, MMCAM_FOURCC('u','d','t','a'), TRUE)) {
		size_t nread = 0;

		_mmcam_dbg_log("find udta container");

		/* read size */
		if (fseek(f, -8L, SEEK_CUR) != 0) {
			goto fail;
		}

		udta_pos = ftell(f);
		if (udta_pos < 0) {
			goto ftell_fail;
		}

		nread = fread(&buf, sizeof(char), sizeof(buf), f);

		_mmcam_dbg_log("recorded file fread %d", nread);

		udta_size = _mmcamcorder_get_container_size(buf);

		/* goto end of udta and write 'loci' box */
		if (fseek(f, (udta_size-4L), SEEK_CUR) != 0) {
			goto fail;
		}

		if (!_mmcamcorder_write_loci(f, location_info)) {
			goto fail;
		}

		current_pos = ftell(f);
		if (current_pos < 0) {
			goto ftell_fail;
		}

		if (!_mmcamcorder_update_size(f, udta_pos, current_pos)) {
			goto fail;
		}
	} else {
		_mmcam_dbg_log("No udta container");
		if (fseek(f, 0, SEEK_END) != 0) {
			goto fail;
		}

		if (!_mmcamcorder_write_udta(f, location_info)) {
			goto fail;
		}
	}

	/* find moov container.
	   update moov container size. */
	if((current_pos = ftell(f))<0)
		goto ftell_fail;

	if (_mmcamcorder_find_tag(f, MMCAM_FOURCC('m','o','o','v'), TRUE)) {
		gint64 internal_pos = ftell(f);

		_mmcam_dbg_log("found moov container");
		if (fseek(f, -8L, SEEK_CUR) !=0) {
			goto fail;
		}

		moov_pos = ftell(f);
		if (moov_pos < 0) {
			goto ftell_fail;
		}

		if (!_mmcamcorder_update_size(f, moov_pos, current_pos)) {
			goto fail;
		}

		/* add orientation info */
		fseek(f, internal_pos, SEEK_SET);
		if (!_mmcamcorder_find_tag(f, MMCAM_FOURCC('t','r','a','k'), FALSE)) {
			_mmcam_dbg_err("failed to find [trak] tag");
			goto fail;
		}

		if (!_mmcamcorder_find_tag(f, MMCAM_FOURCC('t','k','h','d'), FALSE)) {
			_mmcam_dbg_err("failed to find [tkhd] tag");
			goto fail;
		}

		_mmcam_dbg_log("found [tkhd] tag");

		/* seek to start position of composition matrix */
		fseek(f, _OFFSET_COMPOSITION_MATRIX, SEEK_CUR);

		/* update composition matrix for orientation */
		_mmcamcorder_update_composition_matrix(f, orientation);
	} else {
		_mmcam_dbg_err("No 'moov' container");
		goto fail;
	}

	fclose(f);
	return TRUE;

fail:
	fclose(f);
	return FALSE;

ftell_fail:
	_mmcam_dbg_err("ftell() returns negative value.");
	fclose(f);
	return FALSE;
}
