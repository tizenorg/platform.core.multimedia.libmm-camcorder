/*
 * mm_camcorder_samsplecode
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
 */

/**
 * @mm_camcorder_samplecode.c
 *
 * @description
 *	This file contains usage example of mm_camcorder API.
 *
 * @author	SoYeon Kang<soyeon.kang @samsung.com>
 * 			Wonhyung Cho<wh01.cho@samsung.com> 
 */

/* ===========================================================================================
EDIT HISTORY FOR MODULE
	This section contains comments describing changes made to the module.
	Notice that changes are listed in reverse chronological order.
when		who						what, where, why
---------	--------------------	----------------------------------------------------------
07/07/10	wh01.cho@samsung.com   		Created
*/


/*===========================================================================================
|																							|
|  INCLUDE FILES																			|
|  																							|
========================================================================================== */
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>

#include <mm_camcorder.h>

#include <appcore-efl.h>

#include <Elementary.h>
#include <Ecore_X.h> 

/*---------------------------------------------------------------------------
|    GLOBAL VARIABLE DEFINITIONS:											|
---------------------------------------------------------------------------*/

GIOChannel *stdin_channel;
int	g_current_state;

MMHandleType hcam = 0;
unsigned int elapsed_time = 0;
int stillshot_count = 0;

void * overlay = NULL;

struct appdata
{
	Evas *evas;
	Ecore_Evas *ee;
	Evas_Object *win_main;

	Evas_Object *layout_main; /* layout widget based on EDJ */
	Ecore_X_Window xid;

	/* add more variables here */
};

int r;
struct appdata ad;


/*---------------------------------------------------------------------------
|    LOCAL CONSTANT DEFINITIONS:											|
---------------------------------------------------------------------------*/
#define MAX_STRING_LEN				256 					// maximum length of string
#define TARGET_FILENAME					"/root/av.mp4"

/*---------------------------------------------------------------------------
|    LOCAL DATA TYPE DEFINITIONS:											|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    LOCAL VARIABLE DEFINITIONS:											|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    LOCAL FUNCTION PROTOTYPES:												|
---------------------------------------------------------------------------*/
/* Application Body */
int main(int argc, char **argv);
static gboolean cmd_input(GIOChannel *channel);
static void main_menu(gchar buf);
static gboolean mode_change();
static void menu();


/* Start and finish */
static gboolean msg_callback(int message, void *msg_param, void *user_param);
static gboolean initialize_image_capture();
static gboolean initialize_video_capture(); 	//A/V recording
static gboolean initialize_audio_capture(); 	//audio only recording
static gboolean uninitialize_camcorder();


/* Sample functions */
static gboolean capturing_picture();
static gboolean record_and_save_video_file();
static gboolean record_and_cancel_video_file();
static gboolean record_pause_and_resume_recording();
static gboolean get_state_of_camcorder();
static gboolean start_autofocus();
static gboolean filename_setting();
static gboolean set_video_stream_callback();


/* APPFWK functions*/
int app_init(void *data);
int app_exit(void *data);
int app_start(void *data);
int app_stop(void *data);
int idler_exit_cb(void *data);


/*---------------------------------------------------------------------------
|    LOCAL FUNCTION DEFINITIONS:											|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|           Sample Functions:												|
---------------------------------------------------------------------------*/
static int
camcordertest_video_capture_cb(MMCamcorderCaptureDataType *src, MMCamcorderCaptureDataType *thumb, void *preview)
{
	int nret = 0;
	char m_filename[MAX_STRING_LEN];
	FILE* fp=NULL;

	snprintf(m_filename, MAX_STRING_LEN, "./stillshot_%03d.jpg",  stillshot_count++);

	printf("filename : %s\n",  m_filename);

	fp=fopen(m_filename, "w+");
	if(fp==NULL)
	{
		printf("FileOPEN error!!\n");
		return FALSE;
	}
	else
	{
		printf("open success\n\n");
		if(fwrite(src->data, src->length, 1, fp )!=1)
		{
			printf("File write error!!\n");
			return FALSE;
		}
		printf("write success\n");
	}
	fclose(fp);
	printf("Capture done!\n");

	return TRUE;
}


static int
camcordertest_video_stream_cb(MMCamcorderVideoStreamDataType *stream, void *user_param)
{
	int nret = 0;

	printf("stream cb is called(%p, %d, %d)\n",  stream->data, stream->width, stream->height);

	return TRUE;
}


