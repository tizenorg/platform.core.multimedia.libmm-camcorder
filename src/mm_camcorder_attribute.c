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
#include "mm_camcorder_internal.h"

#include <gst/interfaces/colorbalance.h>
#include <gst/interfaces/cameracontrol.h>
#include <gst/interfaces/xoverlay.h>

/*-----------------------------------------------------------------------
|    MACRO DEFINITIONS:							|
-----------------------------------------------------------------------*/
#define MMCAMCORDER_DEFAULT_CAMERA_WIDTH        640
#define MMCAMCORDER_DEFAULT_CAMERA_HEIGHT       480

/*---------------------------------------------------------------------------------------
|    GLOBAL VARIABLE DEFINITIONS for internal						|
---------------------------------------------------------------------------------------*/
int depth[] = {MM_CAMCORDER_AUDIO_FORMAT_PCM_U8,
	       MM_CAMCORDER_AUDIO_FORMAT_PCM_S16_LE};

int visible_values[] = { 0, 1 };	/*0: off, 1:on*/

int strobe_mode[] = {MM_CAMCORDER_STROBE_MODE_OFF,
		     MM_CAMCORDER_STROBE_MODE_ON,
		     MM_CAMCORDER_STROBE_MODE_AUTO,
		     MM_CAMCORDER_STROBE_MODE_REDEYE_REDUCTION,
		     MM_CAMCORDER_STROBE_MODE_SLOW_SYNC,
		     MM_CAMCORDER_STROBE_MODE_FRONT_CURTAIN,
		     MM_CAMCORDER_STROBE_MODE_REAR_CURTAIN,
		     MM_CAMCORDER_STROBE_MODE_PERMANENT};

int tag_enable_values[] = { 0, 1 };

int tag_orientation_values[] =
{
	1,	/*The 0th row is at the visual top of the image, and the 0th column is the visual left-hand side.*/
	2,	/*the 0th row is at the visual top of the image, and the 0th column is the visual right-hand side.*/
	3,	/*the 0th row is at the visual bottom of the image, and the 0th column is the visual right-hand side.*/
	4,	/*the 0th row is at the visual bottom of the image, and the 0th column is the visual left-hand side.*/
	5,	/*the 0th row is the visual left-hand side of the image, and the 0th column is the visual top.*/
	6,	/*the 0th row is the visual right-hand side of the image, and the 0th column is the visual top.*/
	7,	/*the 0th row is the visual right-hand side of the image, and the 0th column is the visual bottom.*/
	8,	/*the 0th row is the visual left-hand side of the image, and the 0th column is the visual bottom.*/
};


