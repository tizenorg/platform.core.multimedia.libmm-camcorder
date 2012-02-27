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
#include "mm_camcorder_audiorec.h"
#include <math.h>

/*---------------------------------------------------------------------------------------
|    GLOBAL VARIABLE DEFINITIONS for internal						|
---------------------------------------------------------------------------------------*/
#define MM_CAMCORDER_START_CHANGE_STATE _MMCamcorderStartHelperFunc((void *)hcamcorder)
#define MM_CAMCORDER_STOP_CHANGE_STATE _MMCamcorderStopHelperFunc((void *)hcamcorder)
/*---------------------------------------------------------------------------------------
|    LOCAL VARIABLE DEFINITIONS for internal						|
---------------------------------------------------------------------------------------*/
#define RESET_PAUSE_TIME                        0
#define _MMCAMCORDER_AUDIO_MINIMUM_SPACE        (100*1024)
#define _MMCAMCORDER_AUDIO_MARGIN_SPACE         (1*1024)
#define _MMCAMCORDER_RETRIAL_COUNT              10
#define _MMCAMCORDER_FRAME_WAIT_TIME            20000 /* micro second */
#define _MMCAMCORDER_FREE_SPACE_CHECK_INTERVAL  10
/*---------------------------------------------------------------------------------------
|    LOCAL FUNCTION PROTOTYPES:								|
---------------------------------------------------------------------------------------*/
/* STATIC INTERNAL FUNCTION */
static gboolean __mmcamcorder_audio_dataprobe_voicerecorder(GstPad *pad, GstBuffer *buffer, gpointer u_data);
static gboolean __mmcamcorder_audio_dataprobe_record(GstPad *pad, GstBuffer *buffer, gpointer u_data);
static int __mmcamcorder_create_audiop_with_encodebin(MMHandleType handle);
static void __mmcamcorder_audiorec_pad_added_cb(GstElement *element, GstPad *pad, MMHandleType handle);

/*=======================================================================================
|  FUNCTION DEFINITIONS									|
=======================================================================================*/

/*---------------------------------------------------------------------------------------
|    GLOBAL FUNCTION DEFINITIONS:							|
---------------------------------------------------------------------------------------*/


