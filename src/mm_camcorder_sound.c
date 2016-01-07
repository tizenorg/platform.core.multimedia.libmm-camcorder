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
#include <mm_sound.h>
#include <mm_sound_private.h>
#include "mm_camcorder_internal.h"
#include "mm_camcorder_sound.h"

/*---------------------------------------------------------------------------------------
|    GLOBAL VARIABLE DEFINITIONS for internal						|
---------------------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------------------
|    LOCAL VARIABLE DEFINITIONS for internal						|
---------------------------------------------------------------------------------------*/
#define SAMPLE_SOUND_RATE       44100
#define DEFAULT_ACTIVE_DEVICE   0xffffffff

/*---------------------------------------------------------------------------------------
|    LOCAL FUNCTION PROTOTYPES:								|
---------------------------------------------------------------------------------------*/
static void __solo_sound_callback(void *data);

static void __pulseaudio_play_sample_cb(pa_context *pulse_context, uint32_t stream_index, void *user_data)
{
	SOUND_INFO *info = NULL;

	mmf_return_if_fail(user_data);

	info = (SOUND_INFO *)user_data;

	_mmcam_dbg_log("START - idx : %d", stream_index);

	pa_threaded_mainloop_signal(info->pulse_mainloop, 0);

	_mmcam_dbg_log("DONE");

	return;
}

static void __pulseaudio_context_state_cb(pa_context *pulse_context, void *user_data)
{
	int state = 0;
	SOUND_INFO *info = NULL;

	mmf_return_if_fail(user_data);

	info = (SOUND_INFO *)user_data;

	state = pa_context_get_state(pulse_context);
	switch (state) {
	case PA_CONTEXT_READY:
		_mmcam_dbg_log("pulseaudio context READY");
		if (info->pulse_context == pulse_context) {
			/* Signal */
			_mmcam_dbg_log("pulseaudio send signal");
			pa_threaded_mainloop_signal(info->pulse_mainloop, 0);
		}
		break;
	case PA_CONTEXT_TERMINATED:
		if (info->pulse_context == pulse_context) {
			/* Signal */
			_mmcam_dbg_log("Context terminated : pulseaudio send signal");
			pa_threaded_mainloop_signal(info->pulse_mainloop, 0);
		}
		break;
	case PA_CONTEXT_UNCONNECTED:
	case PA_CONTEXT_CONNECTING:
	case PA_CONTEXT_AUTHORIZING:
	case PA_CONTEXT_SETTING_NAME:
	case PA_CONTEXT_FAILED:
	default:
		_mmcam_dbg_log("pulseaudio context %p, state %d",
		               pulse_context, state);
		break;
	}

	return;
}