/* basic attributes' info */
mm_cam_attr_construct_info cam_attrs_const_info[] ={
	//0
	{
		MM_CAM_MODE,                        /* ID */
		"mode",                             /* Name */
		MMF_VALUE_TYPE_INT,                 /* Type */
		MM_ATTRS_FLAG_RW,                   /* Flag */
		{(void*)MM_CAMCORDER_MODE_VIDEO_CAPTURE},     /* Default value */
		MM_ATTRS_VALID_TYPE_INT_RANGE,      /* Validity type */
		MM_CAMCORDER_MODE_VIDEO_CAPTURE,    /* Validity val1 (min, *array,...) */
		MM_CAMCORDER_MODE_AUDIO,            /* Validity val2 (max, count, ...) */
		NULL,                               /* Runtime setting function of the attribute */
	},
	// 1
	{
		MM_CAM_AUDIO_DEVICE,
		"audio-device",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)MM_AUDIO_DEVICE_MIC},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		MM_AUDIO_DEVICE_NUM-1,
		NULL,
	},
	// 2
	{
		MM_CAM_CAMERA_DEVICE_COUNT,
		"camera-device-count",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)MM_VIDEO_DEVICE_NUM},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		MM_VIDEO_DEVICE_NONE,
		MM_VIDEO_DEVICE_NUM,
		NULL,
	},
	// 3
	{
		MM_CAM_AUDIO_ENCODER,
		"audio-encoder",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)MM_AUDIO_CODEC_AMR},
		MM_ATTRS_VALID_TYPE_INT_ARRAY,
		(int)NULL,
		0,
		NULL,
	},
	// 4
	{
		MM_CAM_VIDEO_ENCODER,
		"video-encoder",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)MM_VIDEO_CODEC_MPEG4},
		MM_ATTRS_VALID_TYPE_INT_ARRAY,
		(int)NULL,
		0,
		NULL,
	},
	//5
	{
		MM_CAM_IMAGE_ENCODER,
		"image-encoder",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)MM_IMAGE_CODEC_JPEG},
		MM_ATTRS_VALID_TYPE_INT_ARRAY,
		(int)NULL,
		0,
		NULL,
	},
	//6
	{
		MM_CAM_FILE_FORMAT,
		"file-format",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)MM_FILE_FORMAT_MP4},
		MM_ATTRS_VALID_TYPE_INT_ARRAY,
		(int)NULL,
		0,
		NULL,
	},
	//7
	{
		MM_CAM_CAMERA_DEVICE_NAME,
		"camera-device-name",
		MMF_VALUE_TYPE_STRING,
		MM_ATTRS_FLAG_RW,
		{(void*)NULL},
		MM_ATTRS_VALID_TYPE_NONE,
		0,
		0,
		NULL,
	},
	//8
	{
		MM_CAM_AUDIO_SAMPLERATE,
		"audio-samplerate",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)8000},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		_MMCAMCORDER_MAX_INT,
		NULL,
	},
	//9
	{
		MM_CAM_AUDIO_FORMAT,
		"audio-format",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)MM_CAMCORDER_AUDIO_FORMAT_PCM_S16_LE},
		MM_ATTRS_VALID_TYPE_INT_ARRAY,
		(int)depth,
		ARRAY_SIZE(depth),
		NULL,
	},
	//10
	{
		MM_CAM_AUDIO_CHANNEL,
		"audio-channel",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)2},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		1,
		2,
		NULL,
	},
	//11
	{
		MM_CAM_AUDIO_VOLUME,
		"audio-volume",
		MMF_VALUE_TYPE_DOUBLE,
		MM_ATTRS_FLAG_RW,
		{(void*)1},
		MM_ATTRS_VALID_TYPE_DOUBLE_RANGE,
		0,
		10.0,
		_mmcamcorder_commit_audio_volume,
	},
	//12
	{
		MM_CAM_AUDIO_INPUT_ROUTE,
		"audio-input-route",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)MM_AUDIOROUTE_USE_EXTERNAL_SETTING},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		MM_AUDIOROUTE_USE_EXTERNAL_SETTING,
		MM_AUDIOROUTE_CAPTURE_STEREOMIC_ONLY,
		_mmcamcorder_commit_audio_input_route,
	},
	//13
	{
		MM_CAM_FILTER_SCENE_MODE,
		"filter-scene-mode",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_ARRAY,
		0,
		0,
		_mmcamcorder_commit_filter_scene_mode,
	},
	//14
	{
		MM_CAM_FILTER_BRIGHTNESS,
		"filter-brightness",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)1},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		-1,
		_mmcamcorder_commit_filter,
	},
	//15
	{
		MM_CAM_FILTER_CONTRAST,
		"filter-contrast",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		-1,
		_mmcamcorder_commit_filter,
	},
	//16
	{
		MM_CAM_FILTER_WB,
		"filter-wb",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_ARRAY,
		0,
		0,
		_mmcamcorder_commit_filter,
	},
	//17
	{
		MM_CAM_FILTER_COLOR_TONE,
		"filter-color-tone",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_ARRAY,
		0,
		0,
		_mmcamcorder_commit_filter,
	},
	//18
	{
		MM_CAM_FILTER_SATURATION,
		"filter-saturation",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		-1,
		_mmcamcorder_commit_filter,
	},
	//19
	{
		MM_CAM_FILTER_HUE,
		"filter-hue",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		-1,
		_mmcamcorder_commit_filter,
	},
	//20
	{
		MM_CAM_FILTER_SHARPNESS,
		"filter-sharpness",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		-1,
		_mmcamcorder_commit_filter,
	},
	//21
	{
		MM_CAM_CAMERA_FORMAT,
		"camera-format",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)MM_PIXEL_FORMAT_YUYV},
		MM_ATTRS_VALID_TYPE_INT_ARRAY,
		0,
		0,
		NULL,
	},
	//22
	{
		MM_CAM_CAMERA_RECORDING_MOTION_RATE,
		"camera-recording-motion-rate",
		MMF_VALUE_TYPE_DOUBLE,
		MM_ATTRS_FLAG_RW,
		{(void*)1},
		MM_ATTRS_VALID_TYPE_DOUBLE_RANGE,
		0,
		_MMCAMCORDER_MAX_INT,
		_mmcamcorder_commit_camera_recording_motion_rate,
	},
	//23
	{
		MM_CAM_CAMERA_FPS,
		"camera-fps",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)30},
		MM_ATTRS_VALID_TYPE_INT_ARRAY,
		0,
		0,
		_mmcamcorder_commit_camera_fps,
	},
	//24
	{
		MM_CAM_CAMERA_WIDTH,
		"camera-width",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)MMCAMCORDER_DEFAULT_CAMERA_WIDTH},
		MM_ATTRS_VALID_TYPE_INT_ARRAY,
		0,
		0,
		_mmcamcorder_commit_camera_width,
	},
	//25
	{
		MM_CAM_CAMERA_HEIGHT,
		"camera-height",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)MMCAMCORDER_DEFAULT_CAMERA_HEIGHT},
		MM_ATTRS_VALID_TYPE_INT_ARRAY,
		0,
		0,
		_mmcamcorder_commit_camera_height,
	},
	//26
	{
		MM_CAM_CAMERA_DIGITAL_ZOOM,
		"camera-digital-zoom",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)10},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		-1,
		_mmcamcorder_commit_camera_zoom,
	},
	//27
	{
		MM_CAM_CAMERA_OPTICAL_ZOOM,
		"camera-optical-zoom",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		-1,
		_mmcamcorder_commit_camera_zoom,
	},
	//28
	{
		MM_CAM_CAMERA_FOCUS_MODE,
		"camera-focus-mode",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)MM_CAMCORDER_FOCUS_MODE_NONE},
		MM_ATTRS_VALID_TYPE_INT_ARRAY,
		0,
		0,
		_mmcamcorder_commit_camera_focus_mode,
	},
	//29
	{
		MM_CAM_CAMERA_AF_SCAN_RANGE,
		"camera-af-scan-range",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_ARRAY,
		0,
		0,
		_mmcamcorder_commit_camera_af_scan_range,
	},
	//30
	{
		MM_CAM_CAMERA_EXPOSURE_MODE,
		"camera-exposure-mode",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_ARRAY,
		0,
		0,
		_mmcamcorder_commit_camera_capture_mode,
	},
	//31
	{
		MM_CAM_CAMERA_EXPOSURE_VALUE,
		"camera-exposure-value",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		-1,
		_mmcamcorder_commit_camera_capture_mode,
	},
	//32
	{
		MM_CAM_CAMERA_F_NUMBER,
		"camera-f-number",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_ARRAY,
		0,
		0,
		_mmcamcorder_commit_camera_capture_mode,
	},
	//33
	{
		MM_CAM_CAMERA_SHUTTER_SPEED,
		"camera-shutter-speed",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_ARRAY,
		0,
		0,
		_mmcamcorder_commit_camera_capture_mode,
	},
	//34
	{
		MM_CAM_CAMERA_ISO,
		"camera-iso",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_ARRAY,
		0,
		0,
		_mmcamcorder_commit_camera_capture_mode,
	},
	//35
	{
		MM_CAM_CAMERA_WDR,
		"camera-wdr",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_ARRAY,
		0,
		0,
		_mmcamcorder_commit_camera_wdr,
	},
	//36
	{
		MM_CAM_CAMERA_ANTI_HANDSHAKE,
		"camera-anti-handshake",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_ARRAY,
		0,
		0,
		_mmcamcorder_commit_camera_anti_handshake,
	},
	//37
	{
		MM_CAM_CAMERA_FPS_AUTO,
		"camera-fps-auto",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)FALSE},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		1,
		NULL,
	},
	//38
	{
		MM_CAM_CAMERA_HOLD_AF_AFTER_CAPTURING,
		"camera-hold-af-after-capturing",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		1,
		_mmcamcorder_commit_camera_hold_af_after_capturing,
	},
	//39
	{
		MM_CAM_CAMERA_DELAY_ATTR_SETTING,
		"camera-delay-attr-setting",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)FALSE},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		1,
		NULL,
	},
	//40
	{
		MM_CAM_AUDIO_ENCODER_BITRATE,
		"audio-encoder-bitrate",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		_MMCAMCORDER_MAX_INT,
		NULL,
	},
	//41
	{
		MM_CAM_VIDEO_ENCODER_BITRATE,
		"video-encoder-bitrate",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		_MMCAMCORDER_MAX_INT,
		NULL,
	},
	//42
	{
		MM_CAM_IMAGE_ENCODER_QUALITY,
		"image-encoder-quality",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)95},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		-1,
		_mmcamcorder_commit_image_encoder_quality,
	},
	//43
	{
		MM_CAM_CAPTURE_FORMAT,
		"capture-format",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)MM_PIXEL_FORMAT_ENCODED},
		MM_ATTRS_VALID_TYPE_INT_ARRAY,
		0,
		0,
		NULL,
	},
	//44
	{
		MM_CAM_CAPTURE_WIDTH,
		"capture-width",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)1600},
		MM_ATTRS_VALID_TYPE_INT_ARRAY,
		0,
		0,
		_mmcamcorder_commit_capture_width ,
	},
	//45
	{
		MM_CAM_CAPTURE_HEIGHT,
		"capture-height",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)1200},
		MM_ATTRS_VALID_TYPE_INT_ARRAY,
		0,
		0,
		_mmcamcorder_commit_capture_height,
	},
	//46
	{
		MM_CAM_CAPTURE_COUNT,
		"capture-count",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)1},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		-1,
		_mmcamcorder_commit_capture_count,
	},
	//47
	{
		MM_CAM_CAPTURE_INTERVAL,
		"capture-interval",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		_MMCAMCORDER_MAX_INT,
		NULL,
	},
	//48
	{
		MM_CAM_CAPTURE_BREAK_CONTINUOUS_SHOT,
		"capture-break-cont-shot",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)FALSE},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		1,
		_mmcamcorder_commit_capture_break_cont_shot,
	},
	//49
	{
		MM_CAM_DISPLAY_HANDLE,
		"display-handle",
		MMF_VALUE_TYPE_DATA,
		MM_ATTRS_FLAG_RW,
		{(void*)NULL},
		MM_ATTRS_VALID_TYPE_NONE,
		0,
		0,
		_mmcamcorder_commit_display_handle,
	},
	//50
	{
		MM_CAM_DISPLAY_DEVICE,
		"display-device",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)MM_DISPLAY_DEVICE_MAINLCD},
		MM_ATTRS_VALID_TYPE_INT_ARRAY,
		0,
		0,
		NULL,
	},
	//51
	{
		MM_CAM_DISPLAY_SURFACE,
		"display-surface",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)MM_DISPLAY_SURFACE_X},
		MM_ATTRS_VALID_TYPE_INT_ARRAY,
		0,
		0,
		NULL,
	},
	//52
	{
		MM_CAM_DISPLAY_RECT_X,
		"display-rect-x",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		_MMCAMCORDER_MAX_INT,
		_mmcamcorder_commit_display_rect,
	},
	//53
	{
		MM_CAM_DISPLAY_RECT_Y,
		"display-rect-y",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		_MMCAMCORDER_MAX_INT,
		_mmcamcorder_commit_display_rect,
	},
	//54
	{
		MM_CAM_DISPLAY_RECT_WIDTH,
		"display-rect-width",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		_MMCAMCORDER_MAX_INT,
		_mmcamcorder_commit_display_rect,
	},
	//55
	{
		MM_CAM_DISPLAY_RECT_HEIGHT,
		"display-rect-height",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		_MMCAMCORDER_MAX_INT,
		_mmcamcorder_commit_display_rect,
	},
	//56
	{
		MM_CAM_DISPLAY_SOURCE_X,
		"display-src-x",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		_MMCAMCORDER_MAX_INT,
		NULL,
	},
	//57
	{
		MM_CAM_DISPLAY_SOURCE_Y,
		"display-src-y",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		_MMCAMCORDER_MAX_INT,
		NULL,
	},
	//58
	{
		MM_CAM_DISPLAY_SOURCE_WIDTH,
		"display-src-width",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		_MMCAMCORDER_MAX_INT,
		NULL,
	},
	//59
	{
		MM_CAM_DISPLAY_SOURCE_HEIGHT,
		"display-src-height",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		_MMCAMCORDER_MAX_INT,
		NULL,
	},
	//60
	{
		MM_CAM_DISPLAY_ROTATION,
		"display-rotation",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)MM_DISPLAY_ROTATION_NONE},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		MM_DISPLAY_ROTATION_NONE,
		MM_DISPLAY_ROTATION_270,
		_mmcamcorder_commit_display_rotation,
	},
	//61
	{
		MM_CAM_DISPLAY_VISIBLE,
		"display-visible",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)1},
		MM_ATTRS_VALID_TYPE_INT_ARRAY,
		(int)visible_values,
		ARRAY_SIZE(visible_values),
		_mmcamcorder_commit_display_visible,
	},
	//62
	{
		MM_CAM_DISPLAY_SCALE,
		"display-scale",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		MM_DISPLAY_SCALE_DEFAULT,
		MM_DISPLAY_SCALE_TRIPLE_LENGTH,
		_mmcamcorder_commit_display_scale,
	},
	//63
	{
		MM_CAM_DISPLAY_GEOMETRY_METHOD,
		"display-geometry-method",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		MM_DISPLAY_METHOD_LETTER_BOX,
		MM_DISPLAY_METHOD_CUSTOM_ROI,
		_mmcamcorder_commit_display_geometry_method,
	},
	//64
	{
		MM_CAM_TARGET_FILENAME,
		"target-filename",
		MMF_VALUE_TYPE_STRING,
		MM_ATTRS_FLAG_RW,
		{(void*)"/tmp/CAM-NONAME"},
		MM_ATTRS_VALID_TYPE_NONE,
		0,
		0,
		_mmcamcorder_commit_target_filename,
	},
	//65
	{
		MM_CAM_TARGET_MAX_SIZE,
		"target-max-size",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		_MMCAMCORDER_MAX_INT,
		NULL,
	},
	//66
	{
		MM_CAM_TARGET_TIME_LIMIT,
		"target-time-limit",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		_MMCAMCORDER_MAX_INT,
		NULL,
	},
	//67
	{
		MM_CAM_TAG_ENABLE,
		"tag-enable",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		1,
		NULL,
	},
	//68
	{
		MM_CAM_TAG_IMAGE_DESCRIPTION,
		"tag-image-description",
		MMF_VALUE_TYPE_STRING,
		MM_ATTRS_FLAG_RW,
		{(void*)NULL},
		MM_ATTRS_VALID_TYPE_NONE,
		0,
		0,
		NULL,
	},
	//69
	{
		MM_CAM_TAG_ORIENTATION,
		"tag-orientation",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)1},
		MM_ATTRS_VALID_TYPE_INT_ARRAY,
		(int)tag_orientation_values,
		ARRAY_SIZE(tag_orientation_values),
		NULL,
	},
	//70
	{
		MM_CAM_TAG_SOFTWARE,
		"tag-software",
		MMF_VALUE_TYPE_STRING,
		MM_ATTRS_FLAG_RW,
		{(void*)NULL},
		MM_ATTRS_VALID_TYPE_NONE,
		0,
		0,
		NULL,
	},
	//71
	{
		MM_CAM_TAG_LATITUDE,
		"tag-latitude",
		MMF_VALUE_TYPE_DOUBLE,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_DOUBLE_RANGE,
		-360,
		360,
		NULL,
	},
	//72
	{
		MM_CAM_TAG_LONGITUDE,
		"tag-longitude",
		MMF_VALUE_TYPE_DOUBLE,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_DOUBLE_RANGE,
		-360,
		360,
		NULL,
	},
	//73
	{
		MM_CAM_TAG_ALTITUDE,
		"tag-altitude",
		MMF_VALUE_TYPE_DOUBLE,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_DOUBLE_RANGE,
		-999999,
		999999,
		NULL,
	},
	//74
	{
		MM_CAM_STROBE_CONTROL,
		"strobe-control",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		-1,
		_mmcamcorder_commit_strobe,
	},
	//75
	{
		MM_CAM_STROBE_CAPABILITIES,
		"strobe-capabilities",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		-1,
		_mmcamcorder_commit_strobe,
	},
	//76
	{
		MM_CAM_STROBE_MODE,
		"strobe-mode",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_ARRAY,
		0,
		0,
		_mmcamcorder_commit_strobe,
	},
	//77
	{
		MM_CAM_DETECT_MODE,
		"detect-mode",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_ARRAY,
		0,
		0,
		_mmcamcorder_commit_detect,
	},
	//78
	{
		MM_CAM_DETECT_NUMBER,
		"detect-number",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		-1,
		_mmcamcorder_commit_detect,
	},
	//79
	{
		MM_CAM_DETECT_FOCUS_SELECT,
		"detect-focus-select",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		-1,
		_mmcamcorder_commit_detect,
	},
	//80
	{
		MM_CAM_DETECT_SELECT_NUMBER,
		"detect-select-number",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		-1,
		_mmcamcorder_commit_detect,
	},
	//81
	{
		MM_CAM_DETECT_STATUS,
		"detect-status",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_ARRAY,
		0,
		0,
		_mmcamcorder_commit_detect,
	},
	//82
	{
		MM_CAM_CAPTURE_ZERO_SYSTEMLAG,
		"capture-zero-systemlag",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)FALSE},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		1,
		NULL,
	},
	//83
	{
		MM_CAM_CAMERA_AF_TOUCH_X,
		"camera-af-touch-x",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		_MMCAMCORDER_MAX_INT,
		_mmcamcorder_commit_camera_af_touch_area,
	},
	//84
	{
		MM_CAM_CAMERA_AF_TOUCH_Y,
		"camera-af-touch-y",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		_MMCAMCORDER_MAX_INT,
		_mmcamcorder_commit_camera_af_touch_area,
	},
	//85
	{
		MM_CAM_CAMERA_AF_TOUCH_WIDTH,
		"camera-af-touch-width",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		_MMCAMCORDER_MAX_INT,
		_mmcamcorder_commit_camera_af_touch_area,
	},
	//86
	{
		MM_CAM_CAMERA_AF_TOUCH_HEIGHT,
		"camera-af-touch-height",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		_MMCAMCORDER_MAX_INT,
		_mmcamcorder_commit_camera_af_touch_area,
	},
	//87
	{
		MM_CAM_CAMERA_FOCAL_LENGTH,
		"camera-focal-length",
		MMF_VALUE_TYPE_DOUBLE,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_DOUBLE_RANGE,
		0,
		1000,
		_mmcamcorder_commit_camera_capture_mode,
	},
	//88
	{
		MM_CAM_RECOMMEND_PREVIEW_FORMAT_FOR_CAPTURE,
		"recommend-preview-format-for-capture",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)MM_PIXEL_FORMAT_YUYV},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		MM_PIXEL_FORMAT_NV12,
		MM_PIXEL_FORMAT_ITLV_JPEG_UYVY,
		NULL,
	},
	//89
	{
		MM_CAM_RECOMMEND_PREVIEW_FORMAT_FOR_RECORDING,
		"recommend-preview-format-for-recording",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)MM_PIXEL_FORMAT_NV12},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		MM_PIXEL_FORMAT_NV12,
		MM_PIXEL_FORMAT_ITLV_JPEG_UYVY,
		NULL,
	},
	//90
	{
		MM_CAM_CAPTURE_THUMBNAIL,
		"capture-thumbnail",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)TRUE},
		MM_ATTRS_VALID_TYPE_NONE,
		0,
		0,
		NULL,
	},
	//91
	{
		MM_CAM_TAG_GPS_ENABLE,
		"tag-gps-enable",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)TRUE},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		1,
		NULL,
	},
	//92
	{
		MM_CAM_TAG_GPS_TIME_STAMP,
		"tag-gps-time-stamp",
		MMF_VALUE_TYPE_DOUBLE,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_NONE,
		0,
		0,
		NULL,
	},
	//93
	{
		MM_CAM_TAG_GPS_DATE_STAMP,
		"tag-gps-date-stamp",
		MMF_VALUE_TYPE_STRING,
		MM_ATTRS_FLAG_RW,
		{(void*)NULL},
		MM_ATTRS_VALID_TYPE_NONE,
		0,
		0,
		NULL,
	},
	//94
	{
		MM_CAM_TAG_GPS_PROCESSING_METHOD,
		"tag-gps-processing-method",
		MMF_VALUE_TYPE_STRING,
		MM_ATTRS_FLAG_RW,
		{(void*)NULL},
		MM_ATTRS_VALID_TYPE_NONE,
		0,
		0,
		NULL,
	},
	//95
	{
		MM_CAM_CAMERA_ROTATION,
		"camera-rotation",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)MM_VIDEO_INPUT_ROTATION_NONE},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		MM_VIDEO_INPUT_ROTATION_NONE,
		MM_VIDEO_INPUT_ROTATION_270,
		_mmcamcorder_commit_camera_rotate,
	},
	//96
	{
		MM_CAM_ENABLE_CONVERTED_STREAM_CALLBACK,
		"enable-converted-stream-callback",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		1,
		NULL,
	},
	//97
	{
		MM_CAM_CAPTURED_SCREENNAIL,
		"captured-screennail",
		MMF_VALUE_TYPE_DATA,
		MM_ATTRS_FLAG_READABLE,
		{(void*)NULL},
		MM_ATTRS_VALID_TYPE_NONE,
		0,
		0,
		NULL,
	},
	//98
	{
		MM_CAM_CAPTURE_SOUND_ENABLE,
		"capture-sound-enable",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)TRUE},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		1,
		_mmcamcorder_commit_capture_sound_enable,
	},
	//99
	{
		MM_CAM_RECOMMEND_DISPLAY_ROTATION,
		"recommend-display-rotation",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)MM_DISPLAY_ROTATION_270},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		MM_DISPLAY_ROTATION_NONE,
		MM_DISPLAY_ROTATION_270,
		NULL,
	},
	//100
	{
		MM_CAM_CAMERA_FLIP,
		"camera-flip",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)MM_FLIP_NONE},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		MM_FLIP_NONE,
		MM_FLIP_BOTH,
		_mmcamcorder_commit_camera_flip,
	},
	//101
	{
		MM_CAM_CAMERA_HDR_CAPTURE,
		"camera-hdr-capture",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)FALSE},
		MM_ATTRS_VALID_TYPE_INT_ARRAY,
		0,
		0,
		_mmcamcorder_commit_camera_hdr_capture,
	},
	//102
	{
		MM_CAM_DISPLAY_MODE,
		"display-mode",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)MM_DISPLAY_MODE_DEFAULT},
		MM_ATTRS_VALID_TYPE_INT_ARRAY,
		0,
		0,
		_mmcamcorder_commit_display_mode,
	},
	//103
	{
		MM_CAM_CAMERA_FACE_ZOOM_X,
		"camera-face-zoom-x",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		_MMCAMCORDER_MAX_INT,
		_mmcamcorder_commit_camera_face_zoom,
	},
	//104
	{
		MM_CAM_CAMERA_FACE_ZOOM_Y,
		"camera-face-zoom-y",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		_MMCAMCORDER_MAX_INT,
		_mmcamcorder_commit_camera_face_zoom,
	},
	//105
	{
		MM_CAM_CAMERA_FACE_ZOOM_LEVEL,
		"camera-face-zoom-level",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)0},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		0,
		-1,
		NULL,
	},
	//106
	{
		MM_CAM_CAMERA_FACE_ZOOM_MODE,
		"camera-face-zoom-mode",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)FALSE},
		MM_ATTRS_VALID_TYPE_INT_ARRAY,
		0,
		0,
		_mmcamcorder_commit_camera_face_zoom,
	},
	//107
	{
		MM_CAM_AUDIO_DISABLE,
		"audio-disable",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)FALSE},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		FALSE,
		TRUE,
		_mmcamcorder_commit_audio_disable,
	},
	//108
	{
		MM_CAM_RECOMMEND_CAMERA_WIDTH,
		"recommend-camera-width",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)MMCAMCORDER_DEFAULT_CAMERA_WIDTH},
		MM_ATTRS_VALID_TYPE_INT_ARRAY,
		0,
		0,
		NULL,
	},
	//109
	{
		MM_CAM_RECOMMEND_CAMERA_HEIGHT,
		"recommend-camera-height",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)MMCAMCORDER_DEFAULT_CAMERA_HEIGHT},
		MM_ATTRS_VALID_TYPE_INT_ARRAY,
		0,
		0,
		NULL,
	},
	//110
	{
		MM_CAM_CAPTURED_EXIF_RAW_DATA,
		"captured-exif-raw-data",
		MMF_VALUE_TYPE_DATA,
		MM_ATTRS_FLAG_READABLE,
		{(void*)NULL},
		MM_ATTRS_VALID_TYPE_NONE,
		0,
		0,
		NULL,
	},
	//111
	{
		MM_CAM_DISPLAY_EVAS_SURFACE_SINK,
		"display-evas-surface-sink",
		MMF_VALUE_TYPE_STRING,
		MM_ATTRS_FLAG_READABLE,
		{(void*)NULL},
		MM_ATTRS_VALID_TYPE_NONE,
		0,
		0,
		NULL,
	},
	//112
	{
		MM_CAM_DISPLAY_EVAS_DO_SCALING,
		"display-evas-do-scaling",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)TRUE},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		FALSE,
		TRUE,
		_mmcamcorder_commit_display_evas_do_scaling,
	},
	//113
	{
		MM_CAM_CAMERA_FACING_DIRECTION,
		"camera-facing-direction",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)MM_CAMCORDER_CAMERA_FACING_DIRECTION_REAR},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		MM_CAMCORDER_CAMERA_FACING_DIRECTION_REAR,
		MM_CAMCORDER_CAMERA_FACING_DIRECTION_FRONT,
		NULL,
	},
	//114
	{
		MM_CAM_DISPLAY_FLIP,
		"display-flip",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)MM_FLIP_NONE},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		MM_FLIP_NONE,
		MM_FLIP_BOTH,
		_mmcamcorder_commit_display_flip,
	},
	//115
	{
		MM_CAM_CAMERA_VIDEO_STABILIZATION,
		"camera-video-stabilization",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)MM_CAMCORDER_VIDEO_STABILIZATION_OFF},
		MM_ATTRS_VALID_TYPE_INT_ARRAY,
		0,
		0,
		_mmcamcorder_commit_camera_video_stabilization,
	},
	//118
	{
		MM_CAM_TAG_VIDEO_ORIENTATION,
		"tag-video-orientation",
		MMF_VALUE_TYPE_INT,
		MM_ATTRS_FLAG_RW,
		{(void*)MM_CAMCORDER_TAG_VIDEO_ORT_NONE},
		MM_ATTRS_VALID_TYPE_INT_RANGE,
		MM_CAMCORDER_TAG_VIDEO_ORT_NONE,
		MM_CAMCORDER_TAG_VIDEO_ORT_270,
		NULL,
	}
};