static gboolean msg_callback(int message, void *msg_param, void *user_param)
{
	MMHandleType hcamcorder = (MMHandleType)user_param;
	MMMessageParamType *param = (MMMessageParamType *) msg_param;
	int err = 0;
	
	switch (message) {
		case MM_MESSAGE_CAMCORDER_ERROR:
			printf("MM_MESSAGE_CAMCORDER_ERROR : code = %x\n", param->code);
			break;
		case MM_MESSAGE_CAMCORDER_STATE_CHANGED:
			g_current_state = param->state.current;
			break;

		case MM_MESSAGE_CAMCORDER_CAPTURED:
		{
			//Get mode of camcorder
			int mode = 0;
			err = mm_camcorder_get_attributes(hcamcorder, NULL,
											MMCAM_MODE,  &mode,
										       NULL);

			if (mode == MM_CAMCORDER_MODE_IMAGE)
			{
				printf("Stillshot Captured!!(number=%d)\n", param->code);	//If you start multi shot, 'param->code' will give you the order of the pictrues.

				err =  mm_camcorder_capture_stop(hcam);
				if (err < 0) 
				{
					printf("Fail to call mm_camcorder_capture_start  = %x\n", err);
					return FALSE;
				}
			}
			else
			{
				//Audio/Video recording
				MMCamRecordingReport* report ;

				if (param)
					report = (MMCamRecordingReport*)(param->data);
				else
					return FALSE;

				printf("Recording Complete(filename=%s)\n", report->recording_filename);

				//You have to release 'recording_filename' and 'MMCamRecordingReport' structure.
				if (report->recording_filename)
					free(report->recording_filename);

				if (report)
					free(report);
			}
		}
			break;
		case MM_MESSAGE_CAMCORDER_RECORDING_STATUS:
		{
			unsigned int elapsed;
			elapsed = param->recording_status.elapsed / 1000;
			if (elapsed_time != elapsed) {
				unsigned int temp_time;
				int hour, minute, second;
				elapsed_time = elapsed;
				temp_time = elapsed;
				hour = temp_time / 3600;
				temp_time = elapsed % 3600;
				minute = temp_time / 60;
				second = temp_time % 60;
				printf("Current Time - %d:%d:%d\n", hour, minute, second);
			}
		}
			break;			
		case MM_MESSAGE_CAMCORDER_MAX_SIZE:	
		{	
			printf("Reach Size limitation.\n");

			/* After reaching max size, Camcorder starts to drop all buffers that it receives.
			    You have to call mm_camcorder_commit() to finish recording. */
			err = mm_camcorder_commit(hcamcorder);

			if (err < 0) 
			{
				printf("Save recording mm_camcorder_commit  = %x\n", err);
			}
		}
			break;		
		case MM_MESSAGE_CAMCORDER_NO_FREE_SPACE:
		{
			printf("There is no space in storage.\n");

			/* If there is no free space to save recording frame, Camcorder starts to drop all buffers that it receives.
			    You have to call mm_camcorder_commit() to finish recording. */
			err = mm_camcorder_commit(hcamcorder);

			if (err < 0) 
			{
				printf("Save recording mm_camcorder_commit  = %x\n", err);
			}
		}
			break;
		case MM_MESSAGE_CAMCORDER_TIME_LIMIT:
		{
			printf("Reach time limitation.\n");

			/* After reaching time limit, Camcorder starts to drop all buffers that it receives.
			    You have to call mm_camcorder_commit() to finish recording. */
			err = mm_camcorder_commit(hcamcorder);

			if (err < 0) 
			{
				printf("Save recording mm_camcorder_commit  = %x\n", err);
			}
		}
			break;
		case MM_MESSAGE_CAMCORDER_FOCUS_CHANGED:
		{
			printf( "Focus State changed. State:[%d]\n", param->code );
		}
			break;
		default:
			break;
	}

	return TRUE;
}