static int __mmcamcorder_create_audiop_with_encodebin(MMHandleType handle)
{
	int err = MM_ERROR_NONE;
	char *aenc_name = NULL;
	char *mux_name = NULL;

	GstBus *bus = NULL;
	GstPad *srcpad = NULL;
	GstPad *sinkpad = NULL;
	GList *element_list = NULL;

	_MMCamcorderAudioInfo *info = NULL;
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;
	type_element *aenc_elem = NULL;
	type_element *mux_elem = NULL;

	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);
	sc = MMF_CAMCORDER_SUBCONTEXT(handle);

	mmf_return_val_if_fail(sc, MM_ERROR_CAMCORDER_NOT_INITIALIZED);
	mmf_return_val_if_fail(sc->element, MM_ERROR_CAMCORDER_NOT_INITIALIZED);
	mmf_return_val_if_fail(sc->info, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	info = (_MMCamcorderAudioInfo *)sc->info;
	
	_mmcam_dbg_log("");

	mux_elem = _mmcamcorder_get_type_element(handle, MM_CAM_FILE_FORMAT);
	err = _mmcamcorder_conf_get_value_element_name( mux_elem, &mux_name );

	if (!mux_name || !strcmp( mux_name, "wavenc" ) ) /* IF MUX in not chosen then record in raw amr file */
	{
		//But shoud we support non-mux recording??
		_mmcam_dbg_log("Record without muxing.");
		info->bMuxing = FALSE;
	}
	else
	{
		_mmcam_dbg_log("Record with mux.");
		info->bMuxing = TRUE;
	}

	//Create gstreamer element
	//Main pipeline
	__ta__("            camcorder_pipeline",  
	_MMCAMCORDER_PIPELINE_MAKE(sc, _MMCAMCORDER_MAIN_PIPE, "camcorder_pipeline", err);
	);

	__ta__("        __mmcamcorder_create_audiosrc_bin",
	err = _mmcamcorder_create_audiosrc_bin(handle);
	);
	if (err != MM_ERROR_NONE) {
		return err;
	}

	if (info->bMuxing) {
		/* Muxing. can use encodebin. */
		__ta__("        _mmcamcorder_create_encodesink_bin",
		err = _mmcamcorder_create_encodesink_bin((MMHandleType)hcamcorder);
		);
		if (err != MM_ERROR_NONE ) {
			return err;
		}
	} else {
		/* without muxing. can't use encodebin. */
		aenc_elem = _mmcamcorder_get_type_element(handle, MM_CAM_AUDIO_ENCODER);
		if (!aenc_elem)
		{
			_mmcam_dbg_err("Fail to get type element");
			err = MM_ERROR_CAMCORDER_RESOURCE_CREATION;
			goto pipeline_creation_error;
		}

		err = _mmcamcorder_conf_get_value_element_name(aenc_elem, &aenc_name);

		if ((!err) || (!aenc_name))
		{
			_mmcam_dbg_err("Fail to get element name");
			err = MM_ERROR_CAMCORDER_RESOURCE_CREATION;
			goto pipeline_creation_error;
		}

		__ta__("        audiopipeline_audioqueue",  
		_MMCAMCORDER_ELEMENT_MAKE(sc, _MMCAMCORDER_ENCSINK_AQUE, "queue",  NULL, element_list, err);
		);

		if( strcmp( aenc_name, "wavenc" ) != 0 )
		{
			__ta__("        audiopipeline_audioconvertor",
			_MMCAMCORDER_ELEMENT_MAKE(sc, _MMCAMCORDER_ENCSINK_CONV, "audioconvert",  NULL, element_list, err);
			);
		}

		__ta__("        audiopipeline_audioencoder",  
		_MMCAMCORDER_ELEMENT_MAKE(sc, _MMCAMCORDER_ENCSINK_AENC, aenc_name, NULL, element_list, err);
		);

		__ta__("        audiopipeline_filesink",  
		_MMCAMCORDER_ELEMENT_MAKE(sc, _MMCAMCORDER_ENCSINK_SINK, "filesink", NULL, element_list, err);
		);

		/* audio encoder attribute setting */
		if(strcmp(aenc_name,"ari_amrnbenc") == 0) //ari_armnbenc supports attatching amr header
		{
			MMCAMCORDER_G_OBJECT_SET( sc->element[_MMCAMCORDER_ENCSINK_AENC].gst, "write-header", TRUE);
		}
		
	}

	//Set basic infomation

	if (info->bMuxing) /* IF MUX is indicated create MUX */
	{
		gst_bin_add_many(GST_BIN(sc->element[_MMCAMCORDER_MAIN_PIPE].gst),
								 sc->element[_MMCAMCORDER_AUDIOSRC_BIN].gst,
								 sc->element[_MMCAMCORDER_ENCSINK_BIN].gst,
								 NULL);

		srcpad = gst_element_get_static_pad (sc->element[_MMCAMCORDER_AUDIOSRC_BIN].gst, "src");
		sinkpad = gst_element_get_static_pad (sc->element[_MMCAMCORDER_ENCSINK_BIN].gst, "audio_sink0");
		_MM_GST_PAD_LINK_UNREF(srcpad, sinkpad, err, pipeline_creation_error);
	}
	else /* IF MUX in not chosen then record in raw amr file */
	{
		if( !strcmp( aenc_name, "wavenc" ) )
		{
			gst_bin_add_many( GST_BIN(sc->element[_MMCAMCORDER_MAIN_PIPE].gst),
			                  sc->element[_MMCAMCORDER_AUDIOSRC_BIN].gst,
			                  sc->element[_MMCAMCORDER_ENCSINK_AQUE].gst,
			                  sc->element[_MMCAMCORDER_ENCSINK_AENC].gst,
			                  sc->element[_MMCAMCORDER_ENCSINK_SINK].gst,
			                  NULL );

			if (!_MM_GST_ELEMENT_LINK_MANY( sc->element[_MMCAMCORDER_AUDIOSRC_BIN].gst,
			                                sc->element[_MMCAMCORDER_ENCSINK_AQUE].gst,
			                                sc->element[_MMCAMCORDER_ENCSINK_AENC].gst,
			                                sc->element[_MMCAMCORDER_ENCSINK_SINK].gst,
			                                NULL ))
			{
				err = MM_ERROR_CAMCORDER_GST_LINK;
				goto pipeline_creation_error;
			}
		}
		else
		{
			gst_bin_add_many( GST_BIN(sc->element[_MMCAMCORDER_MAIN_PIPE].gst),
			                  sc->element[_MMCAMCORDER_AUDIOSRC_BIN].gst,
			                  sc->element[_MMCAMCORDER_ENCSINK_AQUE].gst,
			                  sc->element[_MMCAMCORDER_ENCSINK_CONV].gst,
			                  sc->element[_MMCAMCORDER_ENCSINK_AENC].gst,
			                  sc->element[_MMCAMCORDER_ENCSINK_SINK].gst,
			                  NULL );

			if (!_MM_GST_ELEMENT_LINK_MANY( sc->element[_MMCAMCORDER_AUDIOSRC_BIN].gst,
			                                sc->element[_MMCAMCORDER_ENCSINK_AQUE].gst,
			                                sc->element[_MMCAMCORDER_ENCSINK_CONV].gst,
			                                sc->element[_MMCAMCORDER_ENCSINK_AENC].gst,
			                                sc->element[_MMCAMCORDER_ENCSINK_SINK].gst,
			                                NULL ))
			{
				err = MM_ERROR_CAMCORDER_GST_LINK;
				goto pipeline_creation_error;
			}
		}
	}	


	//set data probe function
	srcpad = gst_element_get_static_pad(sc->element[_MMCAMCORDER_AUDIOSRC_SRC].gst, "src");
	MMCAMCORDER_ADD_BUFFER_PROBE(srcpad, _MMCAMCORDER_HANDLER_AUDIOREC,
		__mmcamcorder_audio_dataprobe_voicerecorder, hcamcorder);	
	gst_object_unref(srcpad);
	srcpad = NULL;

	if(info->bMuxing)
	{
		MMCAMCORDER_SIGNAL_CONNECT(sc->element[_MMCAMCORDER_ENCSINK_MUX].gst, 
										_MMCAMCORDER_HANDLER_AUDIOREC, 
										"pad-added", 
										__mmcamcorder_audiorec_pad_added_cb, 
										hcamcorder);		
	}
	else
	{
		srcpad = gst_element_get_static_pad(sc->element[_MMCAMCORDER_ENCSINK_AENC].gst, "src");
		MMCAMCORDER_ADD_BUFFER_PROBE(srcpad, _MMCAMCORDER_HANDLER_AUDIOREC,
			__mmcamcorder_audio_dataprobe_record, hcamcorder);
		gst_object_unref(srcpad);
		srcpad = NULL;		
	}

	/*
	 * To get the message callback from the gstreamer.
	 * This can be used to make the API calls asynchronous
	 * as per LiMO compliancy
	 */
	bus = gst_pipeline_get_bus(GST_PIPELINE(sc->element[_MMCAMCORDER_MAIN_PIPE].gst));
	hcamcorder->pipeline_cb_event_id = gst_bus_add_watch( bus, (GstBusFunc)_mmcamcorder_pipeline_cb_message, hcamcorder );
	gst_bus_set_sync_handler(bus, gst_bus_sync_signal_handler, hcamcorder);	
	gst_object_unref(bus);

	return MM_ERROR_NONE;

pipeline_creation_error:
	return err;
}


