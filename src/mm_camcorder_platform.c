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
#include "mm_camcorder_internal.h"
#include "mm_camcorder_platform.h"
#include "mm_camcorder_configure.h"

/*---------------------------------------------------------------------------
|    GLOBAL VARIABLE DEFINITIONS for internal								|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    LOCAL VARIABLE DEFINITIONS for internal								|
---------------------------------------------------------------------------*/
#define 	MM_CAMCORDER_ATTR_NONE	-1


// Rule 1. 1:1 match except NONE.
// Rule 2. MSL should be Superset.
// Rule 3. sensor value should not be same as _MMCAMCORDER_SENSOR_ENUM_NONE.

static int __enum_conv_whitebalance[MM_CAMCORDER_WHITE_BALANCE_NUM] ;

_MMCamcorderEnumConvert _mmcamcorder_enum_conv_whitebalance =
{
	MM_CAMCORDER_WHITE_BALANCE_NUM,
	__enum_conv_whitebalance,
	CONFIGURE_CATEGORY_CTRL_EFFECT,
	"WhiteBalance"
};


static int __enum_conv_colortone[MM_CAMCORDER_COLOR_TONE_NUM];

_MMCamcorderEnumConvert _mmcamcorder_enum_conv_colortone =
{
	MM_CAMCORDER_COLOR_TONE_NUM,
	__enum_conv_colortone,
	CONFIGURE_CATEGORY_CTRL_EFFECT,
	"ColorTone"
};


static int __enum_conv_iso[MM_CAMCORDER_ISO_NUM];

_MMCamcorderEnumConvert _mmcamcorder_enum_conv_iso =
{
	MM_CAMCORDER_ISO_NUM,
	__enum_conv_iso,
	CONFIGURE_CATEGORY_CTRL_PHOTOGRAPH,
	"ISO"
};


static int __enum_conv_prgrm[MM_CAMCORDER_SCENE_MODE_NUM];

_MMCamcorderEnumConvert _mmcamcorder_enum_conv_prgrm =
{
	MM_CAMCORDER_SCENE_MODE_NUM,
	__enum_conv_prgrm,
	CONFIGURE_CATEGORY_CTRL_PHOTOGRAPH,
	"ProgramMode"
};


static int __enum_conv_focus_mode[MM_CAMCORDER_FOCUS_MODE_NUM];

_MMCamcorderEnumConvert _mmcamcorder_enum_conv_focus_mode =
{
	MM_CAMCORDER_FOCUS_MODE_NUM,
	__enum_conv_focus_mode,
	CONFIGURE_CATEGORY_CTRL_PHOTOGRAPH,
	"FocusMode"
};


static int __enum_conv_focus_type[MM_CAMCORDER_AUTO_FOCUS_NUM];

_MMCamcorderEnumConvert _mmcamcorder_enum_conv_focus_type =
{
	MM_CAMCORDER_AUTO_FOCUS_NUM,
	__enum_conv_focus_type,
	CONFIGURE_CATEGORY_CTRL_PHOTOGRAPH,
	"AFType"
};


static int __enum_conv_ae_type[MM_CAMCORDER_AUTO_EXPOSURE_NUM];

_MMCamcorderEnumConvert _mmcamcorder_enum_conv_ae_type =
{
	MM_CAMCORDER_AUTO_EXPOSURE_NUM,
	__enum_conv_ae_type,
	CONFIGURE_CATEGORY_CTRL_PHOTOGRAPH,
	"AEType"
};


static int __enum_conv_strobe_ctrl[MM_CAMCORDER_STROBE_CONTROL_NUM];

_MMCamcorderEnumConvert _mmcamcorder_enum_conv_strobe_ctrl =
{
	MM_CAMCORDER_STROBE_CONTROL_NUM,
	__enum_conv_strobe_ctrl,
	CONFIGURE_CATEGORY_CTRL_STROBE,
	"StrobeControl"
};


static int __enum_conv_strobe_mode[MM_CAMCORDER_STROBE_MODE_NUM];

_MMCamcorderEnumConvert _mmcamcorder_enum_conv_strobe_mode =
{
	MM_CAMCORDER_STROBE_MODE_NUM,
	__enum_conv_strobe_mode,
	CONFIGURE_CATEGORY_CTRL_STROBE,
	"StrobeMode"
};


static int __enum_conv_wdr_ctrl[MM_CAMCORDER_WDR_NUM];

_MMCamcorderEnumConvert _mmcamcorder_enum_conv_wdr_ctrl =
{
	MM_CAMCORDER_WDR_NUM,
	__enum_conv_wdr_ctrl,
	CONFIGURE_CATEGORY_CTRL_EFFECT,
	"WDR"
};


static int __enum_conv_ahs[MM_CAMCORDER_AHS_NUM];

_MMCamcorderEnumConvert _mmcamcorder_enum_conv_ahs =
{
	MM_CAMCORDER_AHS_NUM,
	__enum_conv_ahs,
	CONFIGURE_CATEGORY_CTRL_PHOTOGRAPH,
	"AntiHandshake"
};


static int __enum_conv_video_stabilization[MM_CAMCORDER_VIDEO_STABILIZATION_NUM];