/*-----------------------------------------------------------------------
|    LOCAL VARIABLE DEFINITIONS for internal				|
-----------------------------------------------------------------------*/
/* 	Readonly attributes list.
*	If you want to make some attributes read only, write down here.
*	It will make them read only after composing whole attributes.
*/

static int readonly_attributes[] = {
	MM_CAM_CAMERA_DEVICE_COUNT,
	MM_CAM_CAMERA_DEVICE_NAME,
	MM_CAM_CAMERA_FACING_DIRECTION,
	MM_CAM_CAMERA_SHUTTER_SPEED,
	MM_CAM_RECOMMEND_PREVIEW_FORMAT_FOR_CAPTURE,
	MM_CAM_RECOMMEND_PREVIEW_FORMAT_FOR_RECORDING,
	MM_CAM_CAPTURED_SCREENNAIL,
	MM_CAM_RECOMMEND_DISPLAY_ROTATION,
};

/*-----------------------------------------------------------------------
|    LOCAL FUNCTION PROTOTYPES:						|
-----------------------------------------------------------------------*/
/* STATIC INTERNAL FUNCTION */
static bool __mmcamcorder_set_capture_resolution(MMHandleType handle, int width, int height);
static bool __mmcamcorder_set_camera_resolution(MMHandleType handle, int width, int height);
static int  __mmcamcorder_set_conf_to_valid_info(MMHandleType handle);
static int  __mmcamcorder_release_conf_valid_info(MMHandleType handle);
static bool __mmcamcorder_attrs_is_supported(MMHandleType handle, int idx);
static int  __mmcamcorder_check_valid_pair(MMHandleType handle, char **err_attr_name, const char *attribute_name, va_list var_args);

/*=======================================================================
|  FUNCTION DEFINITIONS							|
=======================================================================*/
/*-----------------------------------------------------------------------
|    GLOBAL FUNCTION DEFINITIONS:					|
-----------------------------------------------------------------------*/
MMHandleType
_mmcamcorder_alloc_attribute( MMHandleType handle, MMCamPreset *info )
{
	_mmcam_dbg_log( "" );

	MMHandleType attrs = 0;
	mmf_attrs_construct_info_t *attrs_const_info = NULL;
	int attr_count = 0;
	int idx;

	/* Create attribute constructor */
	_mmcam_dbg_log("start");

	/* alloc 'mmf_attrs_construct_info_t' */
	attr_count = ARRAY_SIZE(cam_attrs_const_info);
	attrs_const_info = malloc(attr_count * sizeof(mmf_attrs_construct_info_t));

	if (!attrs_const_info) {
		_mmcam_dbg_err("Fail to alloc constructor.");
		return 0;
	}

	for (idx = 0 ; idx < attr_count ; idx++) {
		/* attribute order check. This should be same. */
		if (idx != cam_attrs_const_info[idx].attrid) {
			_mmcam_dbg_err("Please check attributes order. Is the idx same with enum val?");
			return 0;
		}

		attrs_const_info[idx].name = cam_attrs_const_info[idx].name;
		attrs_const_info[idx].value_type = cam_attrs_const_info[idx].value_type;
		attrs_const_info[idx].flags = cam_attrs_const_info[idx].flags;
		attrs_const_info[idx].default_value = cam_attrs_const_info[idx].default_value.value_void;
	}

	/* Camcorder Attributes */
	_mmcam_dbg_log("Create Camcorder Attributes[%p, %d]", attrs_const_info, attr_count);

	attrs = mmf_attrs_new_from_data("Camcorder_Attributes",
	                                attrs_const_info,
	                                attr_count,
	                                _mmcamcorder_commit_camcorder_attrs,
	                                (void *)handle);

	free(attrs_const_info);
	attrs_const_info = NULL;

	if (attrs == 0) {
		_mmcam_dbg_err("Fail to alloc attribute handle");
		return 0;
	}

	__mmcamcorder_set_conf_to_valid_info(handle);

	for (idx = 0; idx < attr_count; idx++)
	{
/*		_mmcam_dbg_log("Valid type [%s:%d, %d, %d]", cam_attrs_const_info[idx].name, cam_attrs_const_info[idx].validity_type
			, cam_attrs_const_info[idx].validity_value1, cam_attrs_const_info[idx].validity_value2);
*/
		mmf_attrs_set_valid_type (attrs, idx, cam_attrs_const_info[idx].validity_type);

		switch (cam_attrs_const_info[idx].validity_type)
		{
			case MM_ATTRS_VALID_TYPE_INT_ARRAY:
				if (cam_attrs_const_info[idx].validity_value1 &&
				    cam_attrs_const_info[idx].validity_value2 > 0) {
					mmf_attrs_set_valid_array(attrs, idx,
					                          (const int *)(cam_attrs_const_info[idx].validity_value1),
					                          cam_attrs_const_info[idx].validity_value2,
					                          (int)(cam_attrs_const_info[idx].default_value.value_int));
				}
			break;
			case MM_ATTRS_VALID_TYPE_INT_RANGE:
				mmf_attrs_set_valid_range(attrs, idx,
				                          cam_attrs_const_info[idx].validity_value1,
				                          cam_attrs_const_info[idx].validity_value2,
					                  (int)(cam_attrs_const_info[idx].default_value.value_int));
			break;
			case MM_ATTRS_VALID_TYPE_DOUBLE_ARRAY:
				if (cam_attrs_const_info[idx].validity_value1 &&
				    cam_attrs_const_info[idx].validity_value2 > 0) {
					mmf_attrs_set_valid_double_array(attrs, idx,
					                                 (const double *)(cam_attrs_const_info[idx].validity_value1),
					                                 cam_attrs_const_info[idx].validity_value2,
					                                 (double)(cam_attrs_const_info[idx].default_value.value_double));
				}
			break;
			case MM_ATTRS_VALID_TYPE_DOUBLE_RANGE:
				mmf_attrs_set_valid_double_range(attrs, idx,
				                                 (double)(cam_attrs_const_info[idx].validity_value1),
				                                 (double)(cam_attrs_const_info[idx].validity_value2),
				                                 (double)(cam_attrs_const_info[idx].default_value.value_double));
			break;
			case MM_ATTRS_VALID_TYPE_NONE:
			break;
			case MM_ATTRS_VALID_TYPE_INVALID:
			default:
				_mmcam_dbg_err("Valid type error.");
			break;
		}
	}

	__mmcamcorder_release_conf_valid_info(handle);

	return attrs;
}


void
_mmcamcorder_dealloc_attribute(MMHandleType attrs)
{
	_mmcam_dbg_log("");

	if (attrs)
	{
		mmf_attrs_free(attrs);

		_mmcam_dbg_log("released attribute");
	}
}


int 
_mmcamcorder_get_attributes(MMHandleType handle,  char **err_attr_name, const char *attribute_name, va_list var_args)
{
	MMHandleType attrs = 0;
	int ret = MM_ERROR_NONE;

	mmf_return_val_if_fail( handle, MM_ERROR_CAMCORDER_INVALID_ARGUMENT );
//	mmf_return_val_if_fail( err_attr_name, MM_ERROR_CAMCORDER_INVALID_ARGUMENT );

	attrs = MMF_CAMCORDER_ATTRS(handle);
	mmf_return_val_if_fail( attrs, MM_ERROR_CAMCORDER_NOT_INITIALIZED );

	ret = mm_attrs_get_valist(attrs, err_attr_name, attribute_name, var_args);

	return ret;
}


int 
_mmcamcorder_set_attributes(MMHandleType handle, char **err_attr_name, const char *attribute_name, va_list var_args)
{
	MMHandleType attrs = 0;
	int ret = MM_ERROR_NONE;

	mmf_return_val_if_fail( handle, MM_ERROR_CAMCORDER_INVALID_ARGUMENT );
//	mmf_return_val_if_fail( err_attr_name, MM_ERROR_CAMCORDER_INVALID_ARGUMENT );

	attrs = MMF_CAMCORDER_ATTRS(handle);
	mmf_return_val_if_fail( attrs, MM_ERROR_CAMCORDER_NOT_INITIALIZED );

	__ta__( "__mmcamcorder_check_valid_pair",
	ret = __mmcamcorder_check_valid_pair( handle, err_attr_name, attribute_name, var_args );
	);

	if (ret == MM_ERROR_NONE) {
		ret = mm_attrs_set_valist(attrs, err_attr_name, attribute_name, var_args);
	}

	return ret;
}


int 
_mmcamcorder_get_attribute_info(MMHandleType handle, const char *attr_name, MMCamAttrsInfo *info)
{
	MMHandleType attrs = 0;
	MMAttrsInfo attrinfo;
	int ret = MM_ERROR_NONE;

	mmf_return_val_if_fail( handle, MM_ERROR_CAMCORDER_INVALID_ARGUMENT );
	mmf_return_val_if_fail( attr_name, MM_ERROR_CAMCORDER_INVALID_ARGUMENT );
	mmf_return_val_if_fail( info, MM_ERROR_CAMCORDER_INVALID_ARGUMENT );

	attrs = MMF_CAMCORDER_ATTRS(handle);
	mmf_return_val_if_fail( attrs, MM_ERROR_CAMCORDER_INVALID_ARGUMENT);

	ret = mm_attrs_get_info_by_name(attrs, attr_name, (MMAttrsInfo*)&attrinfo);

	if (ret == MM_ERROR_NONE)
	{
		memset(info, 0x00, sizeof(MMCamAttrsInfo));
		info->type = attrinfo.type;
		info->flag = attrinfo.flag;
		info->validity_type= attrinfo.validity_type;

		switch(attrinfo.validity_type)
		{
			case MM_ATTRS_VALID_TYPE_INT_ARRAY:
				info->int_array.array = attrinfo.int_array.array;
				info->int_array.count = attrinfo.int_array.count;
				info->int_array.def = attrinfo.int_array.dval;
			break;
			case MM_ATTRS_VALID_TYPE_INT_RANGE:
				info->int_range.min = attrinfo.int_range.min;
				info->int_range.max = attrinfo.int_range.max;
				info->int_range.def = attrinfo.int_range.dval;
			break;
			case MM_ATTRS_VALID_TYPE_DOUBLE_ARRAY:
				info->double_array.array = attrinfo.double_array.array;
				info->double_array.count = attrinfo.double_array.count;
				info->double_array.def = attrinfo.double_array.dval;
			break;
			case MM_ATTRS_VALID_TYPE_DOUBLE_RANGE:
				info->double_range.min = attrinfo.double_range.min;
				info->double_range.max = attrinfo.double_range.max;
				info->double_range.def = attrinfo.double_range.dval;
			break;
			case MM_ATTRS_VALID_TYPE_NONE:
			break;
			case MM_ATTRS_VALID_TYPE_INVALID:
			default:
			break;
		}
	}

	return ret;
}


//attribute commiter
void
__mmcamcorder_print_attrs (const char *attr_name, const mmf_value_t *value, char* cmt_way)
{
	switch(value->type)
	{
		case MMF_VALUE_TYPE_INT:
			_mmcam_dbg_log("%s :(%s:%d)", cmt_way, attr_name, value->value.i_val);
		break;
		case MMF_VALUE_TYPE_DOUBLE:
			_mmcam_dbg_log("%s :(%s:%f)", cmt_way, attr_name, value->value.d_val);
		break;
		case MMF_VALUE_TYPE_STRING:
			_mmcam_dbg_log("%s :(%s:%s)", cmt_way, attr_name, value->value.s_val);
		break;
		case MMF_VALUE_TYPE_DATA:
			_mmcam_dbg_log("%s :(%s:%p)", cmt_way, attr_name, value->value.p_val);
		break;
	}

	return;
}

bool
_mmcamcorder_commit_camcorder_attrs (int attr_idx, const char *attr_name, const mmf_value_t *value, void *commit_param)
{
	bool bret = FALSE;

	mmf_return_val_if_fail(commit_param, FALSE);
	mmf_return_val_if_fail(attr_idx >= 0, FALSE);
	mmf_return_val_if_fail(attr_name, FALSE);
	mmf_return_val_if_fail(value, FALSE);

	if (cam_attrs_const_info[attr_idx].attr_commit)
	{
//		_mmcam_dbg_log("Dynamic commit:(%s)", attr_name);
		__mmcamcorder_print_attrs(attr_name, value, "Dynamic");
		bret = cam_attrs_const_info[attr_idx].attr_commit((MMHandleType)commit_param, attr_idx, value);
	}
	else
	{
//		_mmcam_dbg_log("Static commit:(%s)", attr_name);
		__mmcamcorder_print_attrs(attr_name, value, "Static");
		bret = TRUE;
	}

	return bret;
}


int __mmcamcorder_set_conf_to_valid_info(MMHandleType handle)
{
	int *format = NULL;
	int total_count = 0;

	/* Audio encoder */
	total_count = _mmcamcorder_get_available_format(handle, CONFIGURE_CATEGORY_MAIN_AUDIO_ENCODER, &format);
	cam_attrs_const_info[MM_CAM_AUDIO_ENCODER].validity_value1 = (int)format;
	cam_attrs_const_info[MM_CAM_AUDIO_ENCODER].validity_value2 = (int)total_count;

	/* Video encoder */
	total_count = _mmcamcorder_get_available_format(handle, CONFIGURE_CATEGORY_MAIN_VIDEO_ENCODER, &format);
	cam_attrs_const_info[MM_CAM_VIDEO_ENCODER].validity_value1 = (int)format;
	cam_attrs_const_info[MM_CAM_VIDEO_ENCODER].validity_value2 = (int)total_count;

	/* Image encoder */
	total_count = _mmcamcorder_get_available_format(handle, CONFIGURE_CATEGORY_MAIN_IMAGE_ENCODER, &format);
	cam_attrs_const_info[MM_CAM_IMAGE_ENCODER].validity_value1 = (int)format;
	cam_attrs_const_info[MM_CAM_IMAGE_ENCODER].validity_value2 = (int)total_count;

	/* File format */
	total_count = _mmcamcorder_get_available_format(handle, CONFIGURE_CATEGORY_MAIN_MUX, &format);
	cam_attrs_const_info[MM_CAM_FILE_FORMAT].validity_value1 = (int)format;
	cam_attrs_const_info[MM_CAM_FILE_FORMAT].validity_value2 = (int)total_count;

	return MM_ERROR_NONE;
}