int
_mmcamcorder_create_audio_pipeline(MMHandleType handle)
{
	mmf_camcorder_t *hcamcorder= MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;
	
	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);
	sc = MMF_CAMCORDER_SUBCONTEXT(handle);

	mmf_return_val_if_fail(sc, MM_ERROR_CAMCORDER_NOT_INITIALIZED);

	return __mmcamcorder_create_audiop_with_encodebin(handle);
}


/**
 * This function destroy audio pipeline.
 *
 * @param[in]	handle		Handle of camcorder.
 * @return	void
 * @remarks
 * @see		_mmcamcorder_destroy_pipeline()
 *
 */
void
_mmcamcorder_destroy_audio_pipeline(MMHandleType handle)
{
	mmf_camcorder_t *hcamcorder= MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;
	_MMCamcorderAudioInfo* info = NULL;
	mmf_return_if_fail(hcamcorder);
	sc = MMF_CAMCORDER_SUBCONTEXT(handle);

	mmf_return_if_fail(sc);
	mmf_return_if_fail(sc->element);

	info = sc->info;
	
	_mmcam_dbg_log("");

	if(sc->element[_MMCAMCORDER_MAIN_PIPE].gst)
	{
		_mmcamcorder_gst_set_state(handle, sc->element[_MMCAMCORDER_MAIN_PIPE].gst, GST_STATE_NULL);

		_mmcamcorder_remove_all_handlers((MMHandleType)hcamcorder, _MMCAMCORDER_HANDLER_CATEGORY_ALL);
		
		if( info->bMuxing)
		{
			GstPad *reqpad = NULL;
			//release request pad
			/* FIXME */
			/*
			    The ref_count of mux is always # of streams in here, i don't know why it happens. 
			    So, i unref the mux manually 
			*/				
			reqpad = gst_element_get_static_pad(sc->element[_MMCAMCORDER_ENCSINK_ENCBIN].gst, "audio");
			gst_element_release_request_pad(sc->element[_MMCAMCORDER_ENCSINK_ENCBIN].gst, reqpad);
			gst_object_unref(reqpad);	
			
			if(GST_IS_ELEMENT(sc->element[_MMCAMCORDER_ENCSINK_MUX].gst) && \
				GST_OBJECT_REFCOUNT_VALUE(sc->element[_MMCAMCORDER_ENCSINK_MUX].gst) > 1)					
				gst_object_unref(sc->element[_MMCAMCORDER_ENCSINK_MUX].gst);
		}
		gst_object_unref(sc->element[_MMCAMCORDER_MAIN_PIPE].gst);
//		sc->element[_MMCAMCORDER_MAIN_PIPE].gst = NULL;
	}

}


/**
 * This function operates each command on audio mode.
 *
 * @param	c		[in]	Handle of camcorder context.
 * @param	command	[in]	command type received from Multimedia Framework.
 *
 * @return	This function returns MM_ERROR_NONE on success, or the other values
 *			on error.
 * @remark
 * @see		_mmcamcorder_set_functions()
 *
 */
 /* ADDED BY SISO */


void* _MMCamcorderStartHelperFunc(void *handle)
{
	mmf_camcorder_t *hcamcorder= MMF_CAMCORDER(handle);
	_mmcamcorder_set_state((MMHandleType)hcamcorder, hcamcorder->target_state);

	return NULL;
}

void* _MMCamcorderStopHelperFunc(void *handle)
{
	mmf_camcorder_t *hcamcorder= MMF_CAMCORDER(handle);	
	_mmcamcorder_set_state((MMHandleType)hcamcorder, hcamcorder->target_state);

	return NULL;
}