_MMCamcorderEnumConvert _mmcamcorder_enum_conv_video_stabilization =
{
	MM_CAMCORDER_VIDEO_STABILIZATION_NUM,
	__enum_conv_video_stabilization,
	CONFIGURE_CATEGORY_CTRL_PHOTOGRAPH,
	"VideoStabilization"
};


static int __enum_conv_hdr_capture[MM_CAMCORDER_HDR_CAPTURE_NUM];

_MMCamcorderEnumConvert _mmcamcorder_enum_conv_hdr_capture =
{
	MM_CAMCORDER_HDR_CAPTURE_NUM,
	__enum_conv_hdr_capture,
	CONFIGURE_CATEGORY_CTRL_CAPTURE,
	"SupportHDR"
};

static int __enum_conv_detect_mode[MM_CAMCORDER_DETECT_MODE_NUM];

_MMCamcorderEnumConvert _mmcamcorder_enum_conv_detect_mode =
{
	MM_CAMCORDER_DETECT_MODE_NUM,
	__enum_conv_detect_mode,
	CONFIGURE_CATEGORY_CTRL_DETECT,
	"DetectMode"
};


/**
 * Matching table of caminfo index with category enum of attribute.
 * If predefined item is changed, this static variables should be changed.
 * For detail information, refer below documents.
 *
 */
static _MMCamcorderInfoConverting	g_display_info[] = {
	{
		CONFIGURE_TYPE_MAIN,
		CONFIGURE_CATEGORY_MAIN_VIDEO_OUTPUT,
		MM_CAM_DISPLAY_DEVICE,
		MM_CAMCORDER_ATTR_NONE,
		"DisplayDevice",
		MM_CAMCONVERT_TYPE_INT_ARRAY,
		NULL,
	},
	{
		CONFIGURE_TYPE_MAIN,
		CONFIGURE_CATEGORY_MAIN_VIDEO_OUTPUT,
		MM_CAM_DISPLAY_MODE,
		MM_CAMCORDER_ATTR_NONE,
		"DisplayMode",
		MM_CAMCONVERT_TYPE_INT_ARRAY,
		NULL,
	},
	{
		CONFIGURE_TYPE_MAIN,
		CONFIGURE_CATEGORY_MAIN_VIDEO_OUTPUT,
		MM_CAM_DISPLAY_SURFACE,
		MM_CAMCORDER_ATTR_NONE,
		"Videosink",
		MM_CAMCONVERT_TYPE_INT_ARRAY,
		NULL,
	},
};