static gboolean initialize_image_capture() 
{
	int err;
	MMCamPreset cam_info;
	char *err_attr_name = NULL;
	void * hdisplay = NULL;
	int hsize = 0;

	cam_info.videodev_type = MM_VIDEO_DEVICE_CAMERA0;
	/* If you want to turn front camera on, disable upper line and enable below one.*/
//	cam_info.videodev_type = MM_VIDEO_DEVICE_CAMERA1;

	err = mm_camcorder_create(&hcam, &cam_info);

	if (err != MM_ERROR_NONE) 
	{
		printf("Fail to call mm_camcorder_create = %x\n", err);
		return FALSE;
	}

	mm_camcorder_set_message_callback(hcam, (MMMessageCallback)msg_callback, (void*)hcam);
	mm_camcorder_set_video_capture_callback(hcam, (mm_camcorder_video_capture_callback)camcordertest_video_capture_cb, (void*)hcam);

	hdisplay = &ad.xid;
	hsize = sizeof(ad.xid);

	/* camcorder attribute setting */
	err = mm_camcorder_set_attributes((MMHandleType)hcam, &err_attr_name,
									MMCAM_MODE, MM_CAMCORDER_MODE_IMAGE,
									MMCAM_IMAGE_ENCODER, MM_IMAGE_CODEC_JPEG,
									MMCAM_CAMERA_WIDTH, 640,
									MMCAM_CAMERA_HEIGHT, 480,
									MMCAM_CAMERA_FORMAT, MM_PIXEL_FORMAT_YUYV,
									MMCAM_CAMERA_FPS, 30,
									MMCAM_DISPLAY_ROTATION, MM_DISPLAY_ROTATION_270,
									MMCAM_DISPLAY_HANDLE, (void*) hdisplay,          hsize,
									MMCAM_CAPTURE_FORMAT, MM_PIXEL_FORMAT_ENCODED,
									MMCAM_CAPTURE_WIDTH, 640,
									MMCAM_CAPTURE_HEIGHT, 480,
									NULL);

	if (err < 0) 
	{
		printf("Set attrs fail. (%s:%x)\n", err_attr_name, err);
		if (err_attr_name) {
			free(err_attr_name);
			err_attr_name = NULL;
			return FALSE;
		}
	}

	err =  mm_camcorder_realize(hcam);
	if (err < 0) 
	{
		printf("Fail to call mm_camcorder_realize  = %x\n", err);
		return FALSE;
	}

	/* start camcorder */
	err = mm_camcorder_start(hcam);
	if (err < 0) 
	{
		printf("Fail to call mm_camcorder_start  = %x\n", err);
		return FALSE;
	}

	return TRUE;
}


static gboolean initialize_video_capture() 	//A/V recording
{
	int err;
	MMCamPreset cam_info;
	char *err_attr_name = NULL;
	void * hdisplay = NULL;
	int hsize = 0;

	cam_info.videodev_type = MM_VIDEO_DEVICE_CAMERA0;
	/* If you want to turn front camera on, disable upper line and enable below one.*/
//	cam_info.videodev_type = MM_VIDEO_DEVICE_CAMERA1;

	err = mm_camcorder_create(&hcam, &cam_info);

	if (err != MM_ERROR_NONE) 
	{
		printf("Fail to call mm_camcorder_create = %x\n", err);
		return FALSE;
	}

	mm_camcorder_set_message_callback(hcam, (MMMessageCallback)msg_callback, hcam);

	hdisplay = &ad.xid;
	hsize = sizeof(ad.xid);

	/* camcorder attribute setting */
	err = mm_camcorder_set_attributes((MMHandleType)hcam, &err_attr_name,
					MMCAM_MODE, MM_CAMCORDER_MODE_VIDEO,
					MMCAM_AUDIO_DEVICE, MM_AUDIO_DEVICE_MIC,
					MMCAM_AUDIO_ENCODER, MM_AUDIO_CODEC_AAC,
					MMCAM_VIDEO_ENCODER, MM_VIDEO_CODEC_MPEG4,
					MMCAM_FILE_FORMAT, MM_FILE_FORMAT_3GP,
					MMCAM_CAMERA_WIDTH, 1280,
					MMCAM_CAMERA_HEIGHT, 720,
					MMCAM_CAMERA_FORMAT, MM_PIXEL_FORMAT_NV12,
					MMCAM_CAMERA_FPS, 30,
					MMCAM_AUDIO_SAMPLERATE, 44100,
					MMCAM_AUDIO_FORMAT, MM_CAMCORDER_AUDIO_FORMAT_PCM_S16_LE,
					MMCAM_AUDIO_CHANNEL, 2,
					MMCAM_AUDIO_INPUT_ROUTE, MM_AUDIOROUTE_CAPTURE_NORMAL,
					MMCAM_DISPLAY_ROTATION, MM_DISPLAY_ROTATION_270,
					MMCAM_DISPLAY_HANDLE, (void*) hdisplay,		hsize,
					MMCAM_TARGET_FILENAME, TARGET_FILENAME, 	strlen(TARGET_FILENAME),
					NULL);

	if (err < 0) 
	{
		printf("Set attrs fail. (%s:%x)\n", err_attr_name, err);
		if (err_attr_name) {
			free(err_attr_name);
			err_attr_name = NULL;
			return FALSE;
		}
	}

	err =  mm_camcorder_realize(hcam);
	if (err < 0) 
	{
		printf("Fail to call mm_camcorder_realize  = %x\n", err);
		return FALSE;
	}

	/* start camcorder */
	err = mm_camcorder_start(hcam);
	if (err < 0) 
	{
		printf("Fail to call mm_camcorder_start  = %x\n", err);
		return FALSE;
	}

	return TRUE;
}