int
_mmcamcorder_audio_command(MMHandleType handle, int command)
{
	int cmd = command;
	GstElement *pipeline = NULL;
	GstElement *audioSrc = NULL;
	int ret = MM_ERROR_NONE;
	int err = 0;
	char *dir_name = NULL;	
	int size=0;
	guint64 free_space = 0;
	mmf_camcorder_t *hcamcorder= MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;
	_MMCamcorderAudioInfo 	*info	= NULL;
	char *err_attr_name = NULL;
	
	mmf_return_val_if_fail(hcamcorder, MM_ERROR_CAMCORDER_NOT_INITIALIZED);
	sc = MMF_CAMCORDER_SUBCONTEXT(handle);

	mmf_return_val_if_fail(sc, MM_ERROR_CAMCORDER_NOT_INITIALIZED);
	mmf_return_val_if_fail(sc->element, MM_ERROR_CAMCORDER_NOT_INITIALIZED);
	mmf_return_val_if_fail(sc->info, MM_ERROR_CAMCORDER_NOT_INITIALIZED);
	pipeline = sc->element[_MMCAMCORDER_MAIN_PIPE].gst;
	info = sc->info;
	
	_mmcam_dbg_log("");

	pipeline = sc->element[_MMCAMCORDER_MAIN_PIPE].gst;
	audioSrc = sc->element[_MMCAMCORDER_AUDIOSRC_SRC].gst;
	switch(cmd)
	{
		case _MMCamcorder_CMD_RECORD:
			//check status for resume case
			if (_mmcamcorder_get_state((MMHandleType)hcamcorder) != MM_CAMCORDER_STATE_PAUSED) 
			{
				guint imax_time = 0;
				char *temp_filename = NULL;

				if(sc->pipeline_time) {
					gst_pipeline_set_new_stream_time(GST_PIPELINE(pipeline), sc->pipeline_time);
				}
				sc->pipeline_time = RESET_PAUSE_TIME;

				ret = mm_camcorder_get_attributes(handle, &err_attr_name,
				                                  MMCAM_TARGET_TIME_LIMIT, &imax_time,
				                                  MMCAM_FILE_FORMAT, &(info->fileformat),
				                                  MMCAM_TARGET_FILENAME, &temp_filename, &size,
				                                  NULL);
				if (ret != MM_ERROR_NONE) {
					_mmcam_dbg_warn("failed to get attribute. (%s:%x)", err_attr_name, ret);
					SAFE_FREE (err_attr_name);
					goto _ERR_CAMCORDER_AUDIO_COMMAND;
				}

				info->filename = strdup(temp_filename);
				if (!info->filename)
				{
					_mmcam_dbg_err("STRDUP was failed");
					goto _ERR_CAMCORDER_AUDIO_COMMAND;
				}
				
				_mmcam_dbg_log("Record start : set file name using attribute - %s\n ",info->filename);

				MMCAMCORDER_G_OBJECT_SET( sc->element[_MMCAMCORDER_ENCSINK_SINK].gst, "location", info->filename);

				sc->ferror_send = FALSE;
				sc->ferror_count = 0;
				sc->bget_eos = FALSE;
				info->filesize =0;

				/* set max time */
				if (imax_time <= 0) {
					info->max_time = 0; /* do not check */
				} else {
					info->max_time = ((guint64)imax_time) * 1000; /* to millisecond */
				}

/*
				//set data probe function	
				gst_pad_add_buffer_probe(gst_element_get_pad(sc->element[_MMCAMCORDER_AUDIOSRC_SRC].gst, "src"), 						 
								 G_CALLBACK(__mmcamcorder_audio_dataprobe_voicerecorder),							 
								 hcamcorder); 								 
*/								 
/* TODO : check free space before recording start, need to more discussion */			
#if 1 
				dir_name = g_path_get_dirname(info->filename);
				err = _mmcamcorder_get_freespace(dir_name, &free_space);
				if((err == -1) || free_space <= (_MMCAMCORDER_AUDIO_MINIMUM_SPACE+(5*1024)))
				{
					_mmcam_dbg_err("No more space for recording - %s : [%" G_GUINT64_FORMAT "]\n ", dir_name, free_space);			
					if(dir_name)
					{
						g_free(dir_name);
						dir_name = NULL;
					}				
					return MM_MESSAGE_CAMCORDER_NO_FREE_SPACE;
				}
				if(dir_name)
				{
					g_free(dir_name);
					dir_name = NULL;
				}
#endif
			}		

			ret = _mmcamcorder_gst_set_state(handle, pipeline, GST_STATE_PLAYING);
			if(ret<0) {
				goto _ERR_CAMCORDER_AUDIO_COMMAND;
			}
			break;

		case _MMCamcorder_CMD_PAUSE:
			{
				GstClock *clock = NULL;
				int count = 0;
				
				if (info->b_commiting)
				{
					_mmcam_dbg_warn("now on commiting previous file!!(cmd : %d)", cmd);
					return MM_ERROR_CAMCORDER_CMD_IS_RUNNING;
				}

				for(count=0; count <= _MMCAMCORDER_RETRIAL_COUNT ; count++)
				{
					if(info->filesize > 0)
					{
						break;
					}
					else if(count == _MMCAMCORDER_RETRIAL_COUNT)
					{
						_mmcam_dbg_err("Pause fail, we are waiting for 100 milisecond, but still file size is %" G_GUINT64_FORMAT "", 
									info->filesize);
						return MM_ERROR_CAMCORDER_INVALID_CONDITION;
					}
					else
					{
						_mmcam_dbg_warn("Pause is Waiting for enough audio frame, retrial count[%d], file size is %" G_GUINT64_FORMAT "", 
									count, info->filesize);					
					}
					usleep(_MMCAMCORDER_FRAME_WAIT_TIME);
				}

				ret = _mmcamcorder_gst_set_state(handle, pipeline, GST_STATE_PAUSED);
				if(ret<0) {
					goto _ERR_CAMCORDER_AUDIO_COMMAND;
				}	
				//fixed me. consider delay.
				clock = gst_pipeline_get_clock(GST_PIPELINE(pipeline));
				sc->pipeline_time = gst_clock_get_time(clock) - gst_element_get_base_time(GST_ELEMENT(sc->element[_MMCAMCORDER_MAIN_PIPE].gst));
			}
			break;

		case _MMCamcorder_CMD_CANCEL:
			if (info->b_commiting)
			{
				_mmcam_dbg_warn("now on commiting previous file!!(cmd : %d)", cmd);
				return MM_ERROR_CAMCORDER_CMD_IS_RUNNING;
			}
			
//			ret = _mmcamcorder_gst_set_state(handle, pipeline, GST_STATE_NULL);
			ret = _mmcamcorder_gst_set_state(handle, pipeline, GST_STATE_READY);
			if(ret<0) {
				goto _ERR_CAMCORDER_AUDIO_COMMAND;
			}

			if(info->bMuxing)
			{
				MMCAMCORDER_G_OBJECT_SET( sc->element[_MMCAMCORDER_ENCSINK_ENCBIN].gst, "block", FALSE);
			}
			else
			{
				MMCAMCORDER_G_OBJECT_SET( sc->element[_MMCAMCORDER_ENCSINK_AQUE].gst, "empty-buffers", FALSE);
			}

			_mmcamcorder_gst_set_state(handle,  sc->element[_MMCAMCORDER_ENCSINK_SINK].gst, GST_STATE_NULL);

			sc->pipeline_time = 0;
			sc->pause_time = 0;
			sc->isMaxsizePausing = FALSE;
			sc->isMaxtimePausing = FALSE;
			
			if(info->filename)
			{
				_mmcam_dbg_log("file delete(%s)", info->filename);
				unlink(info->filename);
				g_free(info->filename);
				info->filename = NULL;					
			}

			break;
		case _MMCamcorder_CMD_COMMIT:
		{
			int count = 0;
			g_print("\n\n _MMCamcorder_CMD_COMMIT\n\n");

			if (info->b_commiting)
			{
				_mmcam_dbg_warn("now on commiting previous file!!(cmd : %d)", cmd);
				return MM_ERROR_CAMCORDER_CMD_IS_RUNNING;
			}
			else
			{
				_mmcam_dbg_log("_MMCamcorder_CMD_COMMIT : start");
				info->b_commiting = TRUE;
			}			
			
			for(count=0; count <= _MMCAMCORDER_RETRIAL_COUNT ; count++)
			{
				if(info->filesize > 0)
				{
					break;
				}
				else if(count == _MMCAMCORDER_RETRIAL_COUNT)
				{
					_mmcam_dbg_err("Commit fail, we are waiting for 100 milisecond, but still file size is %" G_GUINT64_FORMAT "", 
								info->filesize);
					info->b_commiting = FALSE;
					return MM_ERROR_CAMCORDER_INVALID_CONDITION;						
				}
				else
				{
					_mmcam_dbg_warn("Commit is Waiting for enough audio frame, retrial count[%d], file size is %" G_GUINT64_FORMAT "", 
								count, info->filesize);					
				}
				usleep(_MMCAMCORDER_FRAME_WAIT_TIME);
			}
			
			if (audioSrc) {
				GstPad *pad = gst_element_get_static_pad (audioSrc, "src");
//				gst_pad_push_event (pad, gst_event_new_eos());
				ret = gst_element_send_event(audioSrc, gst_event_new_eos());
				gst_object_unref(pad);
				pad = NULL;					
				if (_mmcamcorder_get_state((MMHandleType)hcamcorder) == MM_CAMCORDER_STATE_PAUSED) // for pause -> commit case
				{				
					ret = _mmcamcorder_gst_set_state(handle, pipeline, GST_STATE_PLAYING);  
					if(ret<0) {
						goto _ERR_CAMCORDER_AUDIO_COMMAND;
					}				
				}
				
			}

			//wait until finishing EOS
			_mmcam_dbg_log("Start to wait EOS");
			if ((ret =_mmcamcorder_get_eos_message(handle)) != MM_ERROR_NONE)
			{
				goto _ERR_CAMCORDER_AUDIO_COMMAND;
			}
/*
			 while ((!sc->get_eos)&&((retry_cnt--) > 0)) {
				if ((gMessage = gst_bus_timed_pop (bus, timeout)) != NULL)
				{
					if (GST_MESSAGE_TYPE(gMessage) ==GST_MESSAGE_EOS)
					{
						_mmcam_dbg_log("Get a EOS message");
						gst_message_unref(gMessage);
						break;
					}
					else
					{
						_mmcam_dbg_log("Get another message(%x). Post this message to bus again.", GST_MESSAGE_TYPE(gMessage));
						gst_bus_post(bus, gMessage);
					}
				}
				else
				{
					_mmcam_dbg_log("timeout of gst_bus_timed_pop()");
					break;
				}					
				
				usleep(100);		//To Prevent busy waiting
			}

			_mmcamcorder_audio_handle_eos((MMHandleType)hcamcorder);
*/


		}
			break;

		case _MMCamcorder_CMD_PREVIEW_START:
		//	MM_CAMCORDER_START_CHANGE_STATE; 
			break;
		case _MMCamcorder_CMD_PREVIEW_STOP:
		//	MM_CAMCORDER_STOP_CHANGE_STATE; 
			//void
			break;

		default:
			ret = MM_ERROR_CAMCORDER_INVALID_ARGUMENT;
			break;
	}

_ERR_CAMCORDER_AUDIO_COMMAND:
	return ret;

}