static _MMCamcorderInfoConverting	g_caminfo_convert[CAMINFO_CONVERT_NUM] = {
	{
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_CAMERA,
		MM_CAM_CAMERA_DEVICE_NAME,
		MM_CAMCORDER_ATTR_NONE,
		"DeviceName",
		MM_CAMCONVERT_TYPE_STRING,
		NULL,
	},
	{
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_CAMERA,
		MM_CAM_CAMERA_WIDTH,
		MM_CAM_CAMERA_HEIGHT,
		"PreviewResolution",
		MM_CAMCONVERT_TYPE_INT_PAIR_ARRAY,
		NULL,
	},
	{
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_CAMERA,
		MM_CAM_CAPTURE_WIDTH,
		MM_CAM_CAPTURE_HEIGHT,
		"CaptureResolution",
		MM_CAMCONVERT_TYPE_INT_PAIR_ARRAY,
		NULL,
	},
	{
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_CAMERA,
		MM_CAM_CAMERA_FPS,
		MM_CAMCORDER_ATTR_NONE,
		"FPS",
		MM_CAMCONVERT_TYPE_INT_ARRAY,
		NULL,
	},
	{ /* 5 */
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_CAMERA,
		MM_CAM_CAMERA_FORMAT,
		MM_CAMCORDER_ATTR_NONE,
		"PictureFormat",
		MM_CAMCONVERT_TYPE_INT_ARRAY,
		NULL,
	},
	{
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_STROBE,
		MM_CAM_STROBE_CONTROL,
		MM_CAMCORDER_ATTR_NONE,
		"StrobeControl",
		MM_CAMCONVERT_TYPE_INT_ARRAY,
		&_mmcamcorder_enum_conv_strobe_ctrl,
	},
	{
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_STROBE,
		MM_CAM_STROBE_CAPABILITIES,
		MM_CAMCORDER_ATTR_NONE,
		"StrobeCapabilities",
		MM_CAMCONVERT_TYPE_INT_ARRAY,
		NULL,
	},
	{
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_STROBE,
		MM_CAM_STROBE_MODE,
		MM_CAMCORDER_ATTR_NONE,
		"StrobeMode",
		MM_CAMCONVERT_TYPE_INT_ARRAY,
		&_mmcamcorder_enum_conv_strobe_mode,
	},
	{
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_EFFECT,
		MM_CAM_FILTER_BRIGHTNESS,
		MM_CAMCORDER_ATTR_NONE,
		"Brightness",
		MM_CAMCONVERT_TYPE_INT_RANGE,
		NULL,
	},
	{ /* 10 */
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_EFFECT,
		MM_CAM_FILTER_CONTRAST,
		MM_CAMCORDER_ATTR_NONE,
		"Contrast",
		MM_CAMCONVERT_TYPE_INT_RANGE,
		NULL,
	},
	{
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_EFFECT,
		MM_CAM_FILTER_SATURATION,
		MM_CAMCORDER_ATTR_NONE,
		"Saturation",
		MM_CAMCONVERT_TYPE_INT_RANGE,
		NULL,
	},
	{
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_EFFECT,
		MM_CAM_FILTER_HUE,
		MM_CAMCORDER_ATTR_NONE,
		"Hue",
		MM_CAMCONVERT_TYPE_INT_RANGE,
		NULL,
	},
	{
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_EFFECT,
		MM_CAM_FILTER_SHARPNESS,
		MM_CAMCORDER_ATTR_NONE,
		"Sharpness",
		MM_CAMCONVERT_TYPE_INT_RANGE,
		NULL,
	},
	{
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_EFFECT,
		MM_CAM_FILTER_WB,
		MM_CAMCORDER_ATTR_NONE,
		"WhiteBalance",
		MM_CAMCONVERT_TYPE_INT_ARRAY,
		&_mmcamcorder_enum_conv_whitebalance,
	},
	{ /* 15 */
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_EFFECT,
		MM_CAM_FILTER_COLOR_TONE,
		MM_CAMCORDER_ATTR_NONE,
		"ColorTone",
		MM_CAMCONVERT_TYPE_INT_ARRAY,
		&_mmcamcorder_enum_conv_colortone,
	},
	{
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_EFFECT,
		MM_CAM_CAMERA_WDR,
		MM_CAMCORDER_ATTR_NONE,
		"WDR",
		MM_CAMCONVERT_TYPE_INT_ARRAY,
		&_mmcamcorder_enum_conv_wdr_ctrl,
	},
	{
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_PHOTOGRAPH,
		MM_CAM_CAMERA_DIGITAL_ZOOM,
		MM_CAMCORDER_ATTR_NONE,
		"DigitalZoom",
		MM_CAMCONVERT_TYPE_INT_RANGE,
		NULL,
	},
	{
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_PHOTOGRAPH,
		MM_CAM_CAMERA_OPTICAL_ZOOM,
		MM_CAMCORDER_ATTR_NONE,
		"OpticalZoom",
		MM_CAMCONVERT_TYPE_INT_RANGE,
		NULL,
	},
	{
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_PHOTOGRAPH,
		MM_CAM_CAMERA_FOCUS_MODE,
		MM_CAMCORDER_ATTR_NONE,
		"FocusMode",
		MM_CAMCONVERT_TYPE_INT_ARRAY,
		&_mmcamcorder_enum_conv_focus_mode,
	},
	{ /* 20 */
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_PHOTOGRAPH,
		MM_CAM_CAMERA_AF_SCAN_RANGE,
		MM_CAMCORDER_ATTR_NONE,
		"AFType",
		MM_CAMCONVERT_TYPE_INT_ARRAY,
		&_mmcamcorder_enum_conv_focus_type,
	},
	{
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_PHOTOGRAPH,
		MM_CAM_CAMERA_EXPOSURE_MODE,
		MM_CAMCORDER_ATTR_NONE,
		"AEType",
		MM_CAMCONVERT_TYPE_INT_ARRAY,
		&_mmcamcorder_enum_conv_ae_type,
	},
	{
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_PHOTOGRAPH,
		MM_CAM_CAMERA_EXPOSURE_VALUE,
		MM_CAMCORDER_ATTR_NONE,
		"ExposureValue",
		MM_CAMCONVERT_TYPE_INT_RANGE,
		NULL,
	},
	{
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_PHOTOGRAPH,
		MM_CAM_CAMERA_F_NUMBER,
		MM_CAMCORDER_ATTR_NONE,
		"FNumber",
		MM_CAMCONVERT_TYPE_INT_ARRAY,
		NULL,
	},
	{
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_PHOTOGRAPH,
		MM_CAM_CAMERA_SHUTTER_SPEED,
		MM_CAMCORDER_ATTR_NONE,
		"ShutterSpeed",
		MM_CAMCONVERT_TYPE_INT_ARRAY,
		NULL,
	},
	{ /* 25 */
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_PHOTOGRAPH,
		MM_CAM_CAMERA_ISO,
		MM_CAMCORDER_ATTR_NONE,
		"ISO",
		MM_CAMCONVERT_TYPE_INT_ARRAY,
		&_mmcamcorder_enum_conv_iso,
	},
	{
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_PHOTOGRAPH,
		MM_CAM_FILTER_SCENE_MODE,
		MM_CAMCORDER_ATTR_NONE,
		"ProgramMode",
		MM_CAMCONVERT_TYPE_INT_ARRAY,
		&_mmcamcorder_enum_conv_prgrm,
	},
	{
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_PHOTOGRAPH,
		MM_CAM_CAMERA_ANTI_HANDSHAKE,
		MM_CAMCORDER_ATTR_NONE,
		"AntiHandshake",
		MM_CAMCONVERT_TYPE_INT_ARRAY,
		&_mmcamcorder_enum_conv_ahs,
	},
	{
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_CAPTURE,
		MM_CAM_CAPTURE_FORMAT,
		MM_CAMCORDER_ATTR_NONE,
		"OutputMode",
		MM_CAMCONVERT_TYPE_INT_ARRAY,
		NULL,
	},
	{
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_CAPTURE,
		MM_CAM_IMAGE_ENCODER_QUALITY,
		MM_CAMCORDER_ATTR_NONE,
		"JpegQuality",
		MM_CAMCONVERT_TYPE_INT_RANGE,
		NULL,
	},
	{ /* 30 */
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_CAPTURE,
		MM_CAM_CAPTURE_COUNT,
		MM_CAMCORDER_ATTR_NONE,
		"MultishotNumber",
		MM_CAMCONVERT_TYPE_INT_RANGE,
		NULL,
	},
	{
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_CAPTURE,
		MM_CAM_CAMERA_HDR_CAPTURE,
		MM_CAMCORDER_ATTR_NONE,
		"SupportHDR",
		MM_CAMCONVERT_TYPE_INT_ARRAY,
		&_mmcamcorder_enum_conv_hdr_capture,
	},
	{
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_DETECT,
		MM_CAM_DETECT_MODE,
		MM_CAMCORDER_ATTR_NONE,
		"DetectMode",
		MM_CAMCONVERT_TYPE_INT_ARRAY,
		&_mmcamcorder_enum_conv_detect_mode,
	},
	{
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_DETECT,
		MM_CAM_DETECT_NUMBER,
		MM_CAMCORDER_ATTR_NONE,
		"DetectNumber",
		MM_CAMCONVERT_TYPE_INT_RANGE,
		NULL,
	},
	{
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_DETECT,
		MM_CAM_DETECT_FOCUS_SELECT,
		MM_CAMCORDER_ATTR_NONE,
		"DetectFocusSelect",
		MM_CAMCONVERT_TYPE_INT_RANGE,
		NULL,
	},
	{ /* 35 */
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_DETECT,
		MM_CAM_DETECT_SELECT_NUMBER,
		MM_CAMCORDER_ATTR_NONE,
		"DetectSelectNumber",
		MM_CAMCONVERT_TYPE_INT_RANGE,
		NULL,
	},
	{
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_DETECT,
		MM_CAM_DETECT_STATUS,
		MM_CAMCORDER_ATTR_NONE,
		"DetectStatus",
		MM_CAMCONVERT_TYPE_INT_ARRAY,
		NULL,
	},
	{
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_PHOTOGRAPH,
		MM_CAM_CAMERA_FACE_ZOOM_MODE,
		MM_CAMCORDER_ATTR_NONE,
		"FaceZoomMode",
		MM_CAMCONVERT_TYPE_INT_ARRAY,
		NULL,
	},
	{
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_PHOTOGRAPH,
		MM_CAM_CAMERA_FACE_ZOOM_LEVEL,
		MM_CAMCORDER_ATTR_NONE,
		"FaceZoomLevel",
		MM_CAMCONVERT_TYPE_INT_RANGE,
		NULL,
	},
	{
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_CAMERA,
		MM_CAM_RECOMMEND_CAMERA_WIDTH,
		MM_CAM_RECOMMEND_CAMERA_HEIGHT,
		"RecommendPreviewResolution",
		MM_CAMCONVERT_TYPE_INT_PAIR_ARRAY,
		NULL,
	},
	{ /* 40 */
		CONFIGURE_TYPE_CTRL,
		CONFIGURE_CATEGORY_CTRL_PHOTOGRAPH,
		MM_CAM_CAMERA_VIDEO_STABILIZATION,
		MM_CAMCORDER_ATTR_NONE,
		"VideoStabilization",
		MM_CAMCONVERT_TYPE_INT_ARRAY,
		&_mmcamcorder_enum_conv_video_stabilization,
	}
};

