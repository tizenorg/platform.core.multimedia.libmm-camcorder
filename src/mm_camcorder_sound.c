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
#include <audio-session-manager.h>
#include "mm_camcorder_internal.h"
#include "mm_camcorder_sound.h"

/*---------------------------------------------------------------------------------------
|    GLOBAL VARIABLE DEFINITIONS for internal						|
---------------------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------------------
|    LOCAL VARIABLE DEFINITIONS for internal						|
---------------------------------------------------------------------------------------*/
#define BLOCK_SIZE 2048

/*---------------------------------------------------------------------------------------
|    LOCAL FUNCTION PROTOTYPES:								|
---------------------------------------------------------------------------------------*/
static gboolean __prepare_buffer(SOUND_INFO *info, char *filename);
static gboolean __cleanup_buffer(SOUND_INFO *info);
static void *__sound_open_thread_func(void *data);
static void *__sound_write_thread_func(void *data);
static void __solo_sound_callback(void *data);

static gboolean __prepare_buffer(SOUND_INFO *info, char *filename)
{
	mmf_return_val_if_fail(info, FALSE);
	mmf_return_val_if_fail(filename, FALSE);

	info->infile = sf_open(filename, SFM_READ, &info->sfinfo);
	if (!(info->infile)) {
		_mmcam_dbg_err("failed to open file [%s]", filename);
		return FALSE;
	}

	_mmcam_dbg_log("SOUND: frame       = %lld", info->sfinfo.frames);
	_mmcam_dbg_log("SOUND: sameplerate = %d", info->sfinfo.samplerate);
	_mmcam_dbg_log("SOUND: channel     = %d", info->sfinfo.channels);
	_mmcam_dbg_log("SOUND: format      = 0x%x", info->sfinfo.format);

	info->pcm_size = info->sfinfo.frames * info->sfinfo.channels * 2;
	info->pcm_buf = (short *)malloc(info->pcm_size);
	if (info->pcm_buf == NULL) {
		_mmcam_dbg_err("pcm_buf malloc failed");
		sf_close(info->infile);
		info->infile = NULL;
		return FALSE;
	}
	sf_read_short(info->infile, info->pcm_buf, info->pcm_size);

	return TRUE;
}


static gboolean __cleanup_buffer(SOUND_INFO *info)
{
	mmf_return_val_if_fail(info, FALSE);

	if (info->infile) {
		sf_close(info->infile);
		info->infile = NULL;
	}

	if (info->pcm_buf) {
		free(info->pcm_buf);
		info->pcm_buf = NULL;
	}

	_mmcam_dbg_log("Done");

	return TRUE;
}


static void *__sound_open_thread_func(void *data)
{
	int ret = 0;
	system_audio_route_t route = SYSTEM_AUDIO_ROUTE_POLICY_HANDSET_ONLY;
	SOUND_INFO *info = NULL;
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(data);

	mmf_return_val_if_fail(hcamcorder, NULL);

	MMTA_ACUM_ITEM_BEGIN("    __sound_open_thread_func", FALSE);

	info = &(hcamcorder->snd_info);

	__ta__("        __prepare_buffer",
	ret = __prepare_buffer(info, info->filename);
	);
	if (ret == FALSE) {
		goto EXIT_FUNC;
	}

	__ta__("        mm_sound_pcm_play_open",
	ret = mm_sound_pcm_play_open_ex(&(info->handle), info->sfinfo.samplerate,
	                                (info->sfinfo.channels == 1) ? MMSOUND_PCM_MONO : MMSOUND_PCM_STEREO,
	                                MMSOUND_PCM_S16_LE, VOLUME_TYPE_FIXED, ASM_EVENT_EXCLUSIVE_MMSOUND);
	);
	if (ret < 0) {
		/* error */
		_mmcam_dbg_err("mm_sound_pcm_play_open failed [%x]", ret);
		__cleanup_buffer(info);
		goto EXIT_FUNC;
	} else {
		/* success */
		info->state = _MMCAMCORDER_SOUND_STATE_PREPARE;
		_mmcam_dbg_log("mm_sound_pcm_play_open succeeded. state [%d]", info->state);
	}

	ret = mm_sound_route_get_system_policy(&route);
	if (ret != MM_ERROR_NONE) {
		_mmcam_dbg_err("mm_sound_route_get_system_policy failed [%x]", ret);
		goto POLICY_ERROR;
	}

	_mmcam_dbg_log("current policy [%d]", route);

	if (route != SYSTEM_AUDIO_ROUTE_POLICY_HANDSET_ONLY) {
		ret = mm_sound_route_set_system_policy(SYSTEM_AUDIO_ROUTE_POLICY_HANDSET_ONLY);
		if (ret != MM_ERROR_NONE) {
			_mmcam_dbg_err("mm_sound_route_set_system_policy failed [%x]", ret);
			goto POLICY_ERROR;
		}

		info->route_policy_backup = route;
	}

EXIT_FUNC:
	pthread_cond_signal(&(info->open_cond));
	pthread_mutex_unlock(&(info->open_mutex));

	_mmcam_dbg_log("Done");

	MMTA_ACUM_ITEM_END("    __sound_open_thread_func", FALSE);

	return NULL;

POLICY_ERROR:
	pthread_mutex_unlock(&(info->open_mutex));
	_mmcamcorder_sound_finalize((MMHandleType)hcamcorder);

	return NULL;
}