int
_mmcamcorder_audio_handle_eos(MMHandleType handle)
{
	mmf_camcorder_t *hcamcorder= MMF_CAMCORDER(handle);
	_MMCamcorderSubContext *sc = NULL;
	_MMCamcorderAudioInfo 	*info	= NULL;
	GstElement				*pipeline = NULL;
	_MMCamcorderMsgItem msg;
	MMCamRecordingReport * report;

	int err = MM_ERROR_NONE;
		
	mmf_return_val_if_fail(hcamcorder, FALSE);
	sc = MMF_CAMCORDER_SUBCONTEXT(handle);

	mmf_return_val_if_fail(sc, FALSE);
	mmf_return_val_if_fail(sc->info, FALSE);
	
	_mmcam_dbg_err("");

	info	= sc->info;

	//changing pipeline for display
	pipeline = sc->element[_MMCAMCORDER_MAIN_PIPE].gst;
	

	__ta__("        _MMCamcorder_CMD_COMMIT:GST_STATE_READY",  
	err = _mmcamcorder_gst_set_state(handle, pipeline, GST_STATE_READY);
	);

	if( err != MM_ERROR_NONE )
	{
		_mmcam_dbg_warn( "Failed:_MMCamcorder_CMD_COMMIT:GST_STATE_READY. err[%x]", err );
	}

//	__ta__("        _MMCamcorder_CMD_COMMIT:GST_STATE_NULL",  
//	_mmcamcorder_gst_set_state(handle, pipeline, GST_STATE_NULL);
//	)	

	//Send message	
	//Send recording report to application				
	msg.id = MM_MESSAGE_CAMCORDER_CAPTURED;

	report = (MMCamRecordingReport*) malloc(sizeof(MMCamRecordingReport));	//who free this?
	if (!report)
	{
		//think more.
		_mmcam_dbg_err("Recording report fail(%s). Out of memory.", info->filename);
		return FALSE;
	}

	report->recording_filename = strdup(info->filename);
	msg.param.data= report;

	_mmcamcroder_send_message(handle, &msg);

	
	if(info->bMuxing)
	{
		MMCAMCORDER_G_OBJECT_SET( sc->element[_MMCAMCORDER_ENCSINK_ENCBIN].gst, "block", FALSE);
	}
	else
	{
		MMCAMCORDER_G_OBJECT_SET( sc->element[_MMCAMCORDER_ENCSINK_AQUE].gst, "empty-buffers", FALSE);
	}	
	_mmcamcorder_gst_set_state(handle,  sc->element[_MMCAMCORDER_ENCSINK_SINK].gst, GST_STATE_NULL);

	sc->pipeline_time = 0;
	sc->pause_time = 0;
	sc->isMaxsizePausing = FALSE;
	sc->isMaxtimePausing = FALSE;

	g_free(info->filename);
	info->filename = NULL;

	_mmcam_dbg_err("_MMCamcorder_CMD_COMMIT : end");

	info->b_commiting = FALSE;

	return TRUE;
}