/*---------------------------------------------------------------------------
|    LOCAL FUNCTION PROTOTYPES:												|
---------------------------------------------------------------------------*/
/* STATIC INTERNAL FUNCTION */
static int  __mmcamcorder_set_info_to_attr( MMHandleType handle, _MMCamcorderInfoConverting *info, int table_size );
static int  __mmcamcorder_get_valid_array(int * original_array, int original_count, int ** valid_array, int * valid_default );

/*===========================================================================================
|																							|
|  FUNCTION DEFINITIONS																		|
|  																							|
========================================================================================== */
/*---------------------------------------------------------------------------
|    GLOBAL FUNCTION DEFINITIONS:											|
---------------------------------------------------------------------------*/
//convert MSL value to sensor value
int _mmcamcorder_convert_msl_to_sensor(MMHandleType handle, int attr_idx, int mslval)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderInfoConverting *info = NULL;
	int i =0;
	int size = sizeof(g_caminfo_convert) / sizeof(_MMCamcorderInfoConverting);

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	//_mmcam_dbg_log("attr_idx(%d), mslval(%d)", attr_idx, mslval);

	info = hcamcorder->caminfo_convert;

	for (i = 0; i < size; i++)
	{
		if (info[i].attr_idx == attr_idx)
		{
			_MMCamcorderEnumConvert * enum_convert = NULL;
			enum_convert = info[i].enum_convert;

			if (enum_convert == NULL)
			{
				//_mmcam_dbg_log("enum_convert is NULL. Just return the original value.");
				return mslval;
			}

			if (enum_convert->enum_arr == NULL)
			{
				_mmcam_dbg_warn("Unexpected error. Array pointer of enum_convert is NULL. Just return the original value.");
				return mslval;
			}

			if( enum_convert->total_enum_num > mslval && mslval >= 0 )
			{
				//_mmcam_dbg_log("original value(%d) -> converted value(%d)", mslval, enum_convert->enum_arr[mslval]);
				return enum_convert->enum_arr[mslval];
			}
			else
			{
				_mmcam_dbg_warn( "Input mslval[%d] is invalid(out of array[idx:%d,size:%d]), so can not convert. Just return the original value.", mslval, attr_idx, enum_convert->total_enum_num );
				return mslval;
			}
		}
	}

	_mmcam_dbg_warn("There is no category to match. Just return the original value.");

	return mslval;
}