gboolean _mmcamcorder_sound_init(MMHandleType handle)
{
	int ret = 0;
	int sound_enable = TRUE;
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	SOUND_INFO *info = NULL;
	pa_mainloop_api *api = NULL;
	int error = PA_ERR_INTERNAL;

	mmf_return_val_if_fail(hcamcorder, FALSE);

	/* check sound play enable */
	ret = mm_camcorder_get_attributes((MMHandleType)hcamcorder, NULL,
	                                  "capture-sound-enable", &sound_enable,
	                                  NULL);
	_mmcam_dbg_log("Capture sound enable %d", sound_enable);
	if (!sound_enable) {
		_mmcam_dbg_warn("capture sound disabled");
		return FALSE;
	}

	info = &(hcamcorder->snd_info);

	pthread_mutex_lock(&(info->open_mutex));

	if (info->state > _MMCAMCORDER_SOUND_STATE_NONE) {
		_mmcam_dbg_warn("already initialized [%d]", info->state);
		pthread_mutex_unlock(&(info->open_mutex));
		return TRUE;
	}

	pthread_mutex_init(&(info->play_mutex), NULL);
	pthread_cond_init(&(info->play_cond), NULL);

	/**
	 * Init Pulseaudio thread
	 */
	/* create pulseaudio mainloop */
	info->pulse_mainloop = pa_threaded_mainloop_new();
	if (info->pulse_mainloop == NULL) {
		_mmcam_dbg_err("pa_threaded_mainloop_new failed");
		goto SOUND_INIT_ERROR;
	}

	/* start PA mainloop */
	ret = pa_threaded_mainloop_start(info->pulse_mainloop);
	if (ret < 0) {
		_mmcam_dbg_err("pa_threaded_mainloop_start failed");
		goto SOUND_INIT_ERROR;
	}

	/* lock pulseaudio thread */
	pa_threaded_mainloop_lock(info->pulse_mainloop);

	/* get pulseaudio api */
	api = pa_threaded_mainloop_get_api(info->pulse_mainloop);
	if (api == NULL) {
		_mmcam_dbg_err("pa_threaded_mainloop_get_api failed");
		pa_threaded_mainloop_unlock(info->pulse_mainloop);
		goto SOUND_INIT_ERROR;
	}

	/* create pulseaudio context */
	info->pulse_context = pa_context_new(api, NULL);
	if (info->pulse_context == NULL) {
		_mmcam_dbg_err("pa_context_new failed");
		pa_threaded_mainloop_unlock(info->pulse_mainloop);
		goto SOUND_INIT_ERROR;
	}

	/* set pulseaudio context callback */
	pa_context_set_state_callback(info->pulse_context, __pulseaudio_context_state_cb, info);

	if (pa_context_connect(info->pulse_context, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL) < 0) {
		_mmcam_dbg_err("pa_context_connect error");
	}

	/* wait READY state of pulse context */
	while (TRUE) {
		pa_context_state_t state = pa_context_get_state(info->pulse_context);

		_mmcam_dbg_log("pa context state is now %d", state);

		if (!PA_CONTEXT_IS_GOOD (state)) {
			_mmcam_dbg_log("connection failed");
			break;
		}

		if (state == PA_CONTEXT_READY) {
			_mmcam_dbg_log("pa context READY");
			break;
		}

		/* Wait until the context is ready */
		_mmcam_dbg_log("waiting..................");
		pa_threaded_mainloop_wait(info->pulse_mainloop);
		_mmcam_dbg_log("waiting DONE. check again...");
	}

	/* unlock pulseaudio thread */
	pa_threaded_mainloop_unlock(info->pulse_mainloop);

	if (info->sample_stream) {
		pa_stream_connect_playback(info->sample_stream, NULL, NULL, 0, NULL, NULL);

		for (;;) {
			pa_stream_state_t state = pa_stream_get_state(info->sample_stream);

			if (state == PA_STREAM_READY) {
				_mmcam_dbg_warn("device READY done");
				break;
			}

			if (!PA_STREAM_IS_GOOD(state)) {
				error = pa_context_errno(info->pulse_context);
				_mmcam_dbg_err("pa context state is not good, %d", error);
				break;
			}

			/* Wait until the stream is ready */
			pa_threaded_mainloop_wait(info->pulse_mainloop);
		}
	}

	//info->volume_type = PA_TIZEN_AUDIO_VOLUME_TYPE_FIXED;
	info->volume_level = 0;

	info->state = _MMCAMCORDER_SOUND_STATE_INIT;

	_mmcam_dbg_log("init DONE");

	pthread_mutex_unlock(&(info->open_mutex));

	return TRUE;

SOUND_INIT_ERROR:
	/* remove pulse mainloop */
	if (info->pulse_mainloop) {
		pa_threaded_mainloop_lock(info->pulse_mainloop);

		/* remove pulse context */
		if (info->pulse_context) {
			/* release sample stream */
			if (info->sample_stream) {
				pa_stream_disconnect(info->sample_stream);
				pa_stream_unref(info->sample_stream);
				info->sample_stream = NULL;
			}

			/* Make sure we don't get any further callbacks */
			pa_context_set_state_callback(info->pulse_context, NULL, NULL);

			pa_context_disconnect(info->pulse_context);
			pa_context_unref(info->pulse_context);
			info->pulse_context = NULL;
		}

		pa_threaded_mainloop_unlock(info->pulse_mainloop);

		pa_threaded_mainloop_stop(info->pulse_mainloop);
		pa_threaded_mainloop_free(info->pulse_mainloop);
		info->pulse_mainloop = NULL;
	}

	/* remove mutex and cond */
	pthread_mutex_destroy(&(info->play_mutex));
	pthread_cond_destroy(&(info->play_cond));

	pthread_mutex_unlock(&(info->open_mutex));

	return FALSE;
}