static float
__mmcamcorder_get_decibel(unsigned char* raw, int size, MMCamcorderAudioFormat format)
{
	#define MAX_AMPLITUDE_MEAN_16BIT 23170.115738161934
	#define MAX_AMPLITUDE_MEAN_08BIT    89.803909382810

	int i = 0;
	int depthByte = 0;
	int count = 0;

	short* pcm16 = 0;
	char* pcm8 = 0;

	float db = 0.0;
	float rms = 0.0;
	unsigned long long square_sum = 0;

	if (format == MM_CAMCORDER_AUDIO_FORMAT_PCM_S16_LE)
		depthByte = 2;
	else		//MM_CAMCORDER_AUDIO_FORMAT_PCM_U8
		depthByte = 1;

	for( ; i < size ; i += (depthByte<<1) )
	{
		if (depthByte == 1)
		{
			pcm8 = (char *)(raw + i);
			square_sum += (*pcm8)*(*pcm8);
		}
		else		//2byte
		{
			pcm16 = (short*)(raw + i);
			square_sum += (*pcm16)*(*pcm16);
		}

		count++;
	}

	rms = sqrt( square_sum/count );

	if (depthByte == 1)
		db = 20 * log10( rms/MAX_AMPLITUDE_MEAN_08BIT );
	else
		db = 20 * log10( rms/MAX_AMPLITUDE_MEAN_16BIT );

	/*
	_mmcam_dbg_log("size[%d],depthByte[%d],count[%d],rms[%f],db[%f]",
	               size, depthByte, count, rms, db);
	*/

	return db;
}