//convert sensor value to MSL value
int _mmcamcorder_convert_sensor_to_msl(MMHandleType handle, int attr_idx, int sensval)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderInfoConverting *info = NULL;
	int i = 0, j = 0;
	int size = sizeof(g_caminfo_convert) / sizeof(_MMCamcorderInfoConverting);

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	info = hcamcorder->caminfo_convert;
	
	for( i = 0 ; i < size ; i++ )
	{
		if( info[i].attr_idx == attr_idx )
		{
			_MMCamcorderEnumConvert * enum_convert = NULL;
			enum_convert = info[i].enum_convert;

			if( enum_convert == NULL )
			{
				//_mmcam_dbg_log("enum_convert is NULL. Just return the original value.");
				return sensval;
			}

			if( enum_convert->enum_arr == NULL )
			{
				_mmcam_dbg_warn("Unexpected error. Array pointer of enum_convert is NULL. Just return the original value.");
				return sensval;
			}

			for( j = 0 ; j < enum_convert->total_enum_num ; j++ )
			{
				if( sensval == enum_convert->enum_arr[j] )
				{
					//_mmcam_dbg_log("original value(%d) -> converted value(%d)", sensval, j);
					return j;
				}
			}

			_mmcam_dbg_warn("There is no sensor value matched with input param. Just return the original value.");
			return sensval;

		}
	}

	_mmcam_dbg_log("There is no category to match. Just return the original value.");
	return sensval;
}

static int
__mmcamcorder_get_valid_array(int * original_array, int original_count, int ** valid_array, int * valid_default )
{
	int i = 0;
	int valid_count = 0;
	int new_default = _MMCAMCORDER_SENSOR_ENUM_NONE;
	
	for (i = 0; i < original_count; i++)
	{
		if (original_array[i] != _MMCAMCORDER_SENSOR_ENUM_NONE)
			valid_count++;
	}

	if (valid_count > 0)
	{
		*valid_array = (int*)malloc(sizeof(int) * valid_count);

		valid_count = 0;
		for (i = 0; i < original_count; i++)
		{
			if (original_array[i] != _MMCAMCORDER_SENSOR_ENUM_NONE)
			{
				(*valid_array)[valid_count++] = i;
				//_mmcam_dbg_log( "valid_array[%d] = %d", valid_count-1, (*valid_array)[valid_count-1] );

				if( original_array[i] == *valid_default )
				{
					new_default = i;
					//_mmcam_dbg_log( "converted MSL default[%d]", new_default );
				}
			}
		}		
	}

	if( new_default != _MMCAMCORDER_SENSOR_ENUM_NONE )
	{
		*valid_default = new_default;
	}

	return valid_count;
}


int _mmcamcorder_init_attr_from_configure(MMHandleType handle)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderInfoConverting *info = NULL;

	int table_size = 0;
	int ret = MM_ERROR_NONE;

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	_mmcam_dbg_log("");

	/* Initialize attribute related to camera control */
	info = hcamcorder->caminfo_convert;
	table_size = sizeof(g_caminfo_convert) / sizeof(_MMCamcorderInfoConverting);
	ret = __mmcamcorder_set_info_to_attr( handle, info, table_size );
	if( ret != MM_ERROR_NONE )
	{
		_mmcam_dbg_err( "ret : %x", ret );
		return ret;
	}

	/* Initialize attribute related to display */
	info = g_display_info;
	table_size = sizeof(g_display_info) / sizeof(_MMCamcorderInfoConverting);
	ret = __mmcamcorder_set_info_to_attr( handle, info, table_size );
	_mmcam_dbg_log( "result: %x", ret );

	return ret;
}