static void *__sound_write_thread_func(void *data)
{
	int ret = 0;
	int bytes_to_write = 0;
	int remain_bytes = 0;
	system_audio_route_t route = SYSTEM_AUDIO_ROUTE_POLICY_HANDSET_ONLY;
	char *buffer_to_write = NULL;
	SOUND_INFO *info = NULL;
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(data);

	mmf_return_val_if_fail(hcamcorder, NULL);

	info = &(hcamcorder->snd_info);

	_mmcam_dbg_log("RUN sound write thread");

	pthread_mutex_lock(&(info->play_mutex));

	do {
		pthread_cond_wait(&(info->play_cond), &(info->play_mutex));

		_mmcam_dbg_log("Signal received. Play sound.");

		if (info->thread_run == FALSE) {
			_mmcam_dbg_log("Exit thread command is detected");
			break;
		}

		ret = mm_sound_route_get_system_policy(&route);
		if (ret != MM_ERROR_NONE) {
			_mmcam_dbg_err("get_system_policy failed [%x]. skip sound play.", ret);
			break;
		}

		_mmcam_dbg_log("current policy [%d]", route);

		if (route != SYSTEM_AUDIO_ROUTE_POLICY_HANDSET_ONLY) {
			ret = mm_sound_route_set_system_policy(SYSTEM_AUDIO_ROUTE_POLICY_HANDSET_ONLY);
			if (ret != MM_ERROR_NONE) {
				_mmcam_dbg_err("set_system_policy failed. skip sound play.");
				break;
			}

			info->route_policy_backup = route;
		}

		buffer_to_write = (char *)info->pcm_buf;
		remain_bytes = info->pcm_size;
		bytes_to_write = 0;

		while (remain_bytes) {
			bytes_to_write = (remain_bytes >= BLOCK_SIZE) ? BLOCK_SIZE : remain_bytes;
			ret = mm_sound_pcm_play_write(info->handle, buffer_to_write, bytes_to_write);
			if (ret != bytes_to_write) {
				_mmcam_dbg_err("pcm write error [%x]", ret);
			}
			remain_bytes -= bytes_to_write;
			buffer_to_write += bytes_to_write;
		}
	} while (TRUE);

	pthread_mutex_unlock(&(info->play_mutex));

	_mmcam_dbg_log("END sound write thread");

	return NULL;
}


gboolean _mmcamcorder_sound_init(MMHandleType handle, char *filename)
{
	int ret = 0;
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	SOUND_INFO *info = NULL;

	mmf_return_val_if_fail(hcamcorder, FALSE);

	info = &(hcamcorder->snd_info);

	pthread_mutex_lock(&(info->open_mutex));

	if (info->state > _MMCAMCORDER_SOUND_STATE_NONE) {
		_mmcam_dbg_warn("already initialized [%d]", info->state);
		pthread_mutex_unlock(&(info->open_mutex));
		return FALSE;
	}

	if (info->filename) {
		free(info->filename);
		info->filename = NULL;
	}

	info->filename = strdup(filename);
	if (info->filename == NULL) {
		_mmcam_dbg_err("strdup failed");
		ret = FALSE;
	} else {
		pthread_mutex_init(&(info->play_mutex), NULL);
		pthread_cond_init(&(info->play_cond), NULL);
		if (pthread_create(&(info->thread), NULL, __sound_write_thread_func, (void *)handle) == 0) {
			info->thread_run = TRUE;
			info->state = _MMCAMCORDER_SOUND_STATE_INIT;
			info->route_policy_backup = -1;
			_mmcam_dbg_log("write thread created");
			ret = TRUE;
		} else {
			_mmcam_dbg_err("failed to create write thread");
			free(info->filename);
			info->filename = NULL;
			ret = FALSE;
		}
	}

	pthread_mutex_unlock(&(info->open_mutex));

	return ret;
}