static gboolean initialize_audio_capture() 	//audio only recording
{
	int err;
	MMCamPreset cam_info;
	char *err_attr_name = NULL;
	void * hdisplay = NULL;
	int hsize = 0;

	cam_info.videodev_type = MM_VIDEO_DEVICE_NONE;

	err = mm_camcorder_create(&hcam, &cam_info);

	if (err != MM_ERROR_NONE) 
	{
		printf("Fail to call mm_camcorder_create = %x\n", err);
		return FALSE;
	}

	mm_camcorder_set_message_callback(hcam, (MMMessageCallback)msg_callback, (void*)hcam);

	hdisplay = &ad.xid;
	hsize = sizeof(ad.xid);

	/* camcorder attribute setting */
	err = mm_camcorder_set_attributes((MMHandleType)hcam, &err_attr_name,
					MMCAM_MODE, MM_CAMCORDER_MODE_AUDIO,
					MMCAM_AUDIO_DEVICE, MM_AUDIO_DEVICE_MIC,
					MMCAM_AUDIO_ENCODER, MM_AUDIO_CODEC_AAC,
					MMCAM_FILE_FORMAT, MM_FILE_FORMAT_3GP,
					MMCAM_AUDIO_SAMPLERATE, 44100,
					MMCAM_AUDIO_FORMAT, MM_CAMCORDER_AUDIO_FORMAT_PCM_S16_LE,
					MMCAM_AUDIO_CHANNEL, 2,
					MMCAM_AUDIO_INPUT_ROUTE, MM_AUDIOROUTE_CAPTURE_NORMAL,
					MMCAM_TARGET_FILENAME, TARGET_FILENAME, strlen(TARGET_FILENAME),
					MMCAM_TARGET_TIME_LIMIT, 360000,
					NULL);

	if (err < 0) 
	{
		printf("Set attrs fail. (%s:%x)\n", err_attr_name, err);
		if (err_attr_name) {
			free(err_attr_name);
			err_attr_name = NULL;
			return FALSE;
		}
	}

	err =  mm_camcorder_realize(hcam);
	if (err < 0) 
	{
		printf("Fail to call mm_camcorder_realize  = %x\n", err);
		return FALSE;
	}

	/* start camcorder */
	err = mm_camcorder_start(hcam);
	if (err < 0) 
	{
		printf("Fail to call mm_camcorder_start  = %x\n", err);
		return FALSE;
	}

	return TRUE;
}


static gboolean uninitialize_camcorder()
{
	int err;

	err =  mm_camcorder_stop(hcam);
	if (err < 0) 
	{
		printf("Fail to call mm_camcorder_stop  = %x\n", err);
		return FALSE;
	}
	
	err =  mm_camcorder_unrealize(hcam);
	if (err < 0) 
	{
		printf("Fail to call mm_camcorder_unrealize  = %x\n", err);
		return FALSE;
	}

	err = mm_camcorder_destroy(hcam);
	if (err < 0) 
	{
		printf("Fail to call mm_camcorder_destroy  = %x\n", err);
		return FALSE;
	}

	return TRUE;
}


static gboolean capturing_picture() 
{
	int err;

	err =  mm_camcorder_capture_start(hcam);
	if (err < 0) 
	{
		printf("Fail to call mm_camcorder_capture_start  = %x\n", err);
		return FALSE;
	}

	//mm_camcorder_capture_stop should be called after getting MM_MESSAGE_CAMCORDER_CAPTURED message.
	sleep(3);

	return TRUE;
}