static gboolean
__mmcamcorder_audio_dataprobe_voicerecorder(GstPad *pad, GstBuffer *buffer, gpointer u_data)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(u_data);
	double volume = 0.0;
	int format = 0;
	int channel = 0;
	float curdcb = 0.0;
	_MMCamcorderMsgItem msg;
	int err = MM_ERROR_UNKNOWN;
	char *err_name = NULL;

	mmf_return_val_if_fail(hcamcorder, FALSE);

	/* Set volume to audio input */
	err = mm_camcorder_get_attributes((MMHandleType)hcamcorder, &err_name,
									MMCAM_AUDIO_VOLUME, &volume,
									MMCAM_AUDIO_FORMAT, &format,
									MMCAM_AUDIO_CHANNEL, &channel,
									NULL);
	if (err < 0) 
	{
		_mmcam_dbg_warn("Get attrs fail. (%s:%x)", err_name, err);
		SAFE_FREE (err_name);
		return err;
	}
	
	if(volume == 0) //mute
		    memset (GST_BUFFER_DATA(buffer), 0,  GST_BUFFER_SIZE(buffer));

	/* Get current volume level of real input stream */
//	currms = __mmcamcorder_get_RMS(GST_BUFFER_DATA(buffer), GST_BUFFER_SIZE(buffer), depth);
	__ta__( "__mmcamcorder_get_decibel",
	curdcb = __mmcamcorder_get_decibel(GST_BUFFER_DATA(buffer), GST_BUFFER_SIZE(buffer), format);
	);

	msg.id = MM_MESSAGE_CAMCORDER_CURRENT_VOLUME;
	msg.param.rec_volume_dB = curdcb;
	_mmcamcroder_send_message((MMHandleType)hcamcorder, &msg);

	/* CALL audio stream callback */
	if ((hcamcorder->astream_cb) && buffer && GST_BUFFER_DATA(buffer))
	{
		MMCamcorderAudioStreamDataType stream;

		if (_mmcamcorder_get_state((MMHandleType)hcamcorder) < MM_CAMCORDER_STATE_PREPARE)
		{
			_mmcam_dbg_warn("Not ready for stream callback");
			return TRUE;
		}

		/*
		_mmcam_dbg_log("Call audio steramCb, data[%p], format[%d], channel[%d], length[%d], volume_dB[%f]",
			       GST_BUFFER_DATA(buffer), format, channel, GST_BUFFER_SIZE(buffer), curdcb);
		*/

		stream.data = (void *)GST_BUFFER_DATA(buffer);
		stream.format = format;
		stream.channel = channel;
		stream.length = GST_BUFFER_SIZE(buffer);
		stream.timestamp = (unsigned int)(GST_BUFFER_TIMESTAMP(buffer)/1000000);	//nano -> msecond
		stream.volume_dB = curdcb;

		_MMCAMCORDER_LOCK_ASTREAM_CALLBACK( hcamcorder );

		if(hcamcorder->astream_cb)
		{
			hcamcorder->astream_cb(&stream, hcamcorder->astream_cb_param);
		}

		_MMCAMCORDER_UNLOCK_ASTREAM_CALLBACK( hcamcorder );
	}

	return TRUE;
}


static void
__mmcamcorder_audiorec_pad_added_cb(GstElement *element, GstPad *pad,  MMHandleType handle)
{
	mmf_camcorder_t *hcamcorder= MMF_CAMCORDER(handle);

	_mmcam_dbg_log("ENTER(%s)", GST_PAD_NAME(pad));
	//FIXME : the name of audio sink pad of wavparse, oggmux doesn't have 'audio'. How could I handle the name?
	if((strstr(GST_PAD_NAME(pad), "audio")) || (strstr(GST_PAD_NAME(pad), "sink")))
	{
		MMCAMCORDER_ADD_BUFFER_PROBE(pad, _MMCAMCORDER_HANDLER_AUDIOREC,
			__mmcamcorder_audio_dataprobe_record, hcamcorder);
	}
	else
	{
		_mmcam_dbg_warn("Unknow pad is added, check it : [%s]", GST_PAD_NAME(pad));	
	}

	return;
}