int __mmcamcorder_release_conf_valid_info(MMHandleType handle)
{
	int *allocated_memory = NULL;

	_mmcam_dbg_log("START");

	/* Audio encoder info */
	allocated_memory = (int*)(cam_attrs_const_info[MM_CAM_AUDIO_ENCODER].validity_value1);
	if (allocated_memory) {
		free(allocated_memory);
		cam_attrs_const_info[MM_CAM_AUDIO_ENCODER].validity_value1 = (int)NULL;
		cam_attrs_const_info[MM_CAM_AUDIO_ENCODER].validity_value2 = (int)0;
	}

	/* Video encoder info */
	allocated_memory = (int*)(cam_attrs_const_info[MM_CAM_VIDEO_ENCODER].validity_value1);
	if (allocated_memory) {
		free(allocated_memory);
		cam_attrs_const_info[MM_CAM_VIDEO_ENCODER].validity_value1 = (int)NULL;
		cam_attrs_const_info[MM_CAM_VIDEO_ENCODER].validity_value2 = (int)0;
	}

	/* Image encoder info */
	allocated_memory = (int*)(cam_attrs_const_info[MM_CAM_IMAGE_ENCODER].validity_value1);
	if (allocated_memory) {
		free(allocated_memory);
		cam_attrs_const_info[MM_CAM_IMAGE_ENCODER].validity_value1 = (int)NULL;
		cam_attrs_const_info[MM_CAM_IMAGE_ENCODER].validity_value2 = (int)0;
	}

	/* File format info */
	allocated_memory = (int*)(cam_attrs_const_info[MM_CAM_FILE_FORMAT].validity_value1);
	if (allocated_memory) {
		free(allocated_memory);
		cam_attrs_const_info[MM_CAM_FILE_FORMAT].validity_value1 = (int)NULL;
		cam_attrs_const_info[MM_CAM_FILE_FORMAT].validity_value2 = (int)0;
	}

	_mmcam_dbg_log("DONE");

	return MM_ERROR_NONE;
}


bool _mmcamcorder_commit_capture_width (MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	MMHandleType attr = 0;
	int current_state = MM_CAMCORDER_STATE_NONE;

	attr = MMF_CAMCORDER_ATTRS(handle);
	mmf_return_val_if_fail(attr, FALSE);

	_mmcam_dbg_log("(%d)", attr_idx);

	current_state = _mmcamcorder_get_state(handle);
	if (current_state <= MM_CAMCORDER_STATE_PREPARE) {
		int flags = MM_ATTRS_FLAG_NONE;
		int capture_width, capture_height;
		MMCamAttrsInfo info;

		mm_camcorder_get_attribute_info(handle, MMCAM_CAPTURE_HEIGHT, &info);
		flags = info.flag;

		if (!(flags & MM_ATTRS_FLAG_MODIFIED)) {
			mm_camcorder_get_attributes(handle, NULL, MMCAM_CAPTURE_HEIGHT, &capture_height, NULL);
			capture_width = value->value.i_val;

			/* Check whether they are valid pair */
			return __mmcamcorder_set_capture_resolution(handle, capture_width, capture_height);
		}

		return TRUE;
	} else {
		_mmcam_dbg_log("Capture resolution can't be set.(state=%d)", current_state);
		return FALSE;
	}
}


bool _mmcamcorder_commit_capture_height (MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	int current_state = MM_CAMCORDER_STATE_NONE;

	current_state = _mmcamcorder_get_state(handle);

	if (current_state <= MM_CAMCORDER_STATE_PREPARE) {
		int capture_width, capture_height;

		mm_camcorder_get_attributes(handle, NULL, MMCAM_CAPTURE_WIDTH, &capture_width, NULL);
		capture_height = value->value.i_val;

		return __mmcamcorder_set_capture_resolution(handle, capture_width, capture_height);
	} else {
		_mmcam_dbg_log("Capture resolution can't be set.(state=%d)", current_state);

		return FALSE;
	}
}


bool _mmcamcorder_commit_capture_break_cont_shot (MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	int current_state = _mmcamcorder_get_state( handle);
	int ivalue = value->value.i_val;

	mmf_camcorder_t        *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;
	GstCameraControl       *control = NULL;
	type_element           *VideosrcElement = NULL;

	char* videosrc_name = NULL;

	_mmcamcorder_conf_get_element(hcamcorder->conf_main,
	                              CONFIGURE_CATEGORY_MAIN_VIDEO_INPUT,
	                              "VideosrcElement",
	                              &VideosrcElement );
	_mmcamcorder_conf_get_value_element_name(VideosrcElement, &videosrc_name);

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	if (!sc)
		return TRUE;

	if( ivalue && current_state == MM_CAMCORDER_STATE_CAPTURING )
	{
		if( !strcmp( videosrc_name, "avsysvideosrc" ) || !strcmp( videosrc_name, "camerasrc" ) )
		{
			if (!GST_IS_CAMERA_CONTROL(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst))
			{
				_mmcam_dbg_err("Can't cast Video source into camera control.");
				return MM_ERROR_CAMCORDER_NOT_SUPPORTED;
			}

			control = GST_CAMERA_CONTROL( sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst );

			gst_camera_control_set_capture_command( control, GST_CAMERA_CONTROL_CAPTURE_COMMAND_STOP_MULTISHOT );

			_mmcam_dbg_warn( "Commit Break continuous shot : Set command OK. current state[%d]", current_state );
		}
		else
		{
			_mmcam_dbg_warn( "Another videosrc plugin[%s] is not supported yet.", videosrc_name );
		}
	}
	else
	{
		_mmcam_dbg_warn( "Commit Break continuous shot : No effect. value[%d],current state[%d]", ivalue, current_state );
	}

	return TRUE;
}


bool _mmcamcorder_commit_capture_count(MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	int mode = MM_CAMCORDER_MODE_VIDEO_CAPTURE;
	int current_state = MM_CAMCORDER_STATE_NONE;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);

	mmf_return_val_if_fail(hcamcorder, FALSE);

	current_state = _mmcamcorder_get_state(handle);
	mm_camcorder_get_attributes(handle, NULL, MMCAM_MODE, &mode, NULL);

	_mmcam_dbg_log("current state %d, mode %d, set count %d",
	               current_state, mode, value->value.i_val);

	if (mode != MM_CAMCORDER_MODE_AUDIO &&
	    current_state != MM_CAMCORDER_STATE_CAPTURING) {
		return TRUE;
	} else {
		_mmcam_dbg_err("Invalid mode[%d] or state[%d]", mode, current_state);
		return FALSE;
	}
}


bool _mmcamcorder_commit_capture_sound_enable(MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);

	mmf_return_val_if_fail(hcamcorder, FALSE);

	_mmcam_dbg_log("shutter sound policy: %d", hcamcorder->shutter_sound_policy);

	/* return error when disable shutter sound if policy is TRUE */
	if (!value->value.i_val &&
	    hcamcorder->shutter_sound_policy == VCONFKEY_CAMERA_SHUTTER_SOUND_POLICY_ON) {
		_mmcam_dbg_err("not permitted DISABLE SHUTTER SOUND");
		return FALSE;
	} else {
		_mmcam_dbg_log("set value [%d] success", value->value.i_val);
		return TRUE;
	}
}


bool _mmcamcorder_commit_audio_volume (MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	int current_state = MM_CAMCORDER_STATE_NONE;
	_MMCamcorderSubContext *sc = NULL;
	bool bret = FALSE;

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	if (!sc)
		return TRUE;

	current_state = _mmcamcorder_get_state( handle);

	if ((current_state == MM_CAMCORDER_STATE_RECORDING)||(current_state == MM_CAMCORDER_STATE_PAUSED))
	{
		double mslNewVal = 0;
		mslNewVal = value->value.d_val;

		if (sc->element[_MMCAMCORDER_AUDIOSRC_VOL].gst)
		{
			if(mslNewVal == 0.0)
			{
				//Because data probe of audio src do the same job, it doesn't need to set mute here. Already null raw data.
//				MMCAMCORDER_G_OBJECT_SET( sc->element[_MMCAMCORDER_AUDIOSRC_VOL].gst, "mute", TRUE);
				MMCAMCORDER_G_OBJECT_SET( sc->element[_MMCAMCORDER_AUDIOSRC_VOL].gst, "volume", 1.0);
			}
			else
			{
				MMCAMCORDER_G_OBJECT_SET( sc->element[_MMCAMCORDER_AUDIOSRC_VOL].gst, "mute", FALSE);
				MMCAMCORDER_G_OBJECT_SET( sc->element[_MMCAMCORDER_AUDIOSRC_VOL].gst, "volume", mslNewVal);
			}
		}

		_mmcam_dbg_log("Commit : volume(%f)", mslNewVal);
		bret = TRUE;
	}
	else
	{
		_mmcam_dbg_log("Commit : nothing to commit. status(%d)", current_state);
		bret = TRUE;
	}

	return bret;
}


bool _mmcamcorder_commit_camera_fps (MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	_mmcam_dbg_log("FPS(%d)", value->value.i_val);
	return TRUE;
}


bool _mmcamcorder_commit_camera_recording_motion_rate(MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	int current_state = MM_CAMCORDER_STATE_NONE;
	_MMCamcorderSubContext *sc = NULL;

	mmf_return_val_if_fail(handle, TRUE);

	current_state = _mmcamcorder_get_state(handle);

	if (current_state > MM_CAMCORDER_STATE_PREPARE) {
		_mmcam_dbg_warn("invalid state %d", current_state);
		return FALSE;
	}

	/* Verify recording motion rate */
	if (value->value.d_val > 0.0) {
		sc = MMF_CAMCORDER_SUBCONTEXT(handle);
		mmf_return_val_if_fail(sc, TRUE);

		/* set is_slow flag */
		if (value->value.d_val != _MMCAMCORDER_DEFAULT_RECORDING_MOTION_RATE) {
			sc->is_modified_rate = TRUE;
		} else {
			sc->is_modified_rate = FALSE;
		}

		_mmcam_dbg_log("Set slow motion rate %lf", value->value.d_val);
		return TRUE;
	} else {
		_mmcam_dbg_warn("Failed to set recording motion rate %lf", value->value.d_val);
		return FALSE;
	}
}


bool _mmcamcorder_commit_camera_width (MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	MMHandleType attr = 0;
	int current_state = MM_CAMCORDER_STATE_NONE;
	int width, height;

	attr = MMF_CAMCORDER_ATTRS(handle);
	mmf_return_val_if_fail(attr, FALSE);

	_mmcam_dbg_log("Width(%d)", value->value.i_val);

	current_state = _mmcamcorder_get_state(handle);

	if (current_state > MM_CAMCORDER_STATE_READY) {
		_mmcam_dbg_log("Resolution can't be changed.(state=%d)", current_state);
		return FALSE;
	} else {
		int flags = MM_ATTRS_FLAG_NONE;
		MMCamAttrsInfo info;
		mm_camcorder_get_attribute_info(handle, MMCAM_CAMERA_HEIGHT, &info);
		flags = info.flag;

		if (!(flags & MM_ATTRS_FLAG_MODIFIED)) {
			mm_camcorder_get_attributes(handle, NULL, MMCAM_CAMERA_HEIGHT, &height, NULL);
			width = value->value.i_val;
			//This means that width is changed while height isn't changed. So call _mmcamcorder_commit_camera_height forcely.
			_mmcam_dbg_log("Call _mmcamcorder_commit_camera_height");
			return __mmcamcorder_set_camera_resolution(handle, width, height);
		}
		return TRUE;
	}
}


bool _mmcamcorder_commit_camera_height (MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	int width, height;
	int current_state = MM_CAMCORDER_STATE_NONE;
	MMHandleType attr = 0;
	
	attr = MMF_CAMCORDER_ATTRS(handle);
	mmf_return_val_if_fail(attr, FALSE);

	_mmcam_dbg_log("Height(%d)", value->value.i_val);
	current_state = _mmcamcorder_get_state( handle);

	if (current_state > MM_CAMCORDER_STATE_READY) {
		_mmcam_dbg_log("Resolution can't be changed.(state=%d)", current_state);
		return FALSE;
	} else {
		height = value->value.i_val;
		mm_camcorder_get_attributes(handle, NULL, MMCAM_CAMERA_WIDTH, &width, NULL);
		return __mmcamcorder_set_camera_resolution(handle, width, height);
	}
}


bool _mmcamcorder_commit_camera_zoom (MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	_MMCamcorderSubContext *sc = NULL;
	int current_state = MM_CAMCORDER_STATE_NONE;
	GstCameraControl *control = NULL;
	int zoom_level = value->value.i_val;
	int zoom_type;

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	mmf_return_val_if_fail(sc, TRUE);

	_mmcam_dbg_log("(%d)", attr_idx);

	current_state = _mmcamcorder_get_state(handle);

	if (current_state < MM_CAMCORDER_STATE_PREPARE) {
		return TRUE;
	}

	if (attr_idx == MM_CAM_CAMERA_OPTICAL_ZOOM) {
		zoom_type = GST_CAMERA_CONTROL_OPTICAL_ZOOM;
	} else {
		zoom_type = GST_CAMERA_CONTROL_DIGITAL_ZOOM;
	}

	if (sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst) {
		int ret = FALSE;

		if (!GST_IS_CAMERA_CONTROL(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst)) {
			_mmcam_dbg_log("Can't cast Video source into camera control.");
			return TRUE; 
		}

		control = GST_CAMERA_CONTROL (sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst);

		__ta__("                gst_camera_control_set_zoom",
		ret = gst_camera_control_set_zoom(control, zoom_type, zoom_level);
		);

		if (ret) {
			_mmcam_dbg_log("Succeed in operating Zoom[%d].", zoom_level);
			return TRUE;
		} else {
			_mmcam_dbg_warn("Failed to operate Zoom. Type[%d],Level[%d]", zoom_type, zoom_level);
		}
	} else {
		_mmcam_dbg_log("pointer of video src is null");
	}

	return FALSE;
}


bool _mmcamcorder_commit_camera_focus_mode (MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	MMHandleType attr = 0;
	int current_state = MM_CAMCORDER_STATE_NONE;
	_MMCamcorderSubContext *sc = NULL;
	GstCameraControl *control = NULL;
	int mslVal;
	int mode, cur_focus_mode, cur_focus_range;

	attr = MMF_CAMCORDER_ATTRS(handle);
	mmf_return_val_if_fail(attr, FALSE);

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	if (!sc)
		return TRUE;

	_mmcam_dbg_log("Focus mode(%d)", value->value.i_val);

	/* check whether set or not */
	if (!_mmcamcorder_check_supported_attribute(handle, attr_idx)) {
		_mmcam_dbg_log("skip set value %d", value->value.i_val);
		return TRUE;
	}

	current_state = _mmcamcorder_get_state(handle);
	if (current_state < MM_CAMCORDER_STATE_NULL) {
		_mmcam_dbg_log("Focus mode will be changed later.(state=%d)", current_state);
		return TRUE;
	}

	if( sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst )
	{
		int flags = MM_ATTRS_FLAG_NONE;
		MMCamAttrsInfo info;

		if (!GST_IS_CAMERA_CONTROL(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst))
		{
			_mmcam_dbg_log("Can't cast Video source into camera control.");
			return TRUE;
		}

		control = GST_CAMERA_CONTROL (sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst);

		mslVal = value->value.i_val;
		mode = _mmcamcorder_convert_msl_to_sensor( handle, attr_idx, mslVal );

		mm_camcorder_get_attribute_info(handle, MMCAM_CAMERA_AF_SCAN_RANGE, &info);
		flags = info.flag;

		if (!(flags & MM_ATTRS_FLAG_MODIFIED))
		{
			if( gst_camera_control_get_focus( control, &cur_focus_mode, &cur_focus_range ) )
			{
				if( mode != cur_focus_mode )
				{
					MMTA_ACUM_ITEM_BEGIN("                gst_camera_control_set_focus", 0);
					if( gst_camera_control_set_focus( control, mode, cur_focus_range ) )
					{
						MMTA_ACUM_ITEM_END("                gst_camera_control_set_focus", 0);
						_mmcam_dbg_log( "Succeed in setting AF mode[%d]", mslVal );
						return TRUE;
					}
					else
					{
						_mmcam_dbg_warn( "Failed to set AF mode[%d]", mslVal );
					}
					MMTA_ACUM_ITEM_END("                gst_camera_control_set_focus", 0);
				}
				else
				{
					_mmcam_dbg_log( "No need to set AF mode. Current[%d]", mslVal );
					return TRUE;
				}
			}
			else
			{
				_mmcam_dbg_warn( "Failed to get AF mode, so do not set new AF mode[%d]", mslVal );
			}
		}
	}
	else
	{
		_mmcam_dbg_log("pointer of video src is null");
	}
	return TRUE;
}