static gboolean record_and_save_video_file() 
{
	int err;

	/* Start recording */
	err =  mm_camcorder_record(hcam);
	if (err < 0) 
	{
		printf("Fail to call mm_camcorder_record  = %x\n", err);
		return FALSE;
	}

	sleep(5);

	/* Save file */
	err =  mm_camcorder_commit(hcam);
	if (err < 0) 
	{
		printf("Fail to call mm_camcorder_commit  = %x\n", err);
		return FALSE;
	}

	return TRUE;
}


static gboolean record_and_cancel_video_file() 
{
	int err;

	/* Start recording */
	err =  mm_camcorder_record(hcam);
	if (err < 0) 
	{
		printf("Fail to call mm_camcorder_record  = %x\n", err);
		return FALSE;
	}

	sleep(5);

	/* Cancel recording */
	err =  mm_camcorder_cancel(hcam);
	if (err < 0) 
	{
		printf("Fail to call mm_camcorder_cancel  = %x\n", err);
		return FALSE;
	}

	return TRUE;
}


static gboolean record_pause_and_resume_recording() 
{
	int err;

	/* Start recording */
	err =  mm_camcorder_record(hcam);
	if (err < 0) 
	{
		printf("Fail to call mm_camcorder_record  = %x\n", err);
		return FALSE;
	}

	sleep(5);

	/* Pause */
	err =  mm_camcorder_pause(hcam);
	if (err < 0) 
	{
		printf("Fail to call mm_camcorder_pause  = %x\n", err);
		return FALSE;
	}

	sleep(3);

	/* Resume */
	err =  mm_camcorder_record(hcam);
	if (err < 0) 
	{
		printf("Fail to call mm_camcorder_record  = %x\n", err);
		return FALSE;
	}

	sleep(3);

	/* Save file */
	err =  mm_camcorder_commit(hcam);
	if (err < 0) 
	{
		printf("Fail to call mm_camcorder_commit  = %x\n", err);
		return FALSE;
	}

	return TRUE;
}


static gboolean get_state_of_camcorder() 
{
	MMCamcorderStateType state;

	mm_camcorder_get_state(hcam, &state);
	printf("Current status is %d\n", state);

	return TRUE;
}


static gboolean start_autofocus() 
{
	int err;
	char * err_attr_name = NULL;

	/* Set focus mode to 'AUTO' and scan range to 'AF Normal' */
	err = mm_camcorder_set_attributes((MMHandleType)hcam, &err_attr_name,
					MMCAM_CAMERA_FOCUS_MODE, MM_CAMCORDER_FOCUS_MODE_AUTO,
					MMCAM_CAMERA_AF_SCAN_RANGE, MM_CAMCORDER_AUTO_FOCUS_NORMAL,
					NULL);

	if (err < 0) 
	{
		printf("Set attrs fail. (%s:%x)\n", err_attr_name, err);
		if (err_attr_name) {
			free(err_attr_name);
			err_attr_name = NULL;
			return FALSE;
		}
	}

	mm_camcorder_start_focusing(hcam);
	printf("Waiting for adjusting focus\n");

	/*Waiting for 'MM_MESSAGE_CAMCORDER_FOCUS_CHANGED' */
	sleep(3);

	return TRUE;
}

static gboolean filename_setting() 
{
	int err;
	char * new_filename =  "new_name.mp4";

	/* camcorder attribute setting */
	err = mm_camcorder_set_attributes((MMHandleType)hcam, NULL,
					MMCAM_TARGET_FILENAME, new_filename, strlen(new_filename),
					NULL);

	printf("New file name (%s)\n", new_filename);

	sleep(3);

	return TRUE;
}


static gboolean set_video_stream_callback() 
{
	mm_camcorder_set_video_stream_callback(hcam, (mm_camcorder_video_stream_callback)camcordertest_video_stream_cb, (void*)hcam);

	sleep(10);

	return TRUE;
}