static gboolean __mmcamcorder_audio_dataprobe_record(GstPad *pad, GstBuffer *buffer, gpointer u_data)
{
	static int count = 0;
	guint64 rec_pipe_time = 0;
	guint64 free_space = 0;
	guint64 buffer_size = 0;
	guint64 trailer_size = 0;
	char *filename = NULL;

	_MMCamcorderSubContext *sc = NULL;
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(u_data);
	_MMCamcorderAudioInfo *info = NULL;
	_MMCamcorderMsgItem msg;

	mmf_return_val_if_fail(hcamcorder, FALSE);
	mmf_return_val_if_fail(buffer, FALSE);

	sc = MMF_CAMCORDER_SUBCONTEXT(hcamcorder);
	mmf_return_val_if_fail(sc && sc->info, FALSE);
	info = sc->info;

	if (sc->isMaxtimePausing || sc->isMaxsizePausing) {
		_mmcam_dbg_warn("isMaxtimePausing[%d],isMaxsizePausing[%d]",
		                sc->isMaxtimePausing, sc->isMaxsizePausing);
		return FALSE;
	}

	buffer_size = (guint64)GST_BUFFER_SIZE(buffer);

	if (info->filesize == 0) {
		if (info->fileformat == MM_FILE_FORMAT_WAV) {
			info->filesize += 44; /* wave header size */
		} else if (info->fileformat == MM_FILE_FORMAT_AMR) {
			info->filesize += 6; /* amr header size */
		}

		info->filesize += buffer_size;
		return TRUE;
	}

	if (sc->ferror_send) {
		_mmcam_dbg_warn("file write error, drop frames");
		return FALSE;
	}

	/* get trailer size */
	if (info->fileformat == MM_FILE_FORMAT_3GP || info->fileformat == MM_FILE_FORMAT_MP4) {
		MMCAMCORDER_G_OBJECT_GET(sc->element[_MMCAMCORDER_ENCSINK_MUX].gst, "expected-trailer-size", &trailer_size);
		/*_mmcam_dbg_log("trailer_size %d", trailer_size);*/
	} else {
		trailer_size = 0; /* no trailer */
	}

	filename = info->filename;

	/* to minimizing free space check overhead */
	count = count % _MMCAMCORDER_FREE_SPACE_CHECK_INTERVAL;
	if (count++ == 0) {
		gint free_space_ret = _mmcamcorder_get_freespace(filename, &free_space);

		/*_mmcam_dbg_log("check free space for recording");*/

		switch (free_space_ret) {
		case -2: /* file not exist */
		case -1: /* failed to get free space */
			_mmcam_dbg_err("Error occured. [%d]", free_space_ret);
			if (sc->ferror_count == 2 && sc->ferror_send == FALSE) {
				sc->ferror_send = TRUE;
				msg.id = MM_MESSAGE_CAMCORDER_ERROR;
				if (free_space_ret == -2) {
					msg.param.code = MM_ERROR_FILE_NOT_FOUND;
				} else {
					msg.param.code = MM_ERROR_FILE_READ;
				}
				_mmcamcroder_send_message((MMHandleType)hcamcorder, &msg);
			} else {
				sc->ferror_count++;
			}

			return FALSE; /* skip this buffer */

		default: /* succeeded to get free space */
			/* check free space for recording */
			if (free_space < (guint64)(_MMCAMCORDER_AUDIO_MINIMUM_SPACE + buffer_size + trailer_size)) {
				_mmcam_dbg_warn("No more space for recording!!!");
				_mmcam_dbg_warn("Free Space : [%" G_GUINT64_FORMAT "], file size : [%" G_GUINT64_FORMAT "]",
				                free_space, info->filesize);

				if (info->bMuxing) {
					MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_ENCSINK_ENCBIN].gst, "block", TRUE);
				} else {
					MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_ENCSINK_AQUE].gst, "empty-buffers", TRUE);
				}

				sc->isMaxsizePausing = TRUE;
				msg.id = MM_MESSAGE_CAMCORDER_NO_FREE_SPACE;
				_mmcamcroder_send_message((MMHandleType)hcamcorder, &msg);

				return FALSE; /* skip this buffer */
			}
			break;
		}
	}

	if (!GST_CLOCK_TIME_IS_VALID(GST_BUFFER_TIMESTAMP(buffer))) {
		_mmcam_dbg_err("Buffer timestamp is invalid, check it");
		return FALSE;
	}

	rec_pipe_time = GST_TIME_AS_MSECONDS(GST_BUFFER_TIMESTAMP(buffer));

	/* check recording time limit and send recording status message */
	if (info->max_time > 0 && rec_pipe_time > info->max_time) {
		_mmcam_dbg_warn("Current time : [%" G_GUINT64_FORMAT "], Maximum time : [%" G_GUINT64_FORMAT "]", \
		                rec_pipe_time, info->max_time);

		if (info->bMuxing) {
			MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_ENCSINK_ENCBIN].gst, "block", TRUE);
		} else {
			MMCAMCORDER_G_OBJECT_SET(sc->element[_MMCAMCORDER_ENCSINK_AQUE].gst, "empty-buffers", TRUE);
		}

		sc->isMaxtimePausing = TRUE;
		msg.id = MM_MESSAGE_CAMCORDER_TIME_LIMIT;
		_mmcamcroder_send_message((MMHandleType)hcamcorder, &msg);

		/* skip this buffer */
		return FALSE;
	}

	/* send message for recording time and recorded file size */
	if (info->b_commiting == FALSE) {
		info->filesize += buffer_size;

		msg.id = MM_MESSAGE_CAMCORDER_RECORDING_STATUS;
		msg.param.recording_status.elapsed = (unsigned int)rec_pipe_time;
		msg.param.recording_status.filesize = (unsigned int)((info->filesize + trailer_size) >> 10);
		_mmcamcroder_send_message((MMHandleType)hcamcorder, &msg);

		return TRUE;
	} else {
		/* skip this buffer if commit process has been started */
		return FALSE;
	}
}