bool _mmcamcorder_commit_camera_af_scan_range (MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	_MMCamcorderSubContext *sc = NULL;
	GstCameraControl *control = NULL;
	int current_state = MM_CAMCORDER_STATE_NONE;
	int mslVal, newVal;
	int cur_focus_mode = 0;
	int cur_focus_range = 0;
	int msl_mode = MM_CAMCORDER_FOCUS_MODE_NONE;
	int converted_mode = 0;

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	if (!sc)
		return TRUE;

	_mmcam_dbg_log("(%d)", attr_idx);

	/* check whether set or not */
	if (!_mmcamcorder_check_supported_attribute(handle, attr_idx)) {
		_mmcam_dbg_log("skip set value %d", value->value.i_val);
		return TRUE;
	}

	mslVal = value->value.i_val;
	newVal = _mmcamcorder_convert_msl_to_sensor(handle, attr_idx, mslVal);

	current_state = _mmcamcorder_get_state(handle);
	if (current_state < MM_CAMCORDER_STATE_PREPARE) {
		_mmcam_dbg_log("It doesn't need to change dynamically.(state=%d)", current_state);
		return TRUE;
	}
	
	if( sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst )
	{
		if (!GST_IS_CAMERA_CONTROL(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst))
		{
			_mmcam_dbg_log("Can't cast Video source into camera control.");
			return TRUE;
		}

		control = GST_CAMERA_CONTROL (sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst);

		mm_camcorder_get_attributes(handle, NULL, MMCAM_CAMERA_FOCUS_MODE, &msl_mode, NULL);
		converted_mode = _mmcamcorder_convert_msl_to_sensor( handle, MM_CAM_CAMERA_FOCUS_MODE, msl_mode );

		if( gst_camera_control_get_focus( control, &cur_focus_mode, &cur_focus_range ) )
		{

			if (( newVal != cur_focus_range ) || ( converted_mode != cur_focus_mode ))
			{
				MMTA_ACUM_ITEM_BEGIN("                gst_camera_control_set_focus", 0);
				if( gst_camera_control_set_focus( control, converted_mode, newVal ) )
				{
					MMTA_ACUM_ITEM_END("                gst_camera_control_set_focus", 0);
					//_mmcam_dbg_log( "Succeed in setting AF mode[%d]", mslVal );
					return TRUE;
				}
				else
				{
					MMTA_ACUM_ITEM_END("                gst_camera_control_set_focus", 0);
					_mmcam_dbg_warn( "Failed to set AF mode[%d]", mslVal );
				}
			}
			else
			{
				//_mmcam_dbg_log( "No need to set AF mode. Current[%d]", mslVal );
				return TRUE;
			}
		}
		else
		{
			_mmcam_dbg_warn( "Failed to get AF mode, so do not set new AF mode[%d]", mslVal );
		}
	}
	else
	{
		_mmcam_dbg_log("pointer of video src is null");
	}
	return FALSE;
}

bool _mmcamcorder_commit_camera_af_touch_area (MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	_MMCamcorderSubContext *sc = NULL;
	GstCameraControl *control = NULL;
	GstCameraControlRectType set_area = { 0, 0, 0, 0 }, get_area = { 0, 0, 0, 0 };

	int current_state = MM_CAMCORDER_STATE_NONE;
	int ret = FALSE;
	int focus_mode = MM_CAMCORDER_FOCUS_MODE_NONE;
	
	gboolean do_set = FALSE;
	
	MMCamAttrsInfo info_y, info_w, info_h;

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	if (!sc)
		return TRUE;

	_mmcam_dbg_log("(%d)", attr_idx);

	current_state = _mmcamcorder_get_state( handle);

	if( current_state < MM_CAMCORDER_STATE_PREPARE )
	{
		_mmcam_dbg_log("It doesn't need to change dynamically.(state=%d)", current_state);
		return TRUE;
	}
	
	ret = mm_camcorder_get_attributes(handle, NULL,
				MMCAM_CAMERA_FOCUS_MODE, &focus_mode,
				NULL);
	if( ret != MM_ERROR_NONE )
	{
		_mmcam_dbg_warn( "Failed to get FOCUS MODE.[%x]", ret );
		return FALSE;
	}
	
	if ((focus_mode != MM_CAMCORDER_FOCUS_MODE_TOUCH_AUTO ) && (focus_mode != MM_CAMCORDER_FOCUS_MODE_CONTINUOUS))
	{
		_mmcam_dbg_warn( "Focus mode is NOT TOUCH AUTO or CONTINUOUS(current[%d]). return FALSE", focus_mode );
		return FALSE;
	}

	if( sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst )
	{
		if (!GST_IS_CAMERA_CONTROL(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst))
		{
			_mmcam_dbg_log("Can't cast Video source into camera control.");
			return TRUE; 
		}

		switch( attr_idx )
		{
			case MM_CAM_CAMERA_AF_TOUCH_X:
				mm_camcorder_get_attribute_info(handle, MMCAM_CAMERA_AF_TOUCH_Y, &info_y);
				mm_camcorder_get_attribute_info(handle, MMCAM_CAMERA_AF_TOUCH_WIDTH, &info_w);
				mm_camcorder_get_attribute_info(handle, MMCAM_CAMERA_AF_TOUCH_HEIGHT, &info_h);
				if( !( (info_y.flag|info_w.flag|info_h.flag) & MM_ATTRS_FLAG_MODIFIED) )
				{
					set_area.x = value->value.i_val;
					mm_camcorder_get_attributes(handle, NULL,
							MMCAM_CAMERA_AF_TOUCH_Y, &set_area.y,
							MMCAM_CAMERA_AF_TOUCH_WIDTH, &set_area.width,
							MMCAM_CAMERA_AF_TOUCH_HEIGHT, &set_area.height,
							NULL);
					do_set = TRUE;
				}
				else
				{
					_mmcam_dbg_log( "Just store AF area[x:%d]", value->value.i_val );
					return TRUE;
				}
				break;
			case MM_CAM_CAMERA_AF_TOUCH_Y:
				mm_camcorder_get_attribute_info(handle, MMCAM_CAMERA_AF_TOUCH_WIDTH, &info_w);
				mm_camcorder_get_attribute_info(handle, MMCAM_CAMERA_AF_TOUCH_HEIGHT, &info_h);
				if( !( (info_w.flag|info_h.flag) & MM_ATTRS_FLAG_MODIFIED) )
				{
					set_area.y = value->value.i_val;
					mm_camcorder_get_attributes(handle, NULL,
							MMCAM_CAMERA_AF_TOUCH_X, &set_area.x,
							MMCAM_CAMERA_AF_TOUCH_WIDTH, &set_area.width,
							MMCAM_CAMERA_AF_TOUCH_HEIGHT, &set_area.height,
							NULL);
					do_set = TRUE;
				}
				else
				{
					_mmcam_dbg_log( "Just store AF area[y:%d]", value->value.i_val );
					return TRUE;
				}
				break;
			case MM_CAM_CAMERA_AF_TOUCH_WIDTH:
				mm_camcorder_get_attribute_info(handle, MMCAM_CAMERA_AF_TOUCH_HEIGHT, &info_h);
				if( !( info_h.flag & MM_ATTRS_FLAG_MODIFIED) )
				{
					set_area.width = value->value.i_val;
					mm_camcorder_get_attributes(handle, NULL,
							MMCAM_CAMERA_AF_TOUCH_X, &set_area.x,
							MMCAM_CAMERA_AF_TOUCH_Y, &set_area.y,
							MMCAM_CAMERA_AF_TOUCH_HEIGHT, &set_area.height,
							NULL);
					do_set = TRUE;
				}
				else
				{
					_mmcam_dbg_log( "Just store AF area[width:%d]", value->value.i_val );
					return TRUE;
				}
				break;
			case MM_CAM_CAMERA_AF_TOUCH_HEIGHT:
				set_area.height = value->value.i_val;
				mm_camcorder_get_attributes(handle, NULL,
						MMCAM_CAMERA_AF_TOUCH_X, &set_area.x,
						MMCAM_CAMERA_AF_TOUCH_Y, &set_area.y,
						MMCAM_CAMERA_AF_TOUCH_WIDTH, &set_area.width,
						NULL);
				do_set = TRUE;
				break;
			default:
				break;
		}
		
		if( do_set )
		{
			control = GST_CAMERA_CONTROL (sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst);
			
			__ta__( "                gst_camera_control_get_focus_area",
			ret = gst_camera_control_get_auto_focus_area( control, &get_area );
			);
			if( !ret )
			{
				_mmcam_dbg_warn( "Failed to get AF area" );
				return FALSE;
			}
			
			if( get_area.x == set_area.x && get_area.y == set_area.y )
			// width and height are not supported now.
			// get_area.width == set_area.width && get_area.height == set_area.height
			{
				_mmcam_dbg_log( "No need to set AF area[x,y:%d,%d]", get_area.x, get_area.y );
				return TRUE;
			}

			__ta__( "                gst_camera_control_set_focus_area",
			ret = gst_camera_control_set_auto_focus_area( control, set_area );
			);
			if( ret )
			{
				_mmcam_dbg_log( "Succeed to set AF area[%d,%d,%dx%d]", set_area.x, set_area.y, set_area.width, set_area.height );
				return TRUE;
			}
			else
			{
				_mmcam_dbg_warn( "Failed to set AF area[%d,%d,%dx%d]", set_area.x, set_area.y, set_area.width, set_area.height );
			}
		}
	}
	else
	{
		_mmcam_dbg_log("pointer of video src is null");
	}

	return FALSE;
}


bool _mmcamcorder_commit_camera_capture_mode (MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	GstCameraControl *control = NULL;
	int ivalue = value->value.i_val;
	int mslVal1 = 0, mslVal2 = 0;
	int newVal1 = 0, newVal2 = 0;
	int exposure_type = 0;
	int current_state = MM_CAMCORDER_STATE_NONE;
	_MMCamcorderSubContext *sc = NULL;

	int scene_mode = MM_CAMCORDER_SCENE_MODE_NORMAL;
	gboolean check_scene_mode = FALSE;

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	if (!sc)
		return TRUE;

	/* check whether set or not */
	if (!_mmcamcorder_check_supported_attribute(handle, attr_idx)) {
		_mmcam_dbg_log("skip set value %d", value->value.i_val);
		return TRUE;
	}

	current_state = _mmcamcorder_get_state( handle);
	if (current_state < MM_CAMCORDER_STATE_READY) {
		return TRUE;
	}

	if (attr_idx == MM_CAM_CAMERA_F_NUMBER) {
		exposure_type = GST_CAMERA_CONTROL_F_NUMBER;
		mslVal1 = newVal1 = MM_CAMCORDER_GET_NUMERATOR( ivalue );
		mslVal2 = newVal2 = MM_CAMCORDER_GET_DENOMINATOR( ivalue );
	} else if (attr_idx == MM_CAM_CAMERA_SHUTTER_SPEED) {
		exposure_type = GST_CAMERA_CONTROL_SHUTTER_SPEED;
		mslVal1 = newVal1 = MM_CAMCORDER_GET_NUMERATOR( ivalue );
		mslVal2 = newVal2 = MM_CAMCORDER_GET_DENOMINATOR( ivalue );
	} else if (attr_idx == MM_CAM_CAMERA_ISO) {
		exposure_type = GST_CAMERA_CONTROL_ISO;
		mslVal1 = ivalue;
		newVal1 = _mmcamcorder_convert_msl_to_sensor(handle, attr_idx, mslVal1);
		check_scene_mode = TRUE;
	} else if (attr_idx == MM_CAM_CAMERA_EXPOSURE_MODE) {
		exposure_type = GST_CAMERA_CONTROL_EXPOSURE_MODE;
		mslVal1 = ivalue;
		newVal1 =  _mmcamcorder_convert_msl_to_sensor(handle, attr_idx, mslVal1);
		check_scene_mode = TRUE;
	} else if (attr_idx == MM_CAM_CAMERA_EXPOSURE_VALUE) {
		exposure_type = GST_CAMERA_CONTROL_EXPOSURE_VALUE;
		mslVal1 = newVal1 = MM_CAMCORDER_GET_NUMERATOR( ivalue );
		mslVal2 = newVal2 = MM_CAMCORDER_GET_DENOMINATOR( ivalue );
	}

	if (check_scene_mode) {
		mm_camcorder_get_attributes(handle, NULL, MMCAM_FILTER_SCENE_MODE, &scene_mode, NULL);
		if (scene_mode != MM_CAMCORDER_SCENE_MODE_NORMAL) {
			_mmcam_dbg_warn("can not set [%d] when scene mode is NOT normal.", attr_idx);
			return FALSE;
		}
	}

	if (sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst) {
		int ret = 0;

		if (!GST_IS_CAMERA_CONTROL(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst)) {
			_mmcam_dbg_log("Can't cast Video source into camera control.");
			return TRUE;
		}

		control = GST_CAMERA_CONTROL (sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst);

		__ta__("                gst_camera_control_set_exposure",
		ret = gst_camera_control_set_exposure(control, exposure_type, newVal1, newVal2);
		);
		if (ret) {
			_mmcam_dbg_log("Succeed in setting exposure. Type[%d],value1[%d],value2[%d]", exposure_type, mslVal1, mslVal2 );
			return TRUE;
		} else {
			_mmcam_dbg_warn("Failed to set exposure. Type[%d],value1[%d],value2[%d]", exposure_type, mslVal1, mslVal2 );
		}
	} else {
		_mmcam_dbg_log("pointer of video src is null");
	}

	return FALSE;
}


bool _mmcamcorder_commit_camera_wdr (MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	GstCameraControl *control = NULL;
	int mslVal = 0;
	int newVal = 0;
	int cur_value = 0;
	_MMCamcorderSubContext *sc = NULL;
	int current_state = MM_CAMCORDER_STATE_NONE;

	mmf_return_val_if_fail(handle && value, FALSE);

	/* check whether set or not */
	if (!_mmcamcorder_check_supported_attribute(handle, attr_idx)) {
		_mmcam_dbg_log("skip set value %d", value->value.i_val);
		return TRUE;
	}

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	mmf_return_val_if_fail(sc, TRUE);

	/* check current state */
	current_state = _mmcamcorder_get_state( handle);
	if (current_state < MM_CAMCORDER_STATE_PREPARE) {
		_mmcam_dbg_log("It doesn't need to change dynamically.(state=%d)", current_state);
		return TRUE;
	}

	if (current_state == MM_CAMCORDER_STATE_CAPTURING) {
		_mmcam_dbg_warn("Can not set WDR while CAPTURING");
		return FALSE;
	}

	mslVal = value->value.i_val;
	newVal = _mmcamcorder_convert_msl_to_sensor(handle, MM_CAM_CAMERA_WDR, mslVal);

	if (sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst) {
		if (!GST_IS_CAMERA_CONTROL(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst)) {
			_mmcam_dbg_log("Can't cast Video source into camera control.");
			return TRUE;
		}

		control = GST_CAMERA_CONTROL(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst);
		if (gst_camera_control_get_wdr(control, &cur_value)) {
			if (newVal != cur_value) {
				if (gst_camera_control_set_wdr(control, newVal)) {
					_mmcam_dbg_log( "Success - set wdr[%d]", mslVal );
					return TRUE;
				} else {
					_mmcam_dbg_warn("Failed to set WDR. NewVal[%d],CurVal[%d]", newVal, cur_value);
				}
			} else {
				_mmcam_dbg_log( "No need to set new WDR. Current[%d]", mslVal );
				return TRUE;
			}
		} else {
			_mmcam_dbg_warn( "Failed to get WDR." );
		}
	} else {
		_mmcam_dbg_log("pointer of video src is null");
	}

	return FALSE;
}