/*---------------------------------------------------------------------------
|           APPFWK Functions:												|
---------------------------------------------------------------------------*/
int app_init(void *data)
{
	struct appdata *ad = data;

	printf("Function : %s", __func__);

	appcore_measure_start("app_init");
	
	ad->win_main = appcore_efl_get_main_window();
	if(ad->win_main == NULL) {
		printf("ad->win_main(%p)", ad->win_main);
		return (-1);
	}

	ad->evas = evas_object_evas_get(ad->win_main);

	ad->layout_main =  appcore_efl_load_edj(ad->win_main, "/usr/share/edje/cam_testsuite.edj", "main");
	if(ad->layout_main == NULL) {
		printf("ad->layout_main(%p)", ad->layout_main);
		return (-1);
	}

//	appcore_set_font_name(_("vera"));


	evas_object_layer_set(ad->win_main, 0);

	evas_object_move(ad->win_main, 0, 0); 
	evas_object_resize(ad->win_main, 800, 480);

	elm_win_rotation_set(ad->win_main, 270);
	elm_win_fullscreen_set(ad->win_main, 1);

	evas_object_color_set(ad->win_main, 0,0,0,0);
	elm_win_transparent_set(ad->win_main, 1);

  	if(!(ad->xid))
	{
		if(ad->win_main )
			ad->xid	= elm_win_xwindow_get(ad->win_main);
	}

	evas_object_show(ad->win_main);

	//
	{
		int bret;
		bret = mode_change();

		if(!bret)
		{
			printf("\t mode_change() fail. Exit from the application.\n");
		}
		menu();
	}	
	appcore_measure_time("app_init");

	return 0;
}


int app_exit(void *data)
{
	printf("Function : %s", __func__);

	return 0;
}


int app_start(void *data)
{
	printf("Function : %s", __func__);

	appcore_measure_start("app_start");

	appcore_measure_time("app_start");

	return 0;
}


int app_stop(void *data)
{
	printf("Function : %s", __func__);

	appcore_measure_start("app_stop");

	appcore_measure_time("app_stop");

	return 0;
}


static int app_procedure(int event, void *event_param, void *user_param) {
   switch (event) {
   case APPCORE_EVENT_CREATE: // initialize and create first view
   	{
		app_init(user_param);
		return 1;
   	}

   case APPCORE_EVENT_START:  // if necessary to speed up launching time, add deferred initialization which is not relevant to first view's drawing
	{
		app_start(user_param); 
		return 1;
	 }
	
   case APPCORE_EVENT_STOP:   // add necessary code when a multi-tasking supported application enters from the FG (on the TOP of screen) to the BG (on the LOW of screen)
	{
		app_stop(user_param); // // this event is received when a multi-tasking supported application enters from top screen to background if user clicks "HOME" key
		return 1;
	}
	
   case APPCORE_EVENT_RESUME: // add necessary code when a multi-tasking supported application enters from the BG (on the LOW of screen) to the FG (on the TOP of screen)
	{	
//		app_resume(user_param); 
		return 1;
   	}	
   case APPCORE_EVENT_TERMINATE: // free allocated resources before application exits
	{
		app_exit(user_param);
     	return 1;
   	}
	case APPCORE_EVENT_LOW_BATTERY: // free allocated resources before application exits
	 {
	 	return 1; /* 1 means this event is processed in user, 0 or -1 means this event is processed with system default behavior(application terminate) */ 
	}
	case APPCORE_EVENT_LANG_CHANGE: // update internalization according to language change
	 {
		 return 1;
	}
	default:
	 break;
	}
	return 0;
	
}


int idler_exit_cb(void *data)
{
//	ecore_x_window_hide(ad->xid);
	elm_exit();

	return 0;
}


static inline void flush_stdin()
{
	int ch;
	while((ch=getchar()) != EOF && ch != '\n');
}


static void menu()
{
	int mode = 0;
	int err = 0;
	//Get mode of camcorder	
	err = mm_camcorder_get_attributes(hcam, NULL,
									MMCAM_MODE,  &mode,
								       NULL);

	if (mode == MM_CAMCORDER_MODE_IMAGE)
	{
		printf("\nmm-camcorder Sample Application\n");
		printf("\t=======================================\n");
		printf("\t   '1' Capture image \n");
		printf("\t   '5' Get state \n");
		printf("\t   '6' Start Auto-focus \n");
		printf("\t   '8' Set video stream callback \n");
		printf("\t   'b' back.\n");
		printf("\t=======================================\n");
	}
	else if (mode == MM_CAMCORDER_MODE_VIDEO)
	{			
		printf("\nmm-camcorder Sample Application\n");
		printf("\t=======================================\n");
		printf("\t   '2' Record and Save video file \n");
		printf("\t   '3' Record and Cancel video file \n");
		printf("\t   '4' Record, Pause and Resume recording \n");
		printf("\t   '5' Get state \n");
		printf("\t   '6' Start Auto-focus \n");
		printf("\t   '7' Filename setting(only for recording) \n");
		printf("\t   'b' back.\n");
		printf("\t=======================================\n");
	}
	else
	{
		printf("\nmm-camcorder Sample Application\n");
		printf("\t=======================================\n");
		printf("\t   '2' Record and Save video file \n");
		printf("\t   '3' Record and Cancel video file \n");
		printf("\t   '4' Record, Pause and Resume recording \n");
		printf("\t   '5' Get state \n");
		printf("\t   '7' Filename setting(only for recording) \n");
		printf("\t   'b' back.\n");
		printf("\t=======================================\n");
	}

	return;
}


