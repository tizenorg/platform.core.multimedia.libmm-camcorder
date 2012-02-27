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

/*!	\brief The definitions for exif information

	Some of the EXIF informations aren't needed to be changed within a
	session. For example, everytime still-image taken, [Maker name] field
	or [Model name] field always be same in a device, while [Datetime] or
	[Exposure value] isn't. This definitions are for the un-changeable
	values.
*/

/*!
	\def MM_MAKER_NAME
	Shows manufacturer of digicam, "Samsung".
*/
/*!
	\def MM_MODEL_NAME
	Shows model number of digicam.
*/
/*!
	\def MM_SOFTWARE_NAME
	Shows firmware(internal software of digicam) version number.
*/
/*!
	\def MM_EXIF_ORIENTATION
	The orientation of the camera relative to the scene, when the image
	was captured. The start point of stored data is, '1' means upper left,
	'3' lower right, '6' upper right, '8' lower left, '9' undefined.
*/
/*!
	\def MM_EXIF_YCBCRPOSITIONING
	When image format is YCbCr and uses 'Subsampling'(cropping of chroma
	data, all the digicam do that), this value defines the chroma sample
	point of subsampled pixel array. '1' means the center of pixel array,
	'2' means the datum point(0,0).
*/
/*!
	\def MM_COMPONENTS_CONFIGURATION
	Order arrangement of pixel.
*/
/*!
	\def MM_EXIF_COLORSPACE
	Color space of stilled JPEG image.
*/
/*!
	\def MM_EXIF_CUSTOMRENDERED
	The method of compressed jpeg image rendering.
*/

#ifndef __MM_EXIFDEF_H__
#define __MM_EXIFDEF_H__

/*=======================================================================================
| MACRO DEFINITIONS									|
========================================================================================*/
#define MM_EXIF_VERSION			(0x00000030) | (0x00000032 << 8) | (0x00000032 << 16) | (0x00000030 << 24)	/* ASCII 0220 */
#define MM_MAKER_NAME			"SAMSUNG"
#define MM_USER_COMMENT			"User comment "
#define MM_SOFTWARE_NAME		"Camera Application "
#define MM_EXIF_ORIENTATION		1	/* upper-top */
#define MM_EXIF_YCBCRPOSITIONING	1	/* centered */
#define MM_COMPONENTS_CONFIGURATION	(0x00000000) | (0x00000001) | (0x00000002 << 8) | (0x00000003 << 16)		/* Y Cb Cr - */
#define MM_EXIF_COLORSPACE		1	/* sRGB */
#define MM_EXIF_CUSTOMRENDERED		0	/* Normal rendered */
#define MM_EXPOSURE_PROGRAM		3	/* 0~8 0 : not defined */
#define MM_METERING_MODE		0	/* 0~6 0: unkown */
#define MM_SENSING_MODE			1	/* 1~8 1: not defined */
#define MM_FOCAL_LENGTH			450
#define MM_FOCAL_LENGTH_35MMFILM	0	/*unknown */
#define MM_GAIN_CONTROL			0	/* 0~4 0 none */
#define MM_FILE_SOURCE			3	/* 3: DSC */
#define MM_SCENE_TYPE			1	/* 1 DSC : a directly photographed image */
#define MM_EXPOSURE_MODE		0	/*0~2 0 : auto exposure */
#define MM_VALUE_NORMAL			0	/* 1 DSC : a directly photographed image */
#define MM_VALUE_LOW			1	/* 1 DSC : a directly photographed image */
#define MM_VALUE_HARD			2	/* 1 DSC : a directly photographed image */
#define	MM_SUBJECT_DISTANCE_RANGE	0	/* 0~3. 0 : unknown */
#define INVALID_GPS_VALUE		1000

#endif /* __MM_EXIFDEF_H__ */