bool _mmcamcorder_commit_camera_anti_handshake(MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	int current_state = MM_CAMCORDER_STATE_NONE;

	/* check whether set or not */
	if (!_mmcamcorder_check_supported_attribute(handle, attr_idx)) {
		_mmcam_dbg_log("skip set value %d", value->value.i_val);
		return TRUE;
	}

	current_state = _mmcamcorder_get_state(handle);
	if (current_state < MM_CAMCORDER_STATE_READY) {
		_mmcam_dbg_log("It doesn't need to change dynamically.(state=%d)", current_state);
		return TRUE;
	} else if (current_state > MM_CAMCORDER_STATE_PREPARE) {
		_mmcam_dbg_err("Invaild state (state %d)", current_state);
		return FALSE;
	}

	return _mmcamcorder_set_videosrc_anti_shake(handle, value->value.i_val);
}


bool _mmcamcorder_commit_camera_video_stabilization(MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	int current_state = MM_CAMCORDER_STATE_NONE;

	current_state = _mmcamcorder_get_state(handle);
	if (current_state < MM_CAMCORDER_STATE_READY) {
		_mmcam_dbg_log("It doesn't need to change dynamically.(state=%d)", current_state);
		return TRUE;
	} else if (current_state > MM_CAMCORDER_STATE_PREPARE) {
		_mmcam_dbg_err("Invaild state (state %d)", current_state);
		return FALSE;
	}

	return _mmcamcorder_set_videosrc_stabilization(handle, value->value.i_val);
}


bool _mmcamcorder_commit_camera_hold_af_after_capturing (MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	_MMCamcorderSubContext *sc = NULL;
	int current_state = MM_CAMCORDER_STATE_NONE;

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	if (!sc)
		return TRUE;

	current_state = _mmcamcorder_get_state(handle);

	if( current_state < MM_CAMCORDER_STATE_READY )
	{
		_mmcam_dbg_log("It doesn't need to change dynamically.(state=%d)", current_state);
		return TRUE;
	}
	
	if (sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst)
	{
		_mmcam_dbg_log("Commit : value of Hold af after capturing is %d", value->value.i_val);
		MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst, "hold-af-after-capturing", value->value.i_val);
	}
	else
		_mmcam_dbg_warn("Commit : Hold af after capturing cannot be set");

	return TRUE;
}


bool _mmcamcorder_commit_camera_rotate(MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	int current_state = MM_CAMCORDER_STATE_NONE;

	_mmcam_dbg_log("rotate(%d)", value->value.i_val);

	current_state = _mmcamcorder_get_state(handle);

	if (current_state > MM_CAMCORDER_STATE_READY) {
		_mmcam_dbg_err("camera rotation setting failed.(state=%d)", current_state);
		return FALSE;
	} else {
		return _mmcamcorder_set_videosrc_rotation(handle, value->value.i_val);
	}
}


bool _mmcamcorder_commit_image_encoder_quality(MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	int current_state = MM_CAMCORDER_STATE_NONE;

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

	_mmcam_dbg_log("Image encoder quality(%d)", value->value.i_val);

	if (current_state == MM_CAMCORDER_STATE_PREPARE) {
		MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst, "capture-jpg-quality", value->value.i_val);
		return TRUE;
	} else {
		_mmcam_dbg_err("invalid state %d", current_state);
		return FALSE;
	}
}


bool _mmcamcorder_commit_target_filename(MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	_MMCamcorderSubContext *sc = NULL;
	char * filename = NULL;
	int size = 0;

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	if (!sc)
		return TRUE;

	/* get string */
	filename = (char *)mmf_value_get_string(value, &size);

	if (sc->element && sc->element[_MMCAMCORDER_ENCSINK_SINK].gst) {
		MMCAMCORDER_G_OBJECT_SET( sc->element[_MMCAMCORDER_ENCSINK_SINK].gst, "location", filename);
		_mmcam_dbg_log("new file location set.(%s)", filename);
	} else {
		_mmcam_dbg_log("element is not created yet. [%s] will be set later...", filename);
	}

	return TRUE;
}




bool _mmcamcorder_commit_filter (MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	GstColorBalance *balance = NULL;
	GstColorBalanceChannel *Colorchannel = NULL;
	const GList *controls = NULL;
	const GList *item = NULL;
	int newVal = 0;
	int mslNewVal = 0;
	int current_state = MM_CAMCORDER_STATE_NONE;
	gchar * control_label = NULL;
	_MMCamcorderSubContext *sc = NULL;

	int scene_mode = MM_CAMCORDER_SCENE_MODE_NORMAL;
	gboolean check_scene_mode = FALSE;

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	if (!sc)
		return TRUE;

	/* check whether set or not */
	if (!_mmcamcorder_check_supported_attribute(handle, attr_idx)) {
		_mmcam_dbg_log("skip set value %d", value->value.i_val);
		return TRUE;
	}

	current_state = _mmcamcorder_get_state(handle);
	if (current_state < MM_CAMCORDER_STATE_PREPARE) {
		_mmcam_dbg_log("It doesn't need to change dynamically.(state=%d)", current_state);
		return TRUE;
	}

	if (value->type != MM_ATTRS_TYPE_INT) {
		_mmcam_dbg_warn("Mismatched value type (%d)", value->type);
		return FALSE;
	} else {
		mslNewVal = value->value.i_val;
	}

	switch (attr_idx) {
	case MM_CAM_FILTER_BRIGHTNESS:
		control_label = "brightness";
		check_scene_mode = TRUE;
		break;

	case MM_CAM_FILTER_CONTRAST:
		control_label = "contrast";
		break;

	case MM_CAM_FILTER_WB:
		control_label = "white balance";
		check_scene_mode = TRUE;
		break;

	case MM_CAM_FILTER_COLOR_TONE:
		control_label = "color tone";
		break;

	case MM_CAM_FILTER_SATURATION:
		control_label = "saturation";
		check_scene_mode = TRUE;
		break;

	case MM_CAM_FILTER_HUE:
		control_label = "hue";
		break;

	case MM_CAM_FILTER_SHARPNESS:
		control_label = "sharpness";
		check_scene_mode = TRUE;
		break;
	}

	if (check_scene_mode) {
		mm_camcorder_get_attributes(handle, NULL, MMCAM_FILTER_SCENE_MODE, &scene_mode, NULL);
		if (scene_mode != MM_CAMCORDER_SCENE_MODE_NORMAL) {
			_mmcam_dbg_warn("can not set %s when scene mode is NOT normal.", control_label);
			return FALSE;
		}
	}

	newVal = _mmcamcorder_convert_msl_to_sensor(handle, attr_idx, mslNewVal);
	if (newVal == _MMCAMCORDER_SENSOR_ENUM_NONE)
		return FALSE;
			
	_mmcam_dbg_log("label(%s): MSL(%d)->Sensor(%d)", control_label, mslNewVal, newVal);
	
	if (!GST_IS_COLOR_BALANCE(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst)) {
		_mmcam_dbg_log("Can't cast Video source into color balance.");
		return TRUE;
	}
	
	balance = GST_COLOR_BALANCE (sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst);

	controls = gst_color_balance_list_channels (balance);
	if (controls == NULL) {
		_mmcam_dbg_log("There is no list of colorbalance controls");
		return FALSE;
	}

	for (item = controls ; item != NULL ; item = item->next) {
		if (item) {
			if (item->data) {
				Colorchannel = item->data;
				/*_mmcam_dbg_log("Getting name of CID=(%s), input CID=(%s)", Colorchannel->label, control_label);*/

				if (!strcmp(Colorchannel->label, control_label)) {
					break;
				} else {
					Colorchannel = NULL;
				}
			}
		}
	}

	if (Colorchannel== NULL) {
		_mmcam_dbg_log("There is no data in the colorbalance controls(%d)", attr_idx);
		return FALSE;
	}

	__ta__("                gst_color_balance_set_value",
	gst_color_balance_set_value (balance, Colorchannel, newVal);
	);

	_mmcam_dbg_log( "Set complete - %s[msl:%d,real:%d]", Colorchannel->label, mslNewVal, newVal);

	return TRUE;
}


bool _mmcamcorder_commit_filter_scene_mode (MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	GstCameraControl *control = NULL;
	int mslVal = value->value.i_val;
	int newVal = _mmcamcorder_convert_msl_to_sensor( handle, MM_CAM_FILTER_SCENE_MODE, mslVal );
	_MMCamcorderSubContext *sc = NULL;
	int current_state = MM_CAMCORDER_STATE_NONE;

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	if (!sc)
		return TRUE;

	/* check whether set or not */
	if (!_mmcamcorder_check_supported_attribute(handle, attr_idx)) {
		_mmcam_dbg_log("skip set value %d", value->value.i_val);
		return TRUE;
	}

	current_state = _mmcamcorder_get_state(handle);
	if (current_state < MM_CAMCORDER_STATE_PREPARE) {
		_mmcam_dbg_log("It doesn't need to change dynamically.(state=%d)", current_state);
		return TRUE;
	}
	
	if (sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst) {
		int ret = 0;

		if (!GST_IS_CAMERA_CONTROL(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst)) {
			_mmcam_dbg_log("Can't cast Video source into camera control.");
			return TRUE;
		}

		control = GST_CAMERA_CONTROL (sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst);
		__ta__("                gst_camera_control_set_exposure:GST_CAMERA_CONTROL_PROGRAM_MODE",
		ret = gst_camera_control_set_exposure(control, GST_CAMERA_CONTROL_PROGRAM_MODE, newVal, 0);
		);
		if (ret) {
			_mmcam_dbg_log("Succeed in setting program mode[%d].", mslVal);

			if (mslVal == MM_CAMCORDER_SCENE_MODE_NORMAL) {
				int i = 0;
				int attr_idxs[] = {
					MM_CAM_CAMERA_ISO
					, MM_CAM_FILTER_BRIGHTNESS
					, MM_CAM_FILTER_WB
					, MM_CAM_FILTER_SATURATION
					, MM_CAM_FILTER_SHARPNESS
					, MM_CAM_FILTER_COLOR_TONE
					, MM_CAM_CAMERA_EXPOSURE_MODE
				};
				mmf_attrs_t *attr = (mmf_attrs_t *)MMF_CAMCORDER_ATTRS(handle);

				for (i = 0 ; i < ARRAY_SIZE(attr_idxs) ; i++) {
					if (__mmcamcorder_attrs_is_supported((MMHandleType)attr, attr_idxs[i])) {
						mmf_attribute_set_modified(&(attr->items[attr_idxs[i]]));
					}
				}
			}

			return TRUE;
		} else {
			_mmcam_dbg_log( "Failed to set program mode[%d].", mslVal );
		}
	} else {
		_mmcam_dbg_warn("pointer of video src is null");
	}

	return FALSE;
}


bool _mmcamcorder_commit_filter_flip (MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	_mmcam_dbg_warn("Filter Flip(%d)", value->value.i_val);
	return TRUE;
}


bool _mmcamcorder_commit_camera_face_zoom(MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	int ret = 0;
	int current_state = MM_CAMCORDER_STATE_NONE;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	GstCameraControl *control = NULL;
	_MMCamcorderSubContext *sc = NULL;

	mmf_return_val_if_fail(hcamcorder, FALSE);

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	mmf_return_val_if_fail(sc, TRUE);

	/* these are only available after camera preview is started */
	current_state = _mmcamcorder_get_state(handle);
	if (current_state >= MM_CAMCORDER_STATE_PREPARE &&
	    hcamcorder->type != MM_CAMCORDER_MODE_AUDIO) {
		int x = 0;
		int y = 0;
		int zoom_level = 0;
		int preview_width = 0;
		int preview_height = 0;

		switch (attr_idx) {
		case MM_CAM_CAMERA_FACE_ZOOM_X:
			/* check x coordinate of face zoom */
			mm_camcorder_get_attributes(handle, NULL,
			                            MMCAM_CAMERA_WIDTH, &preview_width,
			                            NULL);
			/* x coordinate should be smaller than width of preview */
			if (value->value.i_val < preview_width) {
				_mmcam_dbg_log("set face zoom x %d done", value->value.i_val);
				ret = TRUE;
			} else {
				_mmcam_dbg_err("invalid face zoom x %d", value->value.i_val);
				ret = FALSE;
			}
			break;
		case MM_CAM_CAMERA_FACE_ZOOM_Y:
			/* check y coordinate of face zoom */
			mm_camcorder_get_attributes(handle, NULL,
			                            MMCAM_CAMERA_WIDTH, &preview_height,
			                            NULL);
			/* y coordinate should be smaller than height of preview */
			if (value->value.i_val < preview_height) {
				_mmcam_dbg_log("set face zoom y %d done", value->value.i_val);
				ret = TRUE;
			} else {
				_mmcam_dbg_err("invalid face zoom y %d", value->value.i_val);
				ret = FALSE;
			}
			break;
		case MM_CAM_CAMERA_FACE_ZOOM_MODE:
			if (value->value.i_val == MM_CAMCORDER_FACE_ZOOM_MODE_ON) {
				int face_detect_mode = MM_CAMCORDER_DETECT_MODE_OFF;

				/* start face zoom */
				/* get x,y coordinate and zoom level */
				mm_camcorder_get_attributes(handle, NULL,
				                            MMCAM_CAMERA_FACE_ZOOM_X, &x,
				                            MMCAM_CAMERA_FACE_ZOOM_Y, &y,
				                            MMCAM_CAMERA_FACE_ZOOM_LEVEL, &zoom_level,
				                            MMCAM_DETECT_MODE, &face_detect_mode,
				                            NULL);

				if (face_detect_mode == MM_CAMCORDER_DETECT_MODE_ON) {
					control = GST_CAMERA_CONTROL(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst );
					__ta__("                gst_camera_control_start_face_zoom",
					ret = gst_camera_control_start_face_zoom(control, x, y, zoom_level);
					);
				} else {
					_mmcam_dbg_err("face detect is OFF... could not start face zoom");
					ret = FALSE;
				}
			} else if (value->value.i_val == MM_CAMCORDER_FACE_ZOOM_MODE_OFF) {
				/* stop face zoom */
				control = GST_CAMERA_CONTROL(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst );
				__ta__("                gst_camera_control_stop_face_zoom",
				ret = gst_camera_control_stop_face_zoom(control);
				);
			} else {
				/* should not be reached here */
				_mmcam_dbg_err("unknown command [%d]", value->value.i_val);
				ret = FALSE;
			}

			if (!ret) {
				_mmcam_dbg_err("face zoom[%d] failed", value->value.i_val);
				ret = FALSE;
			} else {
				_mmcam_dbg_log("");
				ret = TRUE;
			}
			break;
		default:
			_mmcam_dbg_warn("should not be reached here. attr_idx %d", attr_idx);
			break;
		}
	} else {
		_mmcam_dbg_err("invalid state[%d] or mode[%d]", current_state, hcamcorder->type);
		ret = FALSE;
	}

	return ret;
}


bool _mmcamcorder_commit_audio_input_route (MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	_mmcam_dbg_log("Commit : Do nothing. this attr will be removed soon.");

	return TRUE;
}