static gboolean mode_change() 
{
	char media_type = '\0';
	bool check= FALSE;

	while(!check)
	{
		printf("\t=============================\n");
		printf("\t	'i' Image.\n");
		printf("\t	'v' Video.\n");		
		printf("\t	'a' Audio.\n"); 
		printf("\t	'q' Exit.\n");
		printf("\t=============================\n");
		printf("\t  Enter the media type:\n\t");	

		while ((media_type=getchar()) == '\n');
		
		switch(media_type)
		{
			case 'i':
				printf("\t=============image================\n");

				if (!initialize_image_capture())
				{
					printf("Fail to call initialize_image_capture\n");
					goto _EXIT;
				}
				check = TRUE;
				break;
			case 'v':
				printf("\t=============video================\n");

				if (!initialize_video_capture())
				{
					printf("Fail to call initialize_video_capture\n");
					goto _EXIT;
				}
				check = TRUE;
				break;
			case 'a':
				printf("\t==============audio===============\n");

				if (!initialize_audio_capture())
				{
					printf("Fail to call initialize_audio_capture\n");
					goto _EXIT;
				}

				check = TRUE;
				break;		
			case 'q':
				printf("\t Quit Camcorder Sample Code!!\n");
				goto _EXIT;
			default:
				printf("\t Invalid media type(%d)\n", media_type);
				continue;
		}
	}

	ecore_x_sync();

	return TRUE;

_EXIT:
	ecore_timer_add(0.1, idler_exit_cb, NULL);
	return FALSE;
}


static void command(gchar buf)
{
	switch(buf)
	{
		case '1' : // Capture image
		{
			capturing_picture();
		}
			break;
		case '2' : // Record and Save video file
		{
			record_and_save_video_file();
		}
			break;
		case '3' : // Record and Cancel video file
		{
			record_and_cancel_video_file();
		}
			break;
		case '4' : // Record, Pause and Resume recording
		{
			record_pause_and_resume_recording();
		}
			break;
		case '5' : // Get state
		{
			get_state_of_camcorder();
		}
			break;
		case '6' : // Start Auto-focus
		{
			start_autofocus();
		}
			break;
		case '7' : // Filename setting(only for recording)
		{
			filename_setting();
		}
			break;
		case '8' : // Set video stream callback
		{
			set_video_stream_callback();
		}
			break;
		case 'b' : // back
			if (!uninitialize_camcorder())
			{
				printf("Fail to call uninitialize_camcorder\n");
				return;
			}

			mode_change();
			break;
		default:
			printf("\t Invalid input \n");
			break;					
	}			

}

 
static gboolean cmd_input(GIOChannel *channel)
{
	gchar buf[256];
	gsize read;

	g_io_channel_read(channel, buf, MAX_STRING_LEN, &read);
	buf[read] = '\0';
	g_strstrip(buf);

	command(buf[0]);

	menu();
	return TRUE;
}

 
int main(int argc, char **argv)
{
	int bret = 0;
	if (!g_thread_supported ()) 
		g_thread_init (NULL);
		
	stdin_channel = g_io_channel_unix_new (0);/* read from stdin */
	g_io_add_watch (stdin_channel, G_IO_IN, (GIOFunc)cmd_input,  NULL);
	
	appcore_measure_link_load_time("camera", NULL);

	memset(&ad, 0x0, sizeof(struct appdata));

	bret = appcore_efl_main("camera", argc, argv, app_procedure, 0, &ad /* user_param, if needed */ );
	
	printf("Main loop quit\n");

	printf("\t Exit from the application.\n");

	return 0;
}


/*EOF*/
