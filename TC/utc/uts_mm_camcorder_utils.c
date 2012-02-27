/*
 * libmm-camcorder TC
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

#include "uts_mm_camcorder_utils.h"

#ifndef __arm__
#include <gst/gst.h>
#endif
#include<stdio.h>

#include <mm_sound.h>
GMainLoop *gGMainLoop = NULL;
camcorder_callback_parameters *gCamcorderCallbackParameters = NULL;
camcorder_callback_parameters *gCamcorderCallbackParametersArray[STRESS];
int overlay = 1;
	
void (*tet_startup)() = Startup;
void (*tet_cleanup)() = Cleanup;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Startup()
{
	gGMainLoop = g_main_loop_new(NULL, FALSE);
	if(!gGMainLoop) {
		tet_printf("\n Failed in startup function while initializing GMainLoop \n");
		return;     
	}
#ifndef __arm__
	gst_init(NULL, NULL);
#endif
	tet_printf("\n Initialization[Startup function] was completed \n");
	return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Cleanup()
{
	if (!gGMainLoop) {
		return;	
	}
	g_main_loop_unref(gGMainLoop);
/*	if(gGMainLoop->context) {       \\Kishor :: This is to check whether g_main_loop_unref was success or not
		tet_printf("\n Failed in cleanup function while cleaning GMainLoop \n");
		return;     
	}
*/	tet_printf("\n Deinitialization[Cleanup function] was completed \n");	
	return;	
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
gboolean CreateCamcorder(MMHandleType* camcorder, MMCamPreset* pInfo)
{
	tet_printf("\n ======================Creating Camcorder====================== \n");
	gint errorValue = -1; 

	errorValue = mm_camcorder_create(camcorder, pInfo);
	if (MM_ERROR_NONE != errorValue)  {
		tet_printf("\n Failed in CreateCamcorder while creating the camcorder with error %x \n", errorValue);
		return false;
	}

	if(gCamcorderCallbackParameters) {
		free(gCamcorderCallbackParameters);
	}

	gCamcorderCallbackParameters = (camcorder_callback_parameters *)malloc(sizeof(camcorder_callback_parameters));
	if(!gCamcorderCallbackParameters) {
		tet_printf("\n Failed while allocating memory to gCamcorderCallbackParameters \n");
		return false;
	}

	gCamcorderCallbackParameters->camcorder = *camcorder;
	gCamcorderCallbackParameters->mode = IMAGE_MODE;
	gCamcorderCallbackParameters->isMultishot = false;
	gCamcorderCallbackParameters->stillshot_count = 0;
	gCamcorderCallbackParameters->multishot_count = 0;
	gCamcorderCallbackParameters->stillshot_filename = NULL;
	gCamcorderCallbackParameters->multishot_filename = NULL;
	gCamcorderCallbackParameters->stillshot_filename = (char *)malloc(25*sizeof(char));
	memset(gCamcorderCallbackParameters->stillshot_filename, 0, (25*sizeof(char)));
	if(gCamcorderCallbackParameters->stillshot_filename!=NULL) {
		gCamcorderCallbackParameters->stillshot_filename = "stillshot_file";
	}		
	gCamcorderCallbackParameters->multishot_filename = (char *)malloc(25*sizeof(char));
	if(!(gCamcorderCallbackParameters->multishot_filename)) {
		gCamcorderCallbackParameters->multishot_filename = "multishot_file";
	}
	/*
	errorValue = mm_camcorder_set_filename_callback(gCamcorderCallbackParameters->camcorder, FilenameCallbackFunctionCalledByCamcorder, gCamcorderCallbackParameters);
	if (MM_ERROR_NONE != errorValue) {
		tet_printf("\n Failed in CreateCamcorder while setting filename callback function in the camcorder with error %x \n", errorValue);
		return false;
	}
	*/

	errorValue = mm_camcorder_set_message_callback(gCamcorderCallbackParameters->camcorder, MessageCallbackFunctionCalledByCamcorder, gCamcorderCallbackParameters);
	if (MM_ERROR_NONE != errorValue) {
		tet_printf("\n Failed in CreateCamcorder while setting message callback function in the camcorder with error %x \n", errorValue);
		return false;
	}

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief This destroys the camcorder.
 * @param [in] camcorder
 * @code
	errorValue = mm_camcorder_destroy(camcorder);
 * @endcode
 * @param [out] None
 * @return true if successful, otherwise false.
 */
gboolean DestroyCamcorder(MMHandleType camcorder)
{
	tet_printf("\n ======================Destroying Camcorder====================== \n");

	gint errorValue = -1; 

	errorValue = mm_camcorder_destroy(camcorder);
	if (MM_ERROR_NONE != errorValue) {
		tet_printf("\n Failed in DestroyCamcorder while destroying the camcorder with error %x \n", errorValue);
		return false;
	}

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
gboolean StartCamcorder(MMHandleType* camcorder, MMCamPreset* pInfo,gint mode)
{
	tet_printf("\n ======================Starting Camcorder====================== \n");

	gint errorValue = -1; 
	gboolean bReturnValue = false;

	bReturnValue = CreateCamcorder(camcorder, pInfo);
	if (false == bReturnValue) {
		tet_printf("\n Failed in StartCamcorder while creating the camcorder \n");
		return false;
	}

	bReturnValue = SetDefaultAttributes(*camcorder,mode);
	if (true != bReturnValue) {
		tet_printf("\n Failed in StartCamcorder while setting default attributes to the camcorder \n");
		return false;
	}

	errorValue = mm_camcorder_realize(*camcorder);
	if (MM_ERROR_NONE != errorValue) {
		tet_printf("\n Failed in StartCamcorder while realizing the camcorder with error %x but mm_camcorder_start API was passed \n", errorValue);
		return false;
	}

	errorValue = mm_camcorder_start(*camcorder);
	if (MM_ERROR_NONE != errorValue) {
		tet_printf("\n Failed in StartCamcorder while starting the camcorder with error %x but mm_camcorder_create API was passed \n", errorValue);
		return false;
	}

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief This stops and destroys the camcorder.
 * @param [in] camcorder
 * @code
	errorValue = mm_camcorder_stop(camcorder);
	errorValue = mm_camcorder_unrealize(camcorder);
	bReturnValue = DestroyCamcorder(camcorder);
 * @endcode
 * @return true if successful, otherwise false.
 */
gboolean StopCamcorder(MMHandleType camcorder)
{
	tet_printf("\n ======================Stoping Camcorder====================== \n");

	gint errorValue = -1;
	gboolean bReturnValue = false;

	errorValue = mm_camcorder_stop(camcorder);
	if (MM_ERROR_NONE != errorValue) {
		tet_printf("\n Failed in StopCamcorder while stoping the camcorder with error %x \n", errorValue);
		return false;
	}

	errorValue = mm_camcorder_unrealize(camcorder);
	if (MM_ERROR_NONE != errorValue) {
		tet_printf("\n Failed in StopCamcorder while unrealizing the camcorder with error %x \n", errorValue);
		return false;
	}

	bReturnValue = DestroyCamcorder(camcorder);
	if (true != bReturnValue) {
		tet_printf("\n Failed in StopCamcorder while destroying the camcorder \n");
		return false;
	}

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
gboolean StartCamcorderforNInstances(MMHandleType camcorderInstances[], gint noOfInstances)
{
	tet_printf("\n ======================StartCamcorderforNInstances====================== \n");

	gint errorValue = -1; 
	gboolean bReturnValue = false;
	gint outerLoopVariable = 0;
	gint mode = IMAGE_MODE;
	MMCamPreset info;

	info.videodev_type = MM_VIDEO_DEVICE_CAMERA0;

	for(outerLoopVariable = 0; outerLoopVariable < noOfInstances ; outerLoopVariable++) {
		camcorderInstances[outerLoopVariable] = 0;

		errorValue = mm_camcorder_create((MMHandleType*)camcorderInstances[outerLoopVariable], &info);
		if (MM_ERROR_NONE != errorValue) {
       		tet_printf("\n Failed in StartCamcorderforNInstances while creating the camcorder with error %x (%d time) \n", errorValue, outerLoopVariable);
			return false;;
		}

		if (!camcorderInstances[outerLoopVariable]) {
       		tet_printf("\n Failed in StartCamcorderforNInstances while accessing the camcorder with error %x (%d time) \n", errorValue, outerLoopVariable);
			return false;;
		}

		mode = outerLoopVariable%3;	
		gCamcorderCallbackParametersArray[outerLoopVariable] = NULL;
		gCamcorderCallbackParametersArray[outerLoopVariable] = (camcorder_callback_parameters *)malloc(sizeof(camcorder_callback_parameters));
		if(!gCamcorderCallbackParametersArray[outerLoopVariable]) {
			tet_printf("\n Failed while allocating memory to gCamcorderCallbackParameters \n");
			return false;
		}

		gCamcorderCallbackParametersArray[outerLoopVariable]->camcorder = camcorderInstances[outerLoopVariable];
		gCamcorderCallbackParametersArray[outerLoopVariable]->mode = mode;
		gCamcorderCallbackParametersArray[outerLoopVariable]->isMultishot = false;
		gCamcorderCallbackParametersArray[outerLoopVariable]->stillshot_count = 0;
		gCamcorderCallbackParametersArray[outerLoopVariable]->multishot_count = 0;
		gCamcorderCallbackParametersArray[outerLoopVariable]->stillshot_filename = NULL;
		gCamcorderCallbackParametersArray[outerLoopVariable]->multishot_filename = NULL;
		gCamcorderCallbackParametersArray[outerLoopVariable]->stillshot_filename = (char *)malloc(25*sizeof(char));
		if(!(gCamcorderCallbackParametersArray[outerLoopVariable]->stillshot_filename)) {
			gCamcorderCallbackParametersArray[outerLoopVariable]->stillshot_filename = "stillshot_file";
		}		

		gCamcorderCallbackParametersArray[outerLoopVariable]->multishot_filename = (char *)malloc(25*sizeof(char));
		if(!(gCamcorderCallbackParametersArray[outerLoopVariable]->multishot_filename)) {
			gCamcorderCallbackParametersArray[outerLoopVariable]->multishot_filename = "multishot_file";
		}
		/*
		errorValue = mm_camcorder_set_filename_callback(camcorderInstances[outerLoopVariable], FilenameCallbackFunctionCalledByCamcorder, gCamcorderCallbackParametersArray[outerLoopVariable]);
		if (MM_ERROR_NONE != errorValue) {
			tet_printf("\n Failed in StartCamcorderforNInstances while setting filename callback function in the camcorder with error %d \n", errorValue);
			return false;
		}
		*/

		errorValue = mm_camcorder_set_message_callback(camcorderInstances[outerLoopVariable], MessageCallbackFunctionCalledByCamcorder, gCamcorderCallbackParametersArray[outerLoopVariable]);
		if (MM_ERROR_NONE != errorValue) {
			tet_printf("\n Failed in StartCamcorderforNInstances while setting message callback function in the camcorder with error %x \n", errorValue);
			return false;
		}

		bReturnValue = SetDefaultAttributes(camcorderInstances[outerLoopVariable], mode);
		if (true != bReturnValue) {
			tet_printf("\n Failed in StartCamcorderforNInstances while setting default attributes to the camcorder with mode %d (%d time) \n",mode, outerLoopVariable);
			return false;
		}

		errorValue = mm_camcorder_realize(camcorderInstances[outerLoopVariable]);
		if (MM_ERROR_NONE != errorValue) {
			tet_printf("\n Failed in StartCamcorderforNInstances while realizing the camcorder with error %x (%d time) \n", errorValue, outerLoopVariable);
			return false;
		}

		errorValue = mm_camcorder_start(camcorderInstances[outerLoopVariable]);
		if (MM_ERROR_NONE != errorValue) {
			tet_printf("\n Failed in StartCamcorderforNInstances while starting the camcorder with error %x (%d time) \n", errorValue, outerLoopVariable);
			return false;
		}
	}

	tet_printf("\n In StartCamcorderforNInstances: All instances were started \n");

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
gboolean StopCamcorderforNInstances(MMHandleType camcorderInstances[], gint noOfInstances)
{
	tet_printf("\n ======================StopCamcorderforNInstances====================== \n");

	gint errorValue = -1; 
	gint failedInstances = 0;
	gint outerLoopVariable = 0;

	for(outerLoopVariable = 0; outerLoopVariable < noOfInstances ; outerLoopVariable++) {
		errorValue = mm_camcorder_stop(camcorderInstances[outerLoopVariable]);
		if (MM_ERROR_NONE != errorValue) {
			tet_printf("\n Failed in StopCamcorderforNInstances while stoping the camcorder with error %x (%d time) \n", errorValue, outerLoopVariable);
			failedInstances++;
			break;
		}

		errorValue = mm_camcorder_unrealize(camcorderInstances[outerLoopVariable]);
		if (MM_ERROR_NONE != errorValue) {
			tet_printf("\n Failed in StopCamcorderforNInstances while unrealizing the camcorder with error %x (%d time) \n", errorValue, outerLoopVariable);
			failedInstances++;
			break;
		}

		errorValue = mm_camcorder_destroy(camcorderInstances[outerLoopVariable]);
		if ((MM_ERROR_NONE != errorValue) || (camcorderInstances[outerLoopVariable])) {
			tet_printf("\n Failed in StopCamcorderforNInstances while destroying the camcorder with error %x (%d time) but mm_camcorder_create API was passed \n", errorValue, outerLoopVariable);
			failedInstances++;
			break;
		}
	}

	if(0 != failedInstances) {
		tet_printf("\n Failed in StopCamcorderforNInstances, Number of Instances failed %d \n", failedInstances);
		return false;
	}

	tet_printf("\n In StopCamcorderforNInstances: All instances were stopped \n");

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
gboolean SetDefaultAttributes(MMHandleType camcorder, gint mode)
{
	tet_printf("\n ======================Setting Default Attributes====================== \n");

	if(0 == camcorder) {
		tet_printf(" \n Failed in SetDefaultAttributes: Camcorder not yet created \n");
		return false;
	}

	//MMHandleType attrsHandle =0;
	char* err_attr_name = (char*)malloc(50);
	int errorValue = -1;

	/*================================================================================
		Image mode
	*=================================================================================*/
	if (IMAGE_MODE == mode)
	{
		errorValue = mm_camcorder_set_attributes(camcorder, &err_attr_name,
						MMCAM_MODE, MM_CAMCORDER_MODE_IMAGE,
						MMCAM_CAMERA_WIDTH, 320,
						MMCAM_CAMERA_HEIGHT, 240,
						MMCAM_CAMERA_FORMAT, MM_PIXEL_FORMAT_YUYV,
						MMCAM_IMAGE_ENCODER, MM_IMAGE_CODEC_JPEG,
						MMCAM_IMAGE_ENCODER_QUALITY, IMAGE_ENC_QUALITY,
						MMCAM_DISPLAY_DEVICE, MM_DISPLAY_DEVICE_MAINLCD,
						MMCAM_DISPLAY_SURFACE, MM_DISPLAY_SURFACE_X,
						MMCAM_DISPLAY_ROTATION, MM_DISPLAY_ROTATION_270,
						MMCAM_CAPTURE_WIDTH, SRC_W_320,
						MMCAM_CAPTURE_HEIGHT, SRC_H_240,
						MMCAM_CAPTURE_COUNT, IMAGE_CAPTURE_COUNT_STILL,
						NULL );

		if (errorValue != MM_ERROR_NONE) {
			tet_printf("\n Failed in mm_camcorder_set_attributes in image mode. attr_name[%s]\n", err_attr_name);
			free( err_attr_name );
			err_attr_name = NULL;
			return false;
		}
	}
	/*================================================================================
		video mode
	*=================================================================================*/
	else if (VIDEO_MODE == mode)
	{
		errorValue = mm_camcorder_set_attributes( camcorder, &err_attr_name,
							MMCAM_CAMERA_WIDTH, SRC_W_320,
							MMCAM_CAMERA_HEIGHT, SRC_H_240,
							MMCAM_CAMERA_FORMAT, MM_PIXEL_FORMAT_NV12,
							MMCAM_CAMERA_FPS, SRC_VIDEO_FRAME_RATE_30,
							MMCAM_MODE, MM_CAMCORDER_MODE_VIDEO,
							MMCAM_AUDIO_DEVICE, MM_AUDIO_DEVICE_MIC,
							MMCAM_AUDIO_ENCODER, MM_AUDIO_CODEC_AMR,
							MMCAM_VIDEO_ENCODER, MM_VIDEO_CODEC_H264,
							MMCAM_FILE_FORMAT, MM_FILE_FORMAT_3GP,
							MMCAM_AUDIO_SAMPLERATE, AUDIO_SOURCE_SAMPLERATE,
							MMCAM_AUDIO_FORMAT, AUDIO_SOURCE_FORMAT,
							MMCAM_AUDIO_CHANNEL, AUDIO_SOURCE_CHANNEL,
							"audio-input-route", MM_AUDIOROUTE_CAPTURE_NORMAL,
							"audio-encoder-bitrate", AUDIO_SOURCE_SAMPLERATE,
							MMCAM_DISPLAY_DEVICE, MM_DISPLAY_DEVICE_MAINLCD,
							MMCAM_DISPLAY_SURFACE, MM_DISPLAY_SURFACE_X,
							MMCAM_TARGET_FILENAME, TARGET_FILENAME, TARGET_FILENAME_LENGTH,
							//MM_CAMCORDER_DISPLAY_ROTATION, DISPLAY_ROTATION,
							NULL );

		tet_printf("\n Setting MM_CAMCORDER_DISPLAY_HANDLE \n");

		if (errorValue != MM_ERROR_NONE) {
			tet_printf( "\n Failed in mm_camcorder_set_attributes in video mode. attr_name[%s]\n", err_attr_name );
			free( err_attr_name );
			err_attr_name = NULL;
			return false;
		}
	}
	/*================================================================================
		Audio mode
	*=================================================================================*/
	else if (AUDIO_MODE == mode)
	{
		errorValue = mm_camcorder_set_attributes( camcorder, &err_attr_name, MMCAM_MODE, MM_CAMCORDER_MODE_AUDIO,
								MMCAM_AUDIO_DEVICE, MM_AUDIO_DEVICE_MIC,
								MMCAM_AUDIO_ENCODER, MM_AUDIO_CODEC_AMR,
								MMCAM_FILE_FORMAT, MM_FILE_FORMAT_3GP,
								MMCAM_AUDIO_SAMPLERATE, AUDIO_SOURCE_SAMPLERATE,
								MMCAM_AUDIO_FORMAT, AUDIO_SOURCE_FORMAT,
								MMCAM_AUDIO_CHANNEL, AUDIO_SOURCE_CHANNEL,
								MMCAM_AUDIO_VOLUME, AUDIO_SOURCE_VOLUME,
								MMCAM_AUDIO_ENCODER_BITRATE, MM_CAMCORDER_MR59,
								MMCAM_TARGET_FILENAME, TARGET_FILENAME, TARGET_FILENAME_LENGTH,
								NULL );

		if (errorValue != MM_ERROR_NONE) {
			tet_printf( "\n Failed in mm_camcorder_set_attributes in audio mode. attr_name[%s]\n", err_attr_name );
			free( err_attr_name );
			err_attr_name = NULL;
			return false;
		}
	}

	free(err_attr_name);
	err_attr_name = NULL;

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
gboolean GetDefaultAttributes(MMHandleType camcorder, gint mode)
{
	tet_printf("\n ======================Getting Default Attributes====================== \n");

	if(0 == camcorder) {
		tet_printf(" \n Failed in GetDefaultAttributes: Camcorder not yet created \n");
		return false;
	}

	//MMHandleType attrsHandle = 0;
	int attrsValue = -1;
	char *stringAttrsValue = TARGET_FILENAME;
	int filenameLength = TARGET_FILENAME_LENGTH;
	int errorValue = -1;
	char* err_attr_name = (char*)malloc(50);

	/*================================================================================
		Image mode
	*=================================================================================*/
	if (IMAGE_MODE == mode)
	{
		/* profile attribute */
		errorValue = mm_camcorder_get_attributes(camcorder, &err_attr_name, MMCAM_MODE, &attrsValue, NULL);
		if ((errorValue < 0) || (attrsValue != MM_CAMCORDER_MODE_IMAGE)) {
			tet_printf("\n Failed in GetDefaultAttributes in image mode while getting profile mode attribute  \n");
			return false;
		}

		errorValue = mm_camcorder_get_attributes(camcorder, &err_attr_name, MMCAM_CAMERA_DEVICE, &attrsValue, NULL);
		if ((errorValue < 0) || (attrsValue != MM_VIDEO_DEVICE_CAMERA0)) {
			tet_printf("\n Failed in GetDefaultAttributes in image mode while getting profile video device attribute  \n");
			return false;
		}

		errorValue = mm_camcorder_get_attributes(camcorder, &err_attr_name, MMCAM_IMAGE_ENCODER, &attrsValue, NULL);
		if ((errorValue < 0) || (attrsValue != MM_IMAGE_CODEC_JPEG)) {
			tet_printf("\n Failed in GetDefaultAttributes in image mode while getting profile image codec attribute  \n");
			return false;
		}
		/* image encoder attribute */
		errorValue = mm_camcorder_get_attributes(camcorder, &err_attr_name, MMCAM_IMAGE_ENCODER_QUALITY, &attrsValue, NULL);
		if ((errorValue < 0) || (attrsValue != IMAGE_ENC_QUALITY)) {
			tet_printf("\n Failed in GetDefaultAttributes in image mode while getting image encoder quality attribute  \n");
			return false;
		}

		/* display attribute */
		errorValue = mm_camcorder_get_attributes(camcorder, &err_attr_name, MMCAM_DISPLAY_SURFACE, &attrsValue, NULL);
		if ((errorValue < 0) || (attrsValue != DISPLAY_ID)) {
			tet_printf("\n Failed in GetDefaultAttributes in image mode while getting display ID attribute  \n");
			return false;
		}

		errorValue = mm_camcorder_get_attributes(camcorder, &err_attr_name, MMCAM_DISPLAY_ROTATION, &attrsValue, NULL);
		if ((errorValue < 0) || (attrsValue != DISPLAY_ROTATION)) {
			tet_printf("\n Failed in GetDefaultAttributes in image mode while getting display rotation attribute  \n");
			return false;
		}

		/* capture attribute */
		errorValue = mm_camcorder_get_attributes(camcorder, &err_attr_name, MMCAM_CAPTURE_WIDTH, &attrsValue, NULL);
		if ((errorValue < 0) || (attrsValue != SRC_W_320)) {
			tet_printf("\n Failed in GetDefaultAttributes in image mode while getting capture width attribute  \n");
			return false;
		}

		errorValue = mm_camcorder_get_attributes(camcorder, &err_attr_name, MMCAM_CAPTURE_HEIGHT, &attrsValue, NULL);
		if ((errorValue < 0) || (attrsValue != SRC_H_240)) {
			tet_printf("\n Failed in GetDefaultAttributes in image mode while getting capture height attribute  \n");
			return false;
		}

		errorValue = mm_camcorder_get_attributes(camcorder, &err_attr_name, MMCAM_CAPTURE_COUNT, &attrsValue, NULL);
		if ((errorValue < 0) || (attrsValue != IMAGE_CAPTURE_COUNT_STILL)) {
			tet_printf("\n Failed in GetDefaultAttributes in image mode while getting capture count attribute  \n");
			return false;
		}
	}
	/*================================================================================
		video mode
	*=================================================================================*/
	else if (VIDEO_MODE == mode)
	{
		/* video source attribute */
		/*
		errorValue = mm_camcorder_get_attributes(camcorder, MM_CAMCORDER_ATTR_VIDEO_SOURCE, &attrsHandle);
		if(MM_ERROR_NONE != errorValue) {
       		tet_printf("\n Failed in GetDefaultAttributes while getting the camcorder video mode video source attributes  \n");
			return false;
       	}
       	*/

		errorValue = mm_camcorder_get_attributes(camcorder, &err_attr_name, MMCAM_CAMERA_WIDTH, &attrsValue, NULL);
		if ((errorValue < 0) || (attrsValue != SRC_W_320)) {
			tet_printf("\n Failed in GetDefaultAttributes in video mode while getting video source width attribute  \n");
			return false;
		}

		errorValue = mm_camcorder_get_attributes(camcorder, &err_attr_name, MMCAM_CAMERA_HEIGHT, &attrsValue, NULL);
		if ((errorValue < 0) || (attrsValue != SRC_H_240)) {
			tet_printf("\n Failed in GetDefaultAttributes in video mode while getting video source height attribute  \n");
			return false;
		}

		errorValue = mm_camcorder_get_attributes(camcorder, &err_attr_name, MMCAM_CAMERA_FPS, &attrsValue, NULL);
		if ((errorValue < 0) || (attrsValue != SRC_VIDEO_FRAME_RATE_30)) {
			tet_printf("\n Failed in GetDefaultAttributes in video mode while getting video source frame rate attribute  \n");
			return false;
		}
		/* profile attribute */
		errorValue = mm_camcorder_get_attributes(camcorder, &err_attr_name, MMCAM_MODE, &attrsValue, NULL);
		if ((errorValue < 0) || (attrsValue != MM_CAMCORDER_MODE_VIDEO)) {
			tet_printf("\n Failed in GetDefaultAttributes in video mode while getting video mode attribute  \n");
			return false;
		}

		errorValue = mm_camcorder_get_attributes(camcorder, &err_attr_name, MMCAM_AUDIO_DEVICE, &attrsValue, NULL);
		if ((errorValue < 0) || (attrsValue != MM_AUDIO_DEVICE_MIC)) {
			tet_printf("\n Failed in GetDefaultAttributes in video mode while getting audio device attribute  \n");
			return false;
		}

		errorValue = mm_camcorder_get_attributes(camcorder, &err_attr_name, MMCAM_CAMERA_DEVICE, &attrsValue, NULL);
		if ((errorValue < 0) || (attrsValue != MM_VIDEO_DEVICE_CAMERA0)) {
			tet_printf("\n Failed in GetDefaultAttributes in video mode while getting video device attribute  \n");
			return false;
		}

		errorValue = mm_camcorder_get_attributes(camcorder, &err_attr_name, MMCAM_AUDIO_ENCODER, &attrsValue, NULL);
		if ((errorValue < 0) || (attrsValue != MM_AUDIO_CODEC_AMR)) {
			tet_printf("\n Failed in GetDefaultAttributes in video mode while getting audio codec attribute  \n");
			return false;
		}

		errorValue = mm_camcorder_get_attributes(camcorder, &err_attr_name, MMCAM_VIDEO_ENCODER, &attrsValue, NULL);
		if ((errorValue < 0) || (attrsValue != MM_VIDEO_CODEC_MPEG4)) {
			tet_printf("\n Failed in GetDefaultAttributes in video mode while getting video codec attribute  \n");
			return false;
		}

		errorValue = mm_camcorder_get_attributes(camcorder, &err_attr_name, MMCAM_FILE_FORMAT, &attrsValue, NULL);
		if ((errorValue < 0) || (attrsValue != MM_FILE_FORMAT_3GP)) {
			tet_printf("\n Failed in GetDefaultAttributes in video mode while getting video file format attribute  \n");
			return false;
		}

		/* audio source attribute setting */
		errorValue = mm_camcorder_get_attributes(camcorder, &err_attr_name, MMCAM_AUDIO_SAMPLERATE, &attrsValue, NULL);
		if ((errorValue < 0) || (attrsValue != AUDIO_SOURCE_SAMPLERATE)) {
			tet_printf("\n Failed in GetDefaultAttributes in video mode while getting audio sample rate attribute  \n");
			return false;
		}

		errorValue = mm_camcorder_get_attributes(camcorder, &err_attr_name, MMCAM_AUDIO_FORMAT, &attrsValue, NULL);
		if ((errorValue < 0) || (attrsValue != AUDIO_SOURCE_FORMAT)) {
			tet_printf("\n Failed in GetDefaultAttributes in video mode while getting audio source format attribute  \n");
			return false;
		}

		errorValue = mm_camcorder_get_attributes(camcorder, &err_attr_name, MMCAM_AUDIO_CHANNEL, &attrsValue, NULL);
		if ((errorValue < 0) || (attrsValue != AUDIO_SOURCE_CHANNEL)) {
			tet_printf("\n Failed in GetDefaultAttributes in video mode while getting audio source channel attribute  \n");
			return false;
		}
		/* display attribute */

		errorValue = mm_camcorder_get_attributes(camcorder, &err_attr_name, MMCAM_DISPLAY_SURFACE, &attrsValue, NULL);
		if ((errorValue < 0) || (attrsValue != DISPLAY_ID)) {
			tet_printf("\n Failed in GetDefaultAttributes in video mode while getting display ID attribute  \n");
			return false;
		}

		/* capture attribute */
		errorValue = mm_camcorder_get_attributes(camcorder, &err_attr_name, MMCAM_CAPTURE_WIDTH, &attrsValue, NULL);
		if ((errorValue < 0) || (attrsValue != SRC_W_320)) {
			tet_printf("\n Failed in GetDefaultAttributes in video mode while getting capture width attribute  \n");
			return false;
		}

		errorValue = mm_camcorder_get_attributes(camcorder, &err_attr_name, MMCAM_CAPTURE_HEIGHT, &attrsValue, NULL);
		if ((errorValue < 0) || (attrsValue != SRC_H_240)) {
			tet_printf("\n Failed in GetDefaultAttributes in video mode while getting capture height attribute  \n");
			return false;
		}
		/* target attribute setting */
		errorValue = mm_camcorder_get_attributes(camcorder, &err_attr_name, MMCAM_TARGET_FILENAME, &stringAttrsValue, &filenameLength, NULL);
		if ((errorValue < 0) || (stringAttrsValue != NULL)) { //Kishor: string comparison required between stringAttrsValue and TARGET_FILENAME
			tet_printf("\n Failed in GetDefaultAttributes in video mode while getting target filename attribute  \n");
			return false;
		}
	}
	/*================================================================================
		Audio mode
	*=================================================================================*/
	else if (AUDIO_MODE == mode)
	{
		/* profile attribute */
		errorValue = mm_camcorder_get_attributes(camcorder, &err_attr_name, MMCAM_MODE, &attrsValue, NULL);
		if ((errorValue < 0) || (attrsValue != MM_CAMCORDER_MODE_AUDIO)) {
			tet_printf("\n Failed in GetDefaultAttributes in audio mode while getting audio mode attribute  \n");
			return false;
		}

		errorValue = mm_camcorder_get_attributes(camcorder, &err_attr_name, MMCAM_AUDIO_DEVICE, &attrsValue, NULL);
		if ((errorValue < 0) || (attrsValue != MM_AUDIO_DEVICE_MIC)) {
			tet_printf("\n Failed in GetDefaultAttributes in audio mode while getting audio device attribute  \n");
			return false;
		}

		errorValue = mm_camcorder_get_attributes(camcorder, &err_attr_name, MMCAM_AUDIO_ENCODER, &attrsValue, NULL);
		if ((errorValue < 0) || (attrsValue != MM_AUDIO_CODEC_AMR)) {
			tet_printf("\n Failed in GetDefaultAttributes in audio mode while getting audio codec attribute  \n");
			return false;
		}

		errorValue = mm_camcorder_get_attributes(camcorder, &err_attr_name, MMCAM_FILE_FORMAT, &attrsValue, NULL);
		if ((errorValue < 0) || (attrsValue != MM_FILE_FORMAT_3GP)) {
			tet_printf("\n Failed in GetDefaultAttributes in audio mode while getting audio file format attribute  \n");
			return false;
		}

		/* audio source attribute */
		errorValue = mm_camcorder_get_attributes(camcorder, &err_attr_name, MMCAM_AUDIO_SAMPLERATE, &attrsValue, NULL);
		if ((errorValue < 0) || (attrsValue != AUDIO_SOURCE_SAMPLERATE)) {
			tet_printf("\n Failed in GetDefaultAttributes in audio mode while getting audio sample rate attribute  \n");
			return false;
		}

		errorValue = mm_camcorder_get_attributes(camcorder, &err_attr_name, MMCAM_AUDIO_FORMAT, &attrsValue, NULL);
		if ((errorValue < 0) || (attrsValue != AUDIO_SOURCE_FORMAT)) {
			tet_printf("\n Failed in GetDefaultAttributes in audio mode while getting audio source format attribute  \n");
			return false;
		}

		errorValue = mm_camcorder_get_attributes(camcorder, &err_attr_name, MMCAM_AUDIO_CHANNEL, &attrsValue, NULL);
		if ((errorValue < 0) || (attrsValue != AUDIO_SOURCE_CHANNEL)) {
			tet_printf("\n Failed in GetDefaultAttributes in audio mode while getting audio source channel attribute  \n");
			return false;
		}

		errorValue = mm_camcorder_get_attributes(camcorder, &err_attr_name, MMCAM_AUDIO_VOLUME, &attrsValue, NULL);
		if ((errorValue < 0) || (attrsValue != AUDIO_SOURCE_VOLUME)) {
			tet_printf("\n Failed in GetDefaultAttributes in audio mode while getting audio encoder bitrate attribute  \n");
			return false;
		}
		/* audio encoder attribute */
		errorValue = mm_camcorder_get_attributes(camcorder, &err_attr_name, MMCAM_VIDEO_ENCODER_BITRATE, &attrsValue, NULL);
		if ((errorValue < 0) || (attrsValue != MM_CAMCORDER_MR59)) {
			tet_printf("\n Failed in GetDefaultAttributes in audio mode while getting audio sample rate attribute  \n");
			return false;
		}
		/* target attribute */
		errorValue = mm_camcorder_get_attributes(camcorder, &err_attr_name, MMCAM_TARGET_FILENAME, &stringAttrsValue, &filenameLength, NULL);
		if ((errorValue < 0) || (stringAttrsValue != NULL)) { //Kishor: string comparison required between stringAttrsValue and TARGET_FILENAME
			tet_printf("\n Failed in GetDefaultAttributes in audio mode while getting target filename attribute  \n");
			return false;
		}
	}

	free(err_attr_name);
	err_attr_name = NULL;

	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool FilenameCallbackFunctionCalledByCamcorder(int format, char *filename, void *user_param)
{
	CAMCORDER_RETURN_IF_FAIL(filename, FALSE);
	CAMCORDER_RETURN_IF_FAIL(user_param, FALSE);

	//MMHandleType attrsHandle;
	char *newFilename = NULL, *previousFilename = NULL;
	int errorValue;
	int filenameLength = TARGET_FILENAME_LENGTH;
	char* err_attr_name = (char*)malloc(50);

	camcorder_callback_parameters *userParamFromCamcorder = (camcorder_callback_parameters *)user_param;
	switch (format) {
	case MM_FILE_FORMAT_JPG:
		/* target attribute */
		/*
		errorValue = mm_camcorder_get_attributes(userParamFromCamcorder->camcorder, MM_CAMCORDER_ATTR_TARGET, &attrsHandle, NULL);
		if(errorValue != MM_ERROR_NONE) {
			tet_printf("\n Failed in CallbackFunctionCalledByCamcorder while getting attributes for the camcorder with JPG format \n");
			return FALSE;
		}
		*/

		errorValue = mm_camcorder_get_attributes(userParamFromCamcorder->camcorder, &err_attr_name, MMCAM_TARGET_FILENAME, &previousFilename, &filenameLength, NULL);
		newFilename = (char *)malloc(CAPTURE_FILENAME_LEN);
		if (!newFilename) {
			tet_printf("\n Failed in CallbackFunctionCalledByCamcorder while allocating memory for new file in the camcorder with JPG format \n");
			return FALSE;
		}
		memset(newFilename, '\0', CAPTURE_FILENAME_LEN); //Kishor :: needs to check the error
		if (userParamFromCamcorder->isMultishot) {
			sprintf(newFilename, "%s%03d.jpg", userParamFromCamcorder->multishot_filename, userParamFromCamcorder->multishot_count++);
		}
		else {
			sprintf(newFilename, "%s%03d.jpg", userParamFromCamcorder->stillshot_filename, userParamFromCamcorder->stillshot_count++);
		}
		errorValue = mm_camcorder_set_attributes(userParamFromCamcorder->camcorder, &err_attr_name, MMCAM_TARGET_FILENAME, newFilename, sizeof(newFilename), NULL);
		if(errorValue < 0) {
			tet_printf("\n Failed in CallbackFunctionCalledByCamcorder while setting the string attribute for the camcorder with JPG format \n");
			return FALSE;
		}

		//Kishor :: free previous filename space
		if (previousFilename) {
			free(previousFilename);
		}
		break;
	case MM_FILE_FORMAT_3GP:
		/* target attribute setting */
		errorValue = mm_camcorder_get_attributes(userParamFromCamcorder->camcorder, &err_attr_name, MMCAM_TARGET_FILENAME, &previousFilename, &filenameLength, NULL);
		newFilename = (char *)malloc(CAPTURE_FILENAME_LEN);
		if (!newFilename) {
			tet_printf("\n Failed in CallbackFunctionCalledByCamcorder while allocating memory for new file in the camcorder with 3GP format \n");
			return FALSE;
		}
		memset(newFilename, '\0', CAPTURE_FILENAME_LEN); //Kishor :: needs to check the error
		if (userParamFromCamcorder->isMultishot) {
			sprintf(newFilename, "%s%03d.3gp", userParamFromCamcorder->multishot_filename, userParamFromCamcorder->multishot_count++);
		}
		else {
			sprintf(newFilename, "%s%03d.3gp", userParamFromCamcorder->stillshot_filename, userParamFromCamcorder->stillshot_count++); 						//recorder team check
		}

		errorValue = mm_camcorder_set_attributes(userParamFromCamcorder->camcorder, &err_attr_name, MMCAM_TARGET_FILENAME, newFilename, sizeof(newFilename), NULL);
		if(errorValue < 0) {
			tet_printf("\n Failed in CallbackFunctionCalledByCamcorder while setting the string attribute for the camcorder with 3GP format \n");
			return FALSE;
		}

		//Kishor :: free previous filename space
		if (previousFilename) {
			free(previousFilename);
		}
		break;
	case MM_FILE_FORMAT_MP4:
		/* target attribute setting */
		errorValue = mm_camcorder_get_attributes(userParamFromCamcorder->camcorder, &err_attr_name, MMCAM_TARGET_FILENAME, &previousFilename, &filenameLength, NULL);
		newFilename = (char *)malloc(CAPTURE_FILENAME_LEN);
		if (!newFilename) {
			tet_printf("\n Failed in CallbackFunctionCalledByCamcorder while allocating memory for new file in the camcorder with MP4 format \n");
			return FALSE;
		}
		memset(newFilename, '\0', CAPTURE_FILENAME_LEN); //Kishor :: needs to check the error
		if (userParamFromCamcorder->isMultishot) {
			sprintf(newFilename, "%s%03d.mp4", userParamFromCamcorder->multishot_filename, userParamFromCamcorder->multishot_count++);
		}
		else {
			sprintf(newFilename, "%s%03d.mp4", userParamFromCamcorder->stillshot_filename, userParamFromCamcorder->stillshot_count++); 						//recorder team check
		}
		errorValue = mm_camcorder_set_attributes(userParamFromCamcorder->camcorder, &err_attr_name, MMCAM_TARGET_FILENAME, newFilename, sizeof(newFilename), NULL);
		if(errorValue < 0) {
			tet_printf("\n Failed in CallbackFunctionCalledByCamcorder while setting the string attribute for the camcorder with MP4 format \n");
			return FALSE;
		}

		//Kishor :: free previous filename space
		if (previousFilename) {
			free(previousFilename);
		}
		break;
	default:
		g_assert_not_reached();
		break;
	}	

	CAMCORDER_RETURN_IF_FAIL(newFilename, FALSE);
	strcpy(filename, newFilename);
	if (newFilename) {
		free(newFilename);
	}
	newFilename = NULL;

	return TRUE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int MessageCallbackFunctionCalledByCamcorder(int message, void *msg_param, void *user_param)
{
	CAMCORDER_RETURN_IF_FAIL(msg_param, FALSE);
	CAMCORDER_RETURN_IF_FAIL(user_param, FALSE);

	MMMessageParamType *messageFromCamcorder = (MMMessageParamType*) msg_param;
	//camcorder_callback_parameters *userParamFromCamcorder = (camcorder_callback_parameters *)user_param;
	int camcorderState = -1;
	//MMCamStillshotReport *stillshotReport = NULL;
	switch (message) {
		case MM_MESSAGE_CAMCORDER_ERROR:
			tet_printf("\n In MessageCallbackFunctionCalledByCamcorder with MM_MESSAGE_CAMCORDER_ERROR \n");
			break;
		case MM_MESSAGE_CAMCORDER_STATE_CHANGED:
			camcorderState = messageFromCamcorder->state.current;
			switch(camcorderState) {
				case MM_CAMCORDER_STATE_NULL:
					tet_printf("[CamcorderApp] Camcorder State is NULL \n");
					break;
				case MM_CAMCORDER_STATE_READY:
					tet_printf("[CamcorderApp] Camcorder State is READY \n");
					break;
				case MM_CAMCORDER_STATE_PREPARE:
					tet_printf("[CamcorderApp] Camcorder State is PREPARE \n");
					break;
				case MM_CAMCORDER_STATE_CAPTURING:
					tet_printf("[CamcorderApp] Camcorder State is CAPTURING \n");
					break;
				case MM_CAMCORDER_STATE_RECORDING:
					tet_printf("[CamcorderApp] Camcorder State is RECORDING \n");
					break;
				case MM_CAMCORDER_STATE_PAUSED:
					tet_printf("[CamcorderApp] Camcorder State is PAUSED \n");
					break;
				default:
					tet_printf("[CamcorderApp] Camcorder State is Unknown \n");
					break;
			}
			break;
		case MM_MESSAGE_CAMCORDER_CAPTURED:
			/*
			stillshotReport = (MMCamStillshotReport*)(messageFromCamcorder->data);
			if (!stillshotReport) {
				tet_printf("\n Failed in MessageCallbackFunctionCalledByCamcorder while retriving mmcam_stillshot_report \n");
				return FALSE;
			}
			if (userParamFromCamcorder->isMultishot) {
				tet_printf("[CamcorderApp] Camcorder Captured(filename=%s) \n", stillshotReport->stillshot_filename);
				if (stillshotReport->stillshot_no >= IMAGE_CAPTURE_COUNT_MULTI) {
					userParamFromCamcorder->isMultishot = FALSE;
				}
			}
			if (stillshotReport->stillshot_filename) {
				free(stillshotReport->stillshot_filename);
			}
			stillshotReport->stillshot_filename = NULL;
			if (stillshotReport) {
				free(stillshotReport);
			}
			stillshotReport = NULL;
			*/
			break;
		default:
			tet_printf("\n In MessageCallbackFunctionCalledByCamcorder with message as default \n");
			break;
	}
	return TRUE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool VideoStreamCallbackFunctionCalledByCamcorder(void *stream, int width, int height, void *user_param)
{
	tet_printf("\n In VideoStreamCallbackFunctionCalledByCamcorder \n");
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool VideoCaptureCallbackFunctionCalledByCamcorder(MMCamcorderCaptureDataType *frame, MMCamcorderCaptureDataType *thumbnail, void *user_param)
{
	tet_printf("\n In VideoCaptureCallbackFunctionCalledByCamcorder \n");
	return true;
}