bool _mmcamcorder_commit_audio_disable(MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	int current_state = MM_CAMCORDER_STATE_NONE;

	current_state = _mmcamcorder_get_state(handle);
	if (current_state > MM_CAMCORDER_STATE_PREPARE) {
		_mmcam_dbg_warn("Can NOT Disable AUDIO. invalid state %d", current_state);
		return FALSE;
	} else {
		_mmcam_dbg_log("Disable AUDIO when Recording");
		return TRUE;
	}
}


bool _mmcamcorder_commit_display_handle(MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	int current_state = MM_CAMCORDER_STATE_NONE;
	char *videosink_name = NULL;
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
		_mmcam_dbg_log("Commit : videosinkname[%s]", videosink_name);

		if (!strcmp(videosink_name, "xvimagesink") || !strcmp(videosink_name, "ximagesink")) {
			_mmcam_dbg_log("Commit : Set XID[%x]", *(int*)(p_handle));
			gst_x_overlay_set_xwindow_id(GST_X_OVERLAY(sc->element[_MMCAMCORDER_VIDEOSINK_SINK].gst), *(int*)(p_handle));
		} else if (!strcmp(videosink_name, "evasimagesink") ||
		           !strcmp(videosink_name, "evaspixmapsink")) {
			_mmcam_dbg_log("Commit : Set evas object [%p]", p_handle);
			MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSINK_SINK].gst, "evas-object", p_handle);
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


bool _mmcamcorder_commit_display_mode(MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	int current_state = MM_CAMCORDER_STATE_NONE;
	char *videosink_name = NULL;

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
	_mmcam_dbg_log("Commit : videosinkname[%s]", videosink_name);

	if (!strcmp(videosink_name, "xvimagesink")) {
		_mmcam_dbg_log("Commit : display mode [%d]", value->value.i_val);
		MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSINK_SINK].gst, "display-mode", value->value.i_val);
		return TRUE;
	} else {
		_mmcam_dbg_warn("Commit : This element [%s] does not support display mode", videosink_name);
		return FALSE;
	}
}


bool _mmcamcorder_commit_display_rotation(MMHandleType handle, int attr_idx, const mmf_value_t *value)
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
		_mmcam_dbg_log("NOT initialized. this will be applied later");
		return TRUE;
	}

	return _mmcamcorder_set_display_rotation(handle, value->value.i_val);
}


bool _mmcamcorder_commit_display_flip(MMHandleType handle, int attr_idx, const mmf_value_t *value)
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
		_mmcam_dbg_log("NOT initialized. this will be applied later");
		return TRUE;
	}

	return _mmcamcorder_set_display_flip(handle, value->value.i_val);
}


bool _mmcamcorder_commit_display_visible(MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	int current_state = MM_CAMCORDER_STATE_NONE;
	char *videosink_name = NULL;

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
	if (!strcmp(videosink_name, "xvimagesink") ||
	    !strcmp(videosink_name, "evaspixmapsink")) {
		MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSINK_SINK].gst, "visible", value->value.i_val);
		_mmcam_dbg_log("Set visible [%d] done.", value->value.i_val);
		return TRUE;
	} else {
		_mmcam_dbg_warn("videosink[%s] does not support VISIBLE.", videosink_name);
		return FALSE;
	}
}


bool _mmcamcorder_commit_display_geometry_method (MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	int method = 0;
	int current_state = MM_CAMCORDER_STATE_NONE;
	char *videosink_name = NULL;

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
	if (!strcmp(videosink_name, "xvimagesink") ||
	    !strcmp(videosink_name, "evaspixmapsink")) {
		method = value->value.i_val;
		MMCAMCORDER_G_OBJECT_SET( sc->element[_MMCAMCORDER_VIDEOSINK_SINK].gst, "display-geometry-method", method);
		return TRUE;
	} else {
		_mmcam_dbg_warn("videosink[%s] does not support geometry method.", videosink_name);
		return FALSE;
	}
}


bool _mmcamcorder_commit_display_rect(MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	int current_state = MM_CAMCORDER_STATE_NONE;
	int method = 0;
	char *videosink_name = NULL;

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
		case MM_CAM_DISPLAY_RECT_X:
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
		case MM_CAM_DISPLAY_RECT_Y:
			mm_camcorder_get_attribute_info(handle, MMCAM_DISPLAY_RECT_WIDTH, &info);
			flags |= info.flag;
			memset(&info, 0x00, sizeof(info));
			mm_camcorder_get_attribute_info(handle, MMCAM_DISPLAY_RECT_HEIGHT, &info);
			flags |= info.flag;

			rect_y = value->value.i_val;
			break;
		case MM_CAM_DISPLAY_RECT_WIDTH:
			mm_camcorder_get_attribute_info(handle, MMCAM_DISPLAY_RECT_HEIGHT, &info);
			flags |= info.flag;

			rect_width = value->value.i_val;
			break;
		case MM_CAM_DISPLAY_RECT_HEIGHT:
			rect_height = value->value.i_val;
			break;
		default:
			_mmcam_dbg_err("Wrong attr_idx!");
			return FALSE;
		}

		if (!(flags & MM_ATTRS_FLAG_MODIFIED)) {
			_mmcam_dbg_log("RECT(x,y,w,h) = (%d,%d,%d,%d)",
			               rect_x, rect_y, rect_width, rect_height);
			g_object_set(sc->element[_MMCAMCORDER_VIDEOSINK_SINK].gst,
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


bool _mmcamcorder_commit_display_scale(MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	int zoom = 0;
	int current_state = MM_CAMCORDER_STATE_NONE;
	char *videosink_name = NULL;
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
	zoom = value->value.i_val;
	if (!strcmp(videosink_name, "xvimagesink")) {
		vs_element = sc->element[_MMCAMCORDER_VIDEOSINK_SINK].gst;

		MMCAMCORDER_G_OBJECT_SET(vs_element, "zoom", zoom + 1);
		_mmcam_dbg_log("Set display zoom to %d", zoom + 1);

		return TRUE;
	} else {
		_mmcam_dbg_warn("videosink[%s] does not support scale", videosink_name);
		return FALSE;
	}
}


bool _mmcamcorder_commit_display_evas_do_scaling(MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	int current_state = MM_CAMCORDER_STATE_NONE;
	int do_scaling = 0;
	char *videosink_name = NULL;

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
	if (!strcmp(videosink_name, "evaspixmapsink")) {
		MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSINK_SINK].gst, "origin-size", !do_scaling);
		_mmcam_dbg_log("Set origin-size to %d", !(value->value.i_val));
		return TRUE;
	} else {
		_mmcam_dbg_warn("videosink[%s] does not support scale", videosink_name);
		return FALSE;
	}
}


bool _mmcamcorder_commit_strobe (MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	bool bret = FALSE;
	_MMCamcorderSubContext *sc = NULL;
	int strobe_type, mslVal, newVal, cur_value;
	int current_state = MM_CAMCORDER_STATE_NONE;

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	if (!sc)
		return TRUE;

	_mmcam_dbg_log( "Commit : strobe attribute(%d)", attr_idx );

	//status check
	current_state = _mmcamcorder_get_state( handle);
	
	if (current_state < MM_CAMCORDER_STATE_PREPARE ||
	    current_state == MM_CAMCORDER_STATE_CAPTURING) {
		//_mmcam_dbg_log("It doesn't need to change dynamically.(state=%d)", current_state);
		return TRUE;
	}

	mslVal = value->value.i_val;

	switch (attr_idx) {
	case MM_CAM_STROBE_CONTROL:
		strobe_type = GST_CAMERA_CONTROL_STROBE_CONTROL;
		newVal = _mmcamcorder_convert_msl_to_sensor(handle, MM_CAM_STROBE_CONTROL, mslVal);
		break;
	case MM_CAM_STROBE_CAPABILITIES:
		strobe_type = GST_CAMERA_CONTROL_STROBE_CAPABILITIES;
		newVal = mslVal;
		break;
	case MM_CAM_STROBE_MODE:
		/* check whether set or not */
		if (!_mmcamcorder_check_supported_attribute(handle, attr_idx)) {
			_mmcam_dbg_log("skip set value %d", mslVal);
			return TRUE;
		}

		strobe_type = GST_CAMERA_CONTROL_STROBE_MODE;
		newVal = _mmcamcorder_convert_msl_to_sensor(handle, MM_CAM_STROBE_MODE, mslVal);
		break;
	default:
		_mmcam_dbg_err("Commit : strobe attribute(attr_idx(%d) is out of range)", attr_idx);
		return FALSE;
	}

	GstCameraControl *control = NULL;
	if (!GST_IS_CAMERA_CONTROL(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst)) {
		_mmcam_dbg_err("Can't cast Video source into camera control.");
		bret = FALSE;
	} else {
		control = GST_CAMERA_CONTROL(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst);

		if (gst_camera_control_get_strobe(control, strobe_type, &cur_value)) {
			if (newVal != cur_value) {
				if (gst_camera_control_set_strobe(control, strobe_type, newVal)) {
					_mmcam_dbg_log("Succeed in setting strobe. Type[%d],value[%d]", strobe_type, mslVal);
					bret = TRUE;
				} else {
					_mmcam_dbg_warn("Set strobe failed. Type[%d],value[%d]", strobe_type, mslVal);
					bret = FALSE;
				}
			} else {
				_mmcam_dbg_log("No need to set strobe. Type[%d],value[%d]", strobe_type, mslVal);
				bret = TRUE;
			}
		} else {
			_mmcam_dbg_warn("Failed to get strobe. Type[%d]", strobe_type);
			bret = FALSE;
		}
	}

	return bret;
}


bool _mmcamcorder_commit_camera_flip(MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	int ret = 0;
	int current_state = MM_CAMCORDER_STATE_NONE;

	if ((void *)handle == NULL) {
		_mmcam_dbg_warn("handle is NULL");
		return FALSE;
	}

	_mmcam_dbg_log("Commit : flip %d", value->value.i_val);

	/* state check */
	current_state = _mmcamcorder_get_state(handle);
	if (current_state > MM_CAMCORDER_STATE_READY) {
		_mmcam_dbg_err("Can not set camera FLIP horizontal at state %d", current_state);
		return FALSE;
	} else if (current_state < MM_CAMCORDER_STATE_READY) {
		_mmcam_dbg_log("Pipeline is not created yet. This will be set when create pipeline.");
		return TRUE;
	}

	ret = _mmcamcorder_set_videosrc_flip(handle, value->value.i_val);

	_mmcam_dbg_log("ret %d", ret);

	return ret;
}


bool _mmcamcorder_commit_camera_hdr_capture(MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	int current_state = MM_CAMCORDER_STATE_NONE;

	if ((void *)handle == NULL) {
		_mmcam_dbg_warn("handle is NULL");
		return FALSE;
	}

	_mmcam_dbg_log("Commit : HDR Capture %d", value->value.i_val);

	/* check whether set or not */
	if (!_mmcamcorder_check_supported_attribute(handle, attr_idx)) {
		_mmcam_dbg_log("skip set value %d", value->value.i_val);
		return TRUE;
	}

	/* state check */
	current_state = _mmcamcorder_get_state(handle);
	if (current_state > MM_CAMCORDER_STATE_PREPARE) {
		_mmcam_dbg_err("can NOT set HDR capture at state %d", current_state);
		return FALSE;
	}

	return TRUE;
}


bool _mmcamcorder_commit_detect(MMHandleType handle, int attr_idx, const mmf_value_t *value)
{
	bool bret = FALSE;
	_MMCamcorderSubContext *sc = NULL;
	int detect_type = GST_CAMERA_CONTROL_FACE_DETECT_MODE;
	int set_value = 0;
	int current_value = 0;
	int current_state = MM_CAMCORDER_STATE_NONE;
	GstCameraControl *control = NULL;

	if ((void *)handle == NULL) {
		_mmcam_dbg_warn("handle is NULL");
		return FALSE;
	}

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	if (!sc) {
		return TRUE;
	}

	_mmcam_dbg_log("Commit : detect attribute(%d)", attr_idx);

	/* state check */
	current_state = _mmcamcorder_get_state( handle);
	if (current_state < MM_CAMCORDER_STATE_READY) {
		//_mmcam_dbg_log("It doesn't need to change dynamically.(state=%d)", current_state);
		return TRUE;
	}

	set_value = value->value.i_val;

	switch (attr_idx) {
	case MM_CAM_DETECT_MODE:
		/* check whether set or not */
		if (!_mmcamcorder_check_supported_attribute(handle, attr_idx)) {
			_mmcam_dbg_log("skip set value %d", set_value);
			return TRUE;
		}

		detect_type = GST_CAMERA_CONTROL_FACE_DETECT_MODE;
		break;
	case MM_CAM_DETECT_NUMBER:
		detect_type = GST_CAMERA_CONTROL_FACE_DETECT_NUMBER;
		break;
	case MM_CAM_DETECT_FOCUS_SELECT:
		detect_type = GST_CAMERA_CONTROL_FACE_FOCUS_SELECT;
		break;
	case MM_CAM_DETECT_SELECT_NUMBER:
		detect_type = GST_CAMERA_CONTROL_FACE_SELECT_NUMBER;
		break;
	case MM_CAM_DETECT_STATUS:
		detect_type = GST_CAMERA_CONTROL_FACE_DETECT_STATUS;
		break;
	default:
		_mmcam_dbg_err("Commit : strobe attribute(attr_idx(%d) is out of range)", attr_idx);
		return FALSE;
	}

	if (!GST_IS_CAMERA_CONTROL(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst)) {
		_mmcam_dbg_err("Can't cast Video source into camera control.");
		bret = FALSE;
	} else {
		control = GST_CAMERA_CONTROL(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst);

		if (gst_camera_control_get_detect(control, detect_type, &current_value)) {
			if (current_value == set_value) {
				_mmcam_dbg_log("No need to set detect(same). Type[%d],value[%d]", detect_type, set_value);
				bret = TRUE;
			} else {
				if (!gst_camera_control_set_detect(control, detect_type, set_value)) {
					_mmcam_dbg_warn("Set detect failed. Type[%d],value[%d]",
					                detect_type, set_value);
					bret = FALSE;
				} else {
					_mmcam_dbg_log("Set detect success. Type[%d],value[%d]",
					               detect_type, set_value);
					bret = TRUE;
				}
			}
		} else {
			_mmcam_dbg_warn("Get detect failed. Type[%d]", detect_type);
			bret = FALSE;
		}
	}

	return bret;
}


static bool
__mmcamcorder_attrs_is_supported(MMHandleType handle, int idx)
{
	mmf_attrs_t *attr = (mmf_attrs_t*)handle;
	int flag;

	if (mm_attrs_get_flags(handle, idx, &flag) == MM_ERROR_NONE) {
		if (flag == MM_ATTRS_FLAG_NONE) {
			return FALSE;
		}
	} else {
		return FALSE;
	}

	if (attr->items[idx].value_spec.type == MM_ATTRS_VALID_TYPE_INT_RANGE) {
		int min, max;
		mm_attrs_get_valid_range((MMHandleType)attr, idx, &min, &max);
		if (max < min) {
			return FALSE;
		}
	} else if (attr->items[idx].value_spec.type == MM_ATTRS_VALID_TYPE_INT_ARRAY) {
		int count;
		int *array;
		mm_attrs_get_valid_array((MMHandleType)attr, idx, &count, &array);
		if (count == 0) {
			return FALSE;
		}
	}

	return TRUE;
}