static int
__mmcamcorder_set_info_to_attr( MMHandleType handle, _MMCamcorderInfoConverting *info, int table_size )
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	MMHandleType     attrs      = 0;

	int i;

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	attrs = MMF_CAMCORDER_ATTRS (handle);
	mmf_return_val_if_fail(attrs, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	camera_conf *conf_info = NULL;

	for (i = 0; i < table_size ; i++ )
	{
		int ret = MM_ERROR_NONE;

		/*
		_mmcam_dbg_log( "%d,%d,%d,%d,%s,%d",
				info[i].type,
				info[i].category,
				info[i].attr_idx,
				info[i].attr_idx_sub,
				info[i].keyword,
				info[i].conv_type );
		*/

		if( info[i].type == CONFIGURE_TYPE_MAIN )
		{
			conf_info = hcamcorder->conf_main;
			_mmcam_dbg_log( "MAIN configure [%s]", info[i].keyword );
		}
		else
		{
			conf_info = hcamcorder->conf_ctrl;
			_mmcam_dbg_log( "CTRL configure [%s]", info[i].keyword );
		}

		switch(info[i].conv_type)
		{
			case MM_CAMCONVERT_TYPE_INT:
			{
				int iarray = (int)NULL;

				if (!_mmcamcorder_conf_get_value_int( conf_info, info[i].category, info[i].keyword, &iarray ))
				{
					ret = MM_ERROR_CAMCORDER_CREATE_CONFIGURE;
					break;		//skip to set
				}

				ret = mm_attrs_set_int(MMF_CAMCORDER_ATTRS(hcamcorder), info[i].attr_idx, iarray);

				//_mmcam_dbg_log("INT . m:%d, s:%d, arr=%d", info[i].main_key, info[i].sub_key1, iarray);
				break;
			}
			case MM_CAMCONVERT_TYPE_INT_ARRAY:
			{
				int *iarray = NULL;
				int iarray_size = 0;
				int idefault = 0;
				type_int_array *tarray = NULL;

				if (!_mmcamcorder_conf_get_value_int_array( conf_info, info[i].category, info[i].keyword, &tarray ))
				{
					ret = MM_ERROR_CAMCORDER_CREATE_CONFIGURE;
					break;		//skip to set
				}

				if( tarray )
				{
					idefault = tarray->default_value;
					
					if( info[i].enum_convert )
					{
						iarray_size = __mmcamcorder_get_valid_array(tarray->value, tarray->count, &iarray, &idefault);
					}
					else
					{
						iarray = tarray->value;
						iarray_size = tarray->count;
					}

					if( iarray_size > 0 )
					{
						/*
						_mmcam_dbg_log("INT Array. m:%d, s:%d, arr=%x, size=%d, default=%d", 
								info[i].main_key, info[i].sub_key1, iarray, iarray_size, idefault);
						*/
						
						mmf_attrs_set_valid_type(attrs, info[i].attr_idx, MM_ATTRS_VALID_TYPE_INT_ARRAY);
						mmf_attrs_set_valid_array(attrs,  info[i].attr_idx, iarray, iarray_size, idefault);
					}

					ret = mm_attrs_set_int(MMF_CAMCORDER_ATTRS(hcamcorder), info[i].attr_idx, idefault);
				}

				if (iarray && iarray != tarray->value )
					free(iarray);

				break;
			}
			case MM_CAMCONVERT_TYPE_INT_RANGE:
			{
				type_int_range *irange = NULL;

				if (!_mmcamcorder_conf_get_value_int_range(conf_info, info[i].category, info[i].keyword, &irange))
				{
					ret = MM_ERROR_CAMCORDER_CREATE_CONFIGURE;
					break;		//skip to set
				}

				if( irange )
				{
					//_mmcam_dbg_log("INT Range. m:%d, s:%d, min=%d, max=%d", info[i].main_key, info[i].sub_key1, irange->min, irange->max);
				
					mmf_attrs_set_valid_type (attrs, info[i].attr_idx, MM_ATTRS_VALID_TYPE_INT_RANGE);
					mmf_attrs_set_valid_range(attrs, info[i].attr_idx, irange->min, irange->max, irange->default_value);

					ret = mm_attrs_set_int(MMF_CAMCORDER_ATTRS(hcamcorder), info[i].attr_idx, irange->default_value);
				}

				break;
			}
			case MM_CAMCONVERT_TYPE_STRING:
			{
				char* cString = NULL;
				unsigned int iString_len = 0;

				if (!_mmcamcorder_conf_get_value_string(conf_info, info[i].category, info[i].keyword, &cString ))
				{
					ret = MM_ERROR_CAMCORDER_CREATE_CONFIGURE;
					break;		//skip to set
				}

				//_mmcam_dbg_log("String. m:%d, s:%d, cString=%s", info[i].main_key, info[i].sub_key1, cString);
				//strlen makes a crash when null pointer is passed.
				if (cString)
					iString_len = strlen(cString);
				else
					iString_len = 0;

				ret = mm_attrs_set_string(attrs, info[i].attr_idx, cString, iString_len);
				break;
			}
			case MM_CAMCONVERT_TYPE_INT_PAIR_ARRAY:
			{
				type_int_pair_array *pair_array = NULL;

				//_mmcam_dbg_log("INT PAIR Array. type:%d, attr_idx:%d, attr_idx_pair:%d", info[i].type, info[i].attr_idx, info[i].attr_idx_pair);

				if (!_mmcamcorder_conf_get_value_int_pair_array(conf_info, info[i].category, info[i].keyword, &pair_array))
				{
					ret = MM_ERROR_CAMCORDER_CREATE_CONFIGURE;
					break;		//skip to set
				}

				if( pair_array && pair_array->count > 0 )
				{
					mmf_attrs_set_valid_type(attrs, info[i].attr_idx, MM_ATTRS_VALID_TYPE_INT_ARRAY);
					mmf_attrs_set_valid_array(attrs, info[i].attr_idx,
					                          pair_array->value[0],
					                          pair_array->count,
					                          pair_array->default_value[0]);
					mmf_attrs_set_valid_type(attrs, info[i].attr_idx_pair, MM_ATTRS_VALID_TYPE_INT_ARRAY);
					mmf_attrs_set_valid_array(attrs, info[i].attr_idx_pair,
					                          pair_array->value[1],
					                          pair_array->count,
					                          pair_array->default_value[1]);

					mm_attrs_set_int(MMF_CAMCORDER_ATTRS(hcamcorder), info[i].attr_idx, pair_array->default_value[0]);
					mm_attrs_set_int(MMF_CAMCORDER_ATTRS(hcamcorder), info[i].attr_idx_pair, pair_array->default_value[1]);
				}
				break;
			}

			case MM_CAMCONVERT_TYPE_USER:
			default:
				_mmcam_dbg_log("default : s:%d", info[i].attr_idx);
				break;
		}
		
		if (ret != MM_ERROR_NONE)
		{
			_mmcam_dbg_warn("Setting error. ( s:%d, ret:%x)", info[i].attr_idx, ret);
		}
	}

	if (mmf_attrs_commit(attrs) == -1)
		return MM_ERROR_CAMCORDER_INVALID_ARGUMENT;
	else	
		return MM_ERROR_NONE;
}