gboolean _mmcamcorder_sound_prepare(MMHandleType handle)
{
	int ret = FALSE;
	pthread_t open_thread;
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	SOUND_INFO *info = NULL;

	mmf_return_val_if_fail(hcamcorder, FALSE);

	info = &(hcamcorder->snd_info);

	pthread_mutex_lock(&(info->open_mutex));

	if (info->state == _MMCAMCORDER_SOUND_STATE_INIT) {
		if (pthread_create(&open_thread, NULL, __sound_open_thread_func, (void *)handle) == 0) {
			_mmcam_dbg_log("open thread created");
			ret = TRUE;
		} else {
			_mmcam_dbg_err("failed to create open thread");
			ret = FALSE;
			pthread_mutex_unlock(&(info->open_mutex));
		}
	} else {
		_mmcam_dbg_warn("Wrong state [%d]", info->state);
		ret = FALSE;
		pthread_mutex_unlock(&(info->open_mutex));
	}

	return ret;
}


gboolean _mmcamcorder_sound_play(MMHandleType handle)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);
	SOUND_INFO *info = NULL;

	mmf_return_val_if_fail(hcamcorder, FALSE);

	info = &(hcamcorder->snd_info);

	pthread_mutex_lock(&(info->open_mutex));

	if (info->state < _MMCAMCORDER_SOUND_STATE_PREPARE) {
		_mmcam_dbg_log("not initialized state:[%d]", info->state);
		pthread_mutex_unlock(&(info->open_mutex));
		return FALSE;
	}

	_mmcam_dbg_log("Play start");

	pthread_mutex_lock(&(info->play_mutex));
	pthread_cond_signal(&(info->play_cond));
	pthread_mutex_unlock(&(info->play_mutex));

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

	pthread_mutex_lock(&(info->open_mutex));

	if (info->state < _MMCAMCORDER_SOUND_STATE_INIT) {
		_mmcam_dbg_warn("not initialized");
		pthread_mutex_unlock(&(info->open_mutex));
		return FALSE;
	}

	info->thread_run = 0;
	pthread_cond_signal(&(info->play_cond));

	if (info->thread) {
		_mmcam_dbg_log("wait for sound write thread join");
		pthread_join(info->thread, NULL);
		_mmcam_dbg_log("join done");
	}

	if (info->state == _MMCAMCORDER_SOUND_STATE_PREPARE) {
		_mmcam_dbg_log("restore route policy [%d]", info->route_policy_backup);

		if (info->route_policy_backup != -1) {
			mm_sound_route_set_system_policy(info->route_policy_backup);
		}

		mm_sound_pcm_play_close(info->handle);
		__cleanup_buffer(info);
	}

	if (info->filename) {
		free(info->filename);
		info->filename = NULL;
	}

	info->state = _MMCAMCORDER_SOUND_STATE_NONE;
	info->route_policy_backup = -1;

	pthread_mutex_destroy(&(info->play_mutex));
	pthread_cond_destroy(&(info->play_cond));

	pthread_mutex_unlock(&(info->open_mutex));

	_mmcam_dbg_log("Done");

	return TRUE;
}


void _mmcamcorder_sound_solo_play(MMHandleType handle, const char* filepath, gboolean sync)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(handle);

	int sound_handle = 0;
	int ret = 0;
	int sound_enable = TRUE;

	mmf_return_if_fail( filepath );

	_mmcam_dbg_log( "START" );

	ret = mm_camcorder_get_attributes((MMHandleType)hcamcorder, NULL,
	                                  "capture-sound-enable", &sound_enable,
	                                  NULL);
	if (ret == MM_ERROR_NONE) {
		if (sound_enable == FALSE) {
			_mmcam_dbg_log("Capture sound DISABLED.");
			return;
		}
	} else {
		_mmcam_dbg_warn("capture-sound-enable get FAILED.[%x]", ret);
	}

	ret = pthread_mutex_trylock(&(hcamcorder->sound_lock));
	if (ret != 0) {
		_mmcam_dbg_warn("g_mutex_trylock failed.[%s]", strerror(ret));
		return;
	}

	MMTA_ACUM_ITEM_BEGIN("CAPTURE SOUND:mm_sound_play_loud_solo_sound", FALSE);

	ret = mm_sound_play_loud_solo_sound(filepath, VOLUME_TYPE_FIXED, __solo_sound_callback,
	                                    (void*)hcamcorder, &sound_handle);
	if (ret != MM_ERROR_NONE) {
		_mmcam_dbg_err( "Capture sound play FAILED.[%x]", ret );
	} else {
		if (sync) {
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

	MMTA_ACUM_ITEM_END("CAPTURE SOUND:mm_sound_play_loud_solo_sound", FALSE);

	pthread_mutex_unlock(&(hcamcorder->sound_lock));

	_mmcam_dbg_log("DONE");

	return;
}

static void __solo_sound_callback(void *data)
{
	mmf_camcorder_t *hcamcorder = MMF_CAMCORDER(data);

	mmf_return_if_fail(hcamcorder);

	_mmcam_dbg_log("START");

	_mmcam_dbg_log("Signal SEND");
	pthread_cond_broadcast(&(hcamcorder->sound_cond));

	_mmcam_dbg_log("DONE");

	return;
}