gboolean _mmcamcorder_sound_play(MMHandleType handle, const char *sample_name, gboolean sync_play)
{
	int sound_enable = TRUE;
	int gain_type = VOLUME_GAIN_SHUTTER1;

	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	SOUND_INFO *info = NULL;
/*
	pa_operation *pulse_op = NULL;
*/

	mmf_return_val_if_fail(hcamcorder && sample_name, FALSE);

	/* check sound play enable */
	mm_camcorder_get_attributes((MMHandleType)hcamcorder, NULL,
	                            "capture-sound-enable", &sound_enable,
	                            NULL);
	_mmcam_dbg_log("Capture sound enable %d", sound_enable);
	if (!sound_enable) {
		_mmcam_dbg_warn("capture sound disabled");
		return FALSE;
	}

	info = &(hcamcorder->snd_info);

	pthread_mutex_lock(&(info->open_mutex));

	if (info->state < _MMCAMCORDER_SOUND_STATE_INIT) {
		_mmcam_dbg_log("not initialized state:[%d]", info->state);
		pthread_mutex_unlock(&(info->open_mutex));
		return FALSE;
	}

	if (!strcmp(sample_name, _MMCAMCORDER_SAMPLE_SOUND_NAME_CAPTURE)) {
		gain_type = VOLUME_GAIN_SHUTTER2;
	} else if (!strcmp(sample_name, _MMCAMCORDER_SAMPLE_SOUND_NAME_REC_STOP)) {
		gain_type = VOLUME_GAIN_CAMCORDING;
	}

	_mmcam_dbg_log("Play start - sample name [%s]", sample_name);

	if (sync_play) {
		pa_threaded_mainloop_lock(info->pulse_mainloop);
/*
		pulse_op = pa_ext_policy_play_sample(info->pulse_context,
		                                  sample_name,
		                                  info->volume_type,
		                                  gain_type,
		                                  info->volume_level,
		                                  __pulseaudio_play_sample_cb,
		                                  info);
*/
		_mmcam_dbg_log("wait for signal");
		pa_threaded_mainloop_wait(info->pulse_mainloop);
		_mmcam_dbg_log("received signal");

		pa_threaded_mainloop_unlock(info->pulse_mainloop);
	} else {
/*
		pulse_op = pa_ext_policy_play_sample(info->pulse_context,
		                                  sample_name,
		                                  info->volume_type,
		                                  gain_type,
		                                  info->volume_level,
		                                  NULL,
		                                  NULL);
*/
	}

/*
	if (pulse_op) {
		pa_operation_unref(pulse_op);
		pulse_op = NULL;
	}
*/

	pthread_mutex_unlock(&(info->open_mutex));

	_mmcam_dbg_log("Done");

	return TRUE;
}


gboolean _mmcamcorder_sound_finalize(MMHandleType handle)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	SOUND_INFO *info = NULL;

	mmf_return_val_if_fail(hcamcorder, FALSE);

	info = &(hcamcorder->snd_info);

	_mmcam_dbg_err("START");

	pthread_mutex_lock(&(info->open_mutex));

	if (info->state < _MMCAMCORDER_SOUND_STATE_INIT) {
		_mmcam_dbg_warn("not initialized");
		pthread_mutex_unlock(&(info->open_mutex));
		return TRUE;
	}

	pa_threaded_mainloop_lock(info->pulse_mainloop);

	if (info->sample_stream) {
		pa_stream_disconnect(info->sample_stream);
		pa_stream_unref(info->sample_stream);
		info->sample_stream = NULL;
	}

	/**
	 * Release pulseaudio thread
	 */
	_mmcam_dbg_log("release pulseaudio thread");

	pa_context_disconnect(info->pulse_context);

	/* Make sure we don't get any further callbacks */
	pa_context_set_state_callback(info->pulse_context, NULL, NULL);

	pa_context_unref(info->pulse_context);
	info->pulse_context = NULL;

	pa_threaded_mainloop_unlock(info->pulse_mainloop);

	pa_threaded_mainloop_stop(info->pulse_mainloop);
	pa_threaded_mainloop_free(info->pulse_mainloop);
	info->pulse_mainloop = NULL;

	info->state = _MMCAMCORDER_SOUND_STATE_NONE;

	/* release mutex and cond */
	_mmcam_dbg_log("release play_mutex/cond");
	pthread_mutex_destroy(&(info->play_mutex));
	pthread_cond_destroy(&(info->play_cond));

	pthread_mutex_unlock(&(info->open_mutex));

	_mmcam_dbg_err("DONE");

	return TRUE;
}