bool
_mmcamcorder_set_attribute_to_camsensor(MMHandleType handle)
{
	mmf_camcorder_t *hcamcorder= MMF_CAMCORDER(handle);
	mmf_attrs_t *attr = NULL;

	int scene_mode = MM_CAMCORDER_SCENE_MODE_NORMAL;

	int i = 0 ;
	int ret = TRUE;
	int attr_idxs_default[] = {
		MM_CAM_CAMERA_DIGITAL_ZOOM
		, MM_CAM_CAMERA_OPTICAL_ZOOM
		, MM_CAM_CAMERA_WDR
		, MM_CAM_CAMERA_HOLD_AF_AFTER_CAPTURING
		, MM_CAM_FILTER_CONTRAST
		, MM_CAM_FILTER_HUE
		, MM_CAM_STROBE_MODE
		, MM_CAM_DETECT_MODE
	};

	int attr_idxs_extra[] = {
		MM_CAM_CAMERA_ISO
		, MM_CAM_FILTER_BRIGHTNESS
		, MM_CAM_FILTER_WB
		, MM_CAM_FILTER_SATURATION
		, MM_CAM_FILTER_SHARPNESS
		, MM_CAM_FILTER_COLOR_TONE
		, MM_CAM_CAMERA_EXPOSURE_MODE
	};

	mmf_return_val_if_fail(hcamcorder, FALSE);

	_mmcam_dbg_log("Set all attribute again.");

	MMTA_ACUM_ITEM_BEGIN("                _mmcamcorder_set_attribute_to_camsensor", 0);

	attr = (mmf_attrs_t *)MMF_CAMCORDER_ATTRS(handle);
	if (attr == NULL) {
		_mmcam_dbg_err("Get attribute handle failed.");
		return FALSE;
	} else {
		/* Get Scene mode */
		mm_camcorder_get_attributes(handle, NULL, MMCAM_FILTER_SCENE_MODE, &scene_mode, NULL);

		_mmcam_dbg_log("attribute count(%d)", attr->count);

		for (i = 0 ; i < ARRAY_SIZE(attr_idxs_default) ; i++) {
			if (__mmcamcorder_attrs_is_supported((MMHandleType)attr, attr_idxs_default[i])) {
				mmf_attribute_set_modified(&(attr->items[attr_idxs_default[i]]));
			}
		}

		/* Set extra if scene mode is NORMAL */
		if (scene_mode == MM_CAMCORDER_SCENE_MODE_NORMAL) {
			for (i = 0 ; i < ARRAY_SIZE(attr_idxs_extra) ; i++) {
				if (__mmcamcorder_attrs_is_supported((MMHandleType)attr, attr_idxs_extra[i])) {
					mmf_attribute_set_modified(&(attr->items[attr_idxs_extra[i]]));
				}
			}
		} else {
			/* Set scene mode if scene mode is NOT NORMAL */
			if (__mmcamcorder_attrs_is_supported((MMHandleType)attr, MM_CAM_FILTER_SCENE_MODE)) {
				mmf_attribute_set_modified(&(attr->items[MM_CAM_FILTER_SCENE_MODE]));
			}
		}

		if (mmf_attrs_commit((MMHandleType)attr) == -1) {
			ret = FALSE;
		} else {
			ret = TRUE;
		}
	}

	MMTA_ACUM_ITEM_END("                _mmcamcorder_set_attribute_to_camsensor", 0);

	_mmcam_dbg_log("Done.");

	return ret;
}


int _mmcamcorder_lock_readonly_attributes(MMHandleType handle)
{
	mmf_camcorder_t *hcamcorder= MMF_CAMCORDER(handle);
	int table_size = 0;
	int i = 0;
	mmf_attrs_t *attr = NULL;
	int nerror = MM_ERROR_NONE ;

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_INVALID_ARGUMENT);

	attr = (mmf_attrs_t*) MMF_CAMCORDER_ATTRS(handle);
	mmf_return_val_if_fail(attr, MM_ERROR_CAMCORDER_NOT_INITIALIZED);	

	_mmcam_dbg_log("");

	table_size = ARRAY_SIZE(readonly_attributes);
	_mmcam_dbg_log("%d", table_size);
	for (i = 0; i < table_size; i++)
	{
		int sCategory = readonly_attributes[i];
		
		mmf_attribute_set_readonly(&(attr->items[sCategory]));
	}

	return nerror;
}


int _mmcamcorder_set_disabled_attributes(MMHandleType handle)
{
	mmf_camcorder_t *hcamcorder= MMF_CAMCORDER(handle);
	//int table_size = 0;
	int i = 0;
	mmf_attrs_t *attr = NULL;
	type_string_array * disabled_attr = NULL;
	int cnt_str = 0;
	int nerror = MM_ERROR_NONE ;

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_INVALID_ARGUMENT);

	attr = (mmf_attrs_t*) MMF_CAMCORDER_ATTRS(handle);
	mmf_return_val_if_fail(attr, MM_ERROR_CAMCORDER_NOT_INITIALIZED);	

	_mmcam_dbg_log("");

	/* add gst_param */
	_mmcamcorder_conf_get_value_string_array(hcamcorder->conf_main,
	                                         CONFIGURE_CATEGORY_MAIN_GENERAL,
	                                         "DisabledAttributes",
	                                         &disabled_attr);
	if (disabled_attr != NULL && disabled_attr->value) {
		cnt_str = disabled_attr->count;
		for (i = 0; i < cnt_str; i++) {
			int idx = 0;
			_mmcam_dbg_log("[%d]%s", i, disabled_attr->value[i] );
			nerror = mm_attrs_get_index((MMHandleType)attr, disabled_attr->value[i], &idx);
			if (nerror == MM_ERROR_NONE) {
				mmf_attribute_set_disabled(&(attr->items[idx]));
			} else {
				_mmcam_dbg_warn("No ATTR named %s[%d]",disabled_attr->value[i], i);
			}
		}
	}

	return nerror;
}


/*---------------------------------------------------------------------------------------
|    INTERNAL FUNCTION DEFINITIONS:							|
---------------------------------------------------------------------------------------*/
static bool __mmcamcorder_set_capture_resolution(MMHandleType handle, int width, int height)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;
	int current_state = MM_CAMCORDER_STATE_NULL;

	mmf_return_val_if_fail(hcamcorder, FALSE);

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	mmf_return_val_if_fail(sc, TRUE);

	current_state = _mmcamcorder_get_state(handle);

	if (sc->element && sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst) {
		if (current_state <= MM_CAMCORDER_STATE_PREPARE) {
			_mmcam_dbg_log("set capture width and height [%dx%d] to camera plugin", width, height);
			MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst, "capture-width", width);
			MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst, "capture-height", height);
		} else {
			_mmcam_dbg_warn("invalid state[%d]", current_state);
			return FALSE;
		}
	} else {
		_mmcam_dbg_log("element is not created yet");
	}

	return TRUE;
}


static bool __mmcamcorder_set_camera_resolution(MMHandleType handle, int width, int height)
{
	int fps = 0;
	double motion_rate = _MMCAMCORDER_DEFAULT_RECORDING_MOTION_RATE;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;
	GstCaps *caps = NULL;

	mmf_return_val_if_fail(hcamcorder, FALSE);

	sc = MMF_CAMCORDER_SUBCONTEXT(handle);
	mmf_return_val_if_fail(sc, TRUE);

	mm_camcorder_get_attributes(handle, NULL,
	                            MMCAM_CAMERA_FPS, &fps,
	                            MMCAM_CAMERA_RECORDING_MOTION_RATE, &motion_rate,
	                            NULL);

	if (hcamcorder->type != MM_CAMCORDER_MODE_AUDIO) {
		if(motion_rate != _MMCAMCORDER_DEFAULT_RECORDING_MOTION_RATE) {
			MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst, "high-speed-fps", fps);
		} else {
			MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSRC_SRC].gst, "high-speed-fps", 0);
		}
	}

	caps = gst_caps_new_simple("video/x-raw-yuv",
	                           "format", GST_TYPE_FOURCC, sc->fourcc,
	                           "width", G_TYPE_INT, width,
	                           "height", G_TYPE_INT, height,
	                           "framerate", GST_TYPE_FRACTION, fps, 1,
	                           NULL);
	MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_VIDEOSRC_FILT].gst, "caps", caps);
	gst_caps_unref(caps);

	return TRUE;
}

static int
__mmcamcorder_check_valid_pair( MMHandleType handle, char **err_attr_name, const char *attribute_name, va_list var_args )
{
	#define INIT_VALUE            -1
	#define CHECK_COUNT           2
	#define CAMERA_RESOLUTION     0
	#define CAPTURE_RESOLUTION    1

	mmf_camcorder_t *hcamcorder= MMF_CAMCORDER(handle);
	MMHandleType attrs = 0;

	int ret = MM_ERROR_NONE;
	int  i = 0, j = 0;
	char *name = NULL;
	char *check_pair_name[2][3] = {
		{ MMCAM_CAMERA_WIDTH,  MMCAM_CAMERA_HEIGHT,  "MMCAM_CAMERA_WIDTH and HEIGHT" },
		{ MMCAM_CAPTURE_WIDTH, MMCAM_CAPTURE_HEIGHT, "MMCAM_CAPTURE_WIDTH and HEIGHT" },
	};

	int check_pair_value[2][2] = {
		{ INIT_VALUE, INIT_VALUE },
		{ INIT_VALUE, INIT_VALUE },
	};

	if( hcamcorder == NULL || attribute_name == NULL )
	{
		_mmcam_dbg_warn( "handle[%p] or attribute_name[%p] is NULL.",
		                 hcamcorder, attribute_name );
		return MM_ERROR_CAMCORDER_INVALID_ARGUMENT;
	}

	if( err_attr_name )
		*err_attr_name = NULL;

	//_mmcam_dbg_log( "ENTER" );

	attrs = MMF_CAMCORDER_ATTRS(handle);

	name = (char*)attribute_name;

	while( name )
	{
		int idx = -1;
		MMAttrsType attr_type = MM_ATTRS_TYPE_INVALID;

		/*_mmcam_dbg_log( "NAME : %s", name );*/

		/* attribute name check */
		if ((ret = mm_attrs_get_index(attrs, name, &idx)) != MM_ERROR_NONE)
		{
			if (err_attr_name)
				*err_attr_name = strdup(name);

			if (ret == MM_ERROR_COMMON_OUT_OF_ARRAY)	//to avoid confusing
				return MM_ERROR_COMMON_ATTR_NOT_EXIST;
			else
				return ret;
		}

		/* type check */
		if ((ret = mm_attrs_get_type(attrs, idx, &attr_type)) != MM_ERROR_NONE)
			return ret;

		switch (attr_type)
		{
			case MM_ATTRS_TYPE_INT:
			{
				gboolean matched = FALSE;
				for( i = 0 ; i < CHECK_COUNT ; i++ ) {
					for( j = 0 ; j < 2 ; j++ ) {
						if( !strcmp( name, check_pair_name[i][j] ) )
						{
							check_pair_value[i][j] = va_arg( (var_args), int );
							_mmcam_dbg_log( "%s : %d", check_pair_name[i][j], check_pair_value[i][j] );
							matched = TRUE;
							break;
						}
					}
					if( matched )
						break;
				}
				if( matched == FALSE )
				{
					va_arg ((var_args), int);
				}				
				break;
			}
			case MM_ATTRS_TYPE_DOUBLE:
				va_arg ((var_args), double);
				break;
			case MM_ATTRS_TYPE_STRING:
				va_arg ((var_args), char*); /* string */
				va_arg ((var_args), int);   /* size */
				break;
			case MM_ATTRS_TYPE_DATA:
				va_arg ((var_args), void*); /* data */
				va_arg ((var_args), int);   /* size */
				break;
			case MM_ATTRS_TYPE_INVALID:
			default:
				_mmcam_dbg_err( "Not supported attribute type(%d, name:%s)", attr_type, name);
				if (err_attr_name)
					*err_attr_name = strdup(name);
				return  MM_ERROR_CAMCORDER_INVALID_ARGUMENT;
		}

		/* next name */
		name = va_arg (var_args, char*);
	}
	
	for( i = 0 ; i < CHECK_COUNT ; i++ )
	{
		if( check_pair_value[i][0] != INIT_VALUE || check_pair_value[i][1] != INIT_VALUE )
		{
			gboolean check_result = FALSE;
			char *err_name = NULL;
			MMCamAttrsInfo attr_info_0, attr_info_1;

			if( check_pair_value[i][0] == INIT_VALUE )
			{
				mm_attrs_get_int_by_name( attrs, check_pair_name[i][0], &check_pair_value[i][0] );
				err_name = strdup(check_pair_name[i][1]);
			}
			else if( check_pair_value[i][1] == INIT_VALUE )
			{
				mm_attrs_get_int_by_name( attrs, check_pair_name[i][1], &check_pair_value[i][1] );
				err_name = strdup(check_pair_name[i][0]);
			}
			else
			{
				err_name = strdup(check_pair_name[i][2]);
			}

			mm_camcorder_get_attribute_info(handle, check_pair_name[i][0], &attr_info_0);
			mm_camcorder_get_attribute_info(handle, check_pair_name[i][1], &attr_info_1);

			check_result = FALSE;

			for( j = 0 ; j < attr_info_0.int_array.count ; j++ ) {
				if( attr_info_0.int_array.array[j] == check_pair_value[i][0]
				 && attr_info_1.int_array.array[j] == check_pair_value[i][1] )
				{
					_mmcam_dbg_log( "Valid Pair[%s,%s] existed %dx%d[index:%d]",
					                check_pair_name[i][0], check_pair_name[i][1],
					                check_pair_value[i][0], check_pair_value[i][1], i );
					check_result = TRUE;
					break;
				}
			}

			if( check_result == FALSE )
			{
				_mmcam_dbg_err( "INVALID pair[%s,%s] %dx%d",
				                check_pair_name[i][0], check_pair_name[i][1],
				                check_pair_value[i][0], check_pair_value[i][1] );
				if (err_attr_name)
					*err_attr_name = err_name;

				return MM_ERROR_CAMCORDER_INVALID_ARGUMENT;
			}

			if (err_name) {
				free(err_name);
				err_name = NULL;
			}
		}
	}

	/*_mmcam_dbg_log("DONE");*/

	return MM_ERROR_NONE;
}


bool _mmcamcorder_check_supported_attribute(MMHandleType handle, int attr_index)
{
	MMAttrsInfo info;

	if ((void *)handle == NULL) {
		_mmcam_dbg_warn("handle %p is NULL", handle);
		return FALSE;
	}

	memset(&info, 0x0, sizeof(MMAttrsInfo));

	mm_attrs_get_info(MMF_CAMCORDER_ATTRS(handle), attr_index, &info);

	switch (info.validity_type) {
	case MM_ATTRS_VALID_TYPE_INT_ARRAY:
		_mmcam_dbg_log("int array count %d", info.int_array.count)
		if (info.int_array.count <= 1) {
			return FALSE;
		}
		break;
	case MM_ATTRS_VALID_TYPE_INT_RANGE:
		_mmcam_dbg_log("int range min %d, max %d",info.int_range.min, info.int_range.max);
		if (info.int_range.min >= info.int_range.max) {
			return FALSE;
		}
		break;
	case MM_ATTRS_VALID_TYPE_DOUBLE_ARRAY:
		_mmcam_dbg_log("double array count %d", info.double_array.count)
		if (info.double_array.count <= 1) {
			return FALSE;
		}
		break;
	case MM_ATTRS_VALID_TYPE_DOUBLE_RANGE:
		_mmcam_dbg_log("double range min %lf, max %lf",info.int_range.min, info.int_range.max);
		if (info.double_range.min >= info.double_range.max) {
			return FALSE;
		}
		break;
	default:
		_mmcam_dbg_warn("invalid type %d", info.validity_type);
		return FALSE;
	}

	return TRUE;
}