int _mmcamcorder_set_converted_value(MMHandleType handle, _MMCamcorderEnumConvert * convert)
{
	mmf_camcorder_t* hcamcorder = MMF_CAMCORDER(handle);
	type_int_array *array = NULL;

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	_mmcamcorder_conf_get_value_int_array( hcamcorder->conf_ctrl, convert->category, convert->keyword, &array );

	if( array )
	{
		convert->enum_arr = array->value;
	}

	return MM_ERROR_NONE;
}


int _mmcamcorder_init_convert_table(MMHandleType handle)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	int enum_conv_size = sizeof(_MMCamcorderEnumConvert);
	int caminfo_conv_size = sizeof(g_caminfo_convert);
	int caminfo_conv_length = sizeof(g_caminfo_convert) / sizeof(_MMCamcorderInfoConverting);
	int i = 0;

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	/* copy default conv data into memory of handle */
	memcpy(&(hcamcorder->caminfo_convert), &g_caminfo_convert, caminfo_conv_size);
	memcpy(&(hcamcorder->enum_conv[ENUM_CONVERT_WHITE_BALANCE]), &_mmcamcorder_enum_conv_whitebalance, enum_conv_size);
	memcpy(&(hcamcorder->enum_conv[ENUM_CONVERT_COLOR_TONE]), &_mmcamcorder_enum_conv_colortone, enum_conv_size);
	memcpy(&(hcamcorder->enum_conv[ENUM_CONVERT_ISO]), &_mmcamcorder_enum_conv_iso, enum_conv_size);
	memcpy(&(hcamcorder->enum_conv[ENUM_CONVERT_PROGRAM_MODE]), &_mmcamcorder_enum_conv_prgrm, enum_conv_size);
	memcpy(&(hcamcorder->enum_conv[ENUM_CONVERT_FOCUS_MODE]), &_mmcamcorder_enum_conv_focus_mode, enum_conv_size);
	memcpy(&(hcamcorder->enum_conv[ENUM_CONVERT_AF_RANGE]), &_mmcamcorder_enum_conv_focus_type, enum_conv_size);
	memcpy(&(hcamcorder->enum_conv[ENUM_CONVERT_EXPOSURE_MODE]), &_mmcamcorder_enum_conv_ae_type, enum_conv_size);
	memcpy(&(hcamcorder->enum_conv[ENUM_CONVERT_STROBE_MODE]), &_mmcamcorder_enum_conv_strobe_mode, enum_conv_size);
	memcpy(&(hcamcorder->enum_conv[ENUM_CONVERT_WDR]), &_mmcamcorder_enum_conv_wdr_ctrl, enum_conv_size);
	memcpy(&(hcamcorder->enum_conv[ENUM_CONVERT_ANTI_HAND_SHAKE]), &_mmcamcorder_enum_conv_ahs, enum_conv_size);
	memcpy(&(hcamcorder->enum_conv[ENUM_CONVERT_VIDEO_STABILIZATION]), &_mmcamcorder_enum_conv_video_stabilization, enum_conv_size);

	/* set ini info to conv data */
	_mmcamcorder_set_converted_value(handle, &(hcamcorder->enum_conv[ENUM_CONVERT_WHITE_BALANCE]));
	_mmcamcorder_set_converted_value(handle, &(hcamcorder->enum_conv[ENUM_CONVERT_COLOR_TONE]));
	_mmcamcorder_set_converted_value(handle, &(hcamcorder->enum_conv[ENUM_CONVERT_ISO]));
	_mmcamcorder_set_converted_value(handle, &(hcamcorder->enum_conv[ENUM_CONVERT_PROGRAM_MODE]));
	_mmcamcorder_set_converted_value(handle, &(hcamcorder->enum_conv[ENUM_CONVERT_FOCUS_MODE]));
	_mmcamcorder_set_converted_value(handle, &(hcamcorder->enum_conv[ENUM_CONVERT_AF_RANGE]));
	_mmcamcorder_set_converted_value(handle, &(hcamcorder->enum_conv[ENUM_CONVERT_EXPOSURE_MODE]));
	_mmcamcorder_set_converted_value(handle, &(hcamcorder->enum_conv[ENUM_CONVERT_STROBE_MODE]));
	_mmcamcorder_set_converted_value(handle, &(hcamcorder->enum_conv[ENUM_CONVERT_WDR]));
	_mmcamcorder_set_converted_value(handle, &(hcamcorder->enum_conv[ENUM_CONVERT_ANTI_HAND_SHAKE]));
	_mmcamcorder_set_converted_value(handle, &(hcamcorder->enum_conv[ENUM_CONVERT_VIDEO_STABILIZATION]));

	/* set modified conv data to handle */
	for (i = 0 ; i < caminfo_conv_length ; i++) {
		if (hcamcorder->caminfo_convert[i].type == CONFIGURE_TYPE_CTRL) {
			switch (hcamcorder->caminfo_convert[i].category) {
			case CONFIGURE_CATEGORY_CTRL_STROBE:
				if (!strcmp(hcamcorder->caminfo_convert[i].keyword, "StrobeMode")) {
					hcamcorder->caminfo_convert[i].enum_convert = &(hcamcorder->enum_conv[ENUM_CONVERT_STROBE_MODE]);
				}
				break;
			case CONFIGURE_CATEGORY_CTRL_EFFECT:
				if (!strcmp(hcamcorder->caminfo_convert[i].keyword, "WhiteBalance")) {
					hcamcorder->caminfo_convert[i].enum_convert = &(hcamcorder->enum_conv[ENUM_CONVERT_WHITE_BALANCE]);
				} else if (!strcmp(hcamcorder->caminfo_convert[i].keyword, "ColorTone")) {
					hcamcorder->caminfo_convert[i].enum_convert = &(hcamcorder->enum_conv[ENUM_CONVERT_COLOR_TONE]);
				} else if (!strcmp(hcamcorder->caminfo_convert[i].keyword, "WDR")) {
					hcamcorder->caminfo_convert[i].enum_convert = &(hcamcorder->enum_conv[ENUM_CONVERT_WDR]);
				}
				break;
			case CONFIGURE_CATEGORY_CTRL_PHOTOGRAPH:
				if (!strcmp(hcamcorder->caminfo_convert[i].keyword, "FocusMode")) {
					hcamcorder->caminfo_convert[i].enum_convert = &(hcamcorder->enum_conv[ENUM_CONVERT_FOCUS_MODE]);
				} else if (!strcmp(hcamcorder->caminfo_convert[i].keyword, "AFType")) {
					hcamcorder->caminfo_convert[i].enum_convert = &(hcamcorder->enum_conv[ENUM_CONVERT_AF_RANGE]);
				} else if (!strcmp(hcamcorder->caminfo_convert[i].keyword, "AEType")) {
					hcamcorder->caminfo_convert[i].enum_convert = &(hcamcorder->enum_conv[ENUM_CONVERT_EXPOSURE_MODE]);
				} else if (!strcmp(hcamcorder->caminfo_convert[i].keyword, "ISO")) {
					hcamcorder->caminfo_convert[i].enum_convert = &(hcamcorder->enum_conv[ENUM_CONVERT_ISO]);
				} else if (!strcmp(hcamcorder->caminfo_convert[i].keyword, "ProgramMode")) {
					hcamcorder->caminfo_convert[i].enum_convert = &(hcamcorder->enum_conv[ENUM_CONVERT_PROGRAM_MODE]);
				} else if (!strcmp(hcamcorder->caminfo_convert[i].keyword, "AntiHandshake")) {
					hcamcorder->caminfo_convert[i].enum_convert = &(hcamcorder->enum_conv[ENUM_CONVERT_ANTI_HAND_SHAKE]);
				} else if (!strcmp(hcamcorder->caminfo_convert[i].keyword, "VideoStabilization")) {
					hcamcorder->caminfo_convert[i].enum_convert = &(hcamcorder->enum_conv[ENUM_CONVERT_VIDEO_STABILIZATION]);
				}
				break;
			default:
				break;
			}
		}
	}

	return MM_ERROR_NONE;
}


double _mmcamcorder_convert_volume(int mslVal)
{
	double newVal = -1;
	switch (mslVal)
	{
		case 0:
			newVal = 0;
			break;
		case 1:
			newVal = 1;
			break;
		default:
			_mmcam_dbg_warn("out of range");
			break;
	}

	return newVal;
}