void _mmcamcorder_sound_solo_play(MMHandleType handle, const char* filepath, gboolean sync_play)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);

	int sound_handle = 0;
	int ret = MM_ERROR_NONE;
	int sound_enable = TRUE;
	int sound_played = FALSE;
	int gain_type = VOLUME_GAIN_SHUTTER1;

	mmf_return_if_fail(filepath && hcamcorder);

	_mmcam_dbg_log("START : %s", filepath);

	_mmcamcorder_sound_solo_play_wait(handle);

	ret = pthread_mutex_trylock(&(hcamcorder->sound_lock));
	if (ret != 0) {
		_mmcam_dbg_warn("g_mutex_trylock failed.[%s]", strerror(ret));
		return;
	}

	/* check filename to set gain_type */
	if (!strcmp(filepath, _MMCAMCORDER_FILEPATH_CAPTURE_SND) ||
	    !strcmp(filepath, _MMCAMCORDER_FILEPATH_REC_START_SND)) {
		if (!strcmp(filepath, _MMCAMCORDER_FILEPATH_REC_START_SND)) {
			gain_type = VOLUME_GAIN_CAMCORDING;
		} else {
			gain_type = VOLUME_GAIN_SHUTTER1;
		}
	} else if (!strcmp(filepath, _MMCAMCORDER_FILEPATH_CAPTURE2_SND)) {
		gain_type = VOLUME_GAIN_SHUTTER2;
	} else if (!strcmp(filepath, _MMCAMCORDER_FILEPATH_REC_STOP_SND)) {
		gain_type = VOLUME_GAIN_CAMCORDING;
	}

	_mmcam_dbg_log("gain type 0x%x", gain_type);

	ret = mm_camcorder_get_attributes((MMHandleType)hcamcorder, NULL,
	                                  "capture-sound-enable", &sound_enable,
	                                  NULL);
	_mmcam_dbg_log("Capture sound enable %d", sound_enable);

	if (!sound_enable) {
		/* send capture sound completed message */
		pthread_mutex_unlock(&(hcamcorder->sound_lock));
		return;
	}


	if (hcamcorder->shutter_sound_policy == VCONFKEY_CAMERA_SHUTTER_SOUND_POLICY_ON ||
	    hcamcorder->sub_context->info_image->sound_status) {
		ret = mm_sound_play_sound(filepath, VOLUME_TYPE_FIXED | gain_type,
		                          (mm_sound_stop_callback_func)__solo_sound_callback, (void*)hcamcorder, &sound_handle);
		sound_played = TRUE;
	} else {
		_mmcam_dbg_warn("skip shutter sound");
	}

	_mmcam_dbg_log("sync_play %d, sound_played %d, ret 0x%x", sync_play, sound_played, ret);

	if (ret != MM_ERROR_NONE) {
		_mmcam_dbg_err( "Capture sound play FAILED.[%x]", ret );
	} else {
		if (sound_played) {
			/* increase capture sound count */
			hcamcorder->capture_sound_count++;
		}

		/* wait for sound completed signal */
		if (sync_play && sound_played) {
			struct timespec timeout;
			struct timeval tv;

			gettimeofday( &tv, NULL );
			timeout.tv_sec = tv.tv_sec + 2;
			timeout.tv_nsec = tv.tv_usec * 1000;

			_mmcam_dbg_log("Wait for signal");

			if (!pthread_cond_timedwait(&(hcamcorder->sound_cond), &(hcamcorder->sound_lock), &timeout)) {
				_mmcam_dbg_log("signal received.");
			} else {
				_mmcam_dbg_warn("capture sound play timeout.");
				if (sound_handle > 0) {
					mm_sound_stop_sound(sound_handle);
				}
			}
		}
	}

	pthread_mutex_unlock(&(hcamcorder->sound_lock));

	_mmcam_dbg_log("DONE");

	return;
}

static void __solo_sound_callback(void *data)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(data);

	mmf_return_if_fail(hcamcorder);

	_mmcam_dbg_log("START");

	/* decrease capture sound count */
	pthread_mutex_lock(&(hcamcorder->sound_lock));
	if (hcamcorder->capture_sound_count > 0) {
		hcamcorder->capture_sound_count--;
	} else {
		_mmcam_dbg_warn("invalid capture_sound_count %d, reset count", hcamcorder->capture_sound_count);
		hcamcorder->capture_sound_count = 0;
	}
	pthread_mutex_unlock(&(hcamcorder->sound_lock));

	_mmcam_dbg_log("Signal SEND");
	pthread_cond_broadcast(&(hcamcorder->sound_cond));

	_mmcam_dbg_log("DONE");

	return;
}


void _mmcamcorder_sound_solo_play_wait(MMHandleType handle)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);

	mmf_return_if_fail(hcamcorder);

	_mmcam_dbg_log("START");

	/* check playing sound count */
	pthread_mutex_lock(&(hcamcorder->sound_lock));
	if (hcamcorder->capture_sound_count > 0) {
		struct timespec timeout;
		struct timeval tv;

		gettimeofday( &tv, NULL );
		timeout.tv_sec = tv.tv_sec + 2;
		timeout.tv_nsec = tv.tv_usec * 1000;

		_mmcam_dbg_log("Wait for signal");

		if (!pthread_cond_timedwait(&(hcamcorder->sound_cond), &(hcamcorder->sound_lock), &timeout)) {
			_mmcam_dbg_log("signal received.");
		} else {
			_mmcam_dbg_warn("capture sound play timeout.");
		}
	} else {
		_mmcam_dbg_warn("no playing sound - count %d", hcamcorder->capture_sound_count);
	}
	pthread_mutex_unlock(&(hcamcorder->sound_lock));

	_mmcam_dbg_log("DONE");

	return;
}
