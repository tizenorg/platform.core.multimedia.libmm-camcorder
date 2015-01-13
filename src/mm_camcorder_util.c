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
#include <stdio.h>
#include <stdarg.h>
#include <sys/vfs.h> /* struct statfs */
#include <sys/stat.h>
#include <gst/video/video-info.h>

#include "mm_camcorder_internal.h"
#include "mm_camcorder_util.h"
#include "mm_camcorder_sound.h"
#include <mm_util_imgp.h>

/*-----------------------------------------------------------------------
|    GLOBAL VARIABLE DEFINITIONS for internal				|
-----------------------------------------------------------------------*/

/*-----------------------------------------------------------------------
|    LOCAL VARIABLE DEFINITIONS for internal				|
-----------------------------------------------------------------------*/
#define TIME_STRING_MAX_LEN     64

#define FPUTC_CHECK(x_char, x_file)\
{\
	if (fputc(x_char, x_file) == EOF) \
	{\
		_mmcam_dbg_err("[Critical] fputc() returns fail.\n");	\
		return FALSE;\
	}\
}
#define FPUTS_CHECK(x_str, x_file)\
{\
	if (fputs(x_str, x_file) == EOF) \
	{\
		_mmcam_dbg_err("[Critical] fputs() returns fail.\n");\
		SAFE_FREE(str);	\
		return FALSE;\
	}\
}

/*---------------------------------------------------------------------------
|    LOCAL FUNCTION PROTOTYPES:												|
---------------------------------------------------------------------------*/
/* STATIC INTERNAL FUNCTION */
	
//static gint 		skip_mdat(FILE *f);
static guint16           get_language_code(const char *str);
static gchar*            str_to_utf8(const gchar *str);
static inline gboolean   write_tag(FILE *f, const gchar *tag);
static inline gboolean   write_to_32(FILE *f, guint val);
static inline gboolean   write_to_16(FILE *f, guint val);
static inline gboolean   write_to_24(FILE *f, guint val);

/*===========================================================================================
|																							|
|  FUNCTION DEFINITIONS																		|
|  																							|
========================================================================================== */
/*---------------------------------------------------------------------------
|    GLOBAL FUNCTION DEFINITIONS:											|
---------------------------------------------------------------------------*/

gint32 _mmcamcorder_double_to_fix(gdouble d_number)
{
	return (gint32) (d_number * 65536.0);
}

// find top level tag only, do not use this function for finding sub level tags
gint _mmcamcorder_find_tag(FILE *f, guint32 tag_fourcc, gboolean do_rewind)
{
	guchar buf[8];

	if (do_rewind) {
		rewind(f);
	}

	while (fread(&buf, sizeof(guchar), 8, f)>0) {
		unsigned long long buf_size = 0;
		unsigned int buf_fourcc = MMCAM_FOURCC(buf[4], buf[5], buf[6], buf[7]);

		if (tag_fourcc == buf_fourcc) {
			_mmcam_dbg_log("find tag : %c%c%c%c", MMCAM_FOURCC_ARGS(tag_fourcc));
			return TRUE;
		} else {
			_mmcam_dbg_log("skip [%c%c%c%c] tag", MMCAM_FOURCC_ARGS(buf_fourcc));

			buf_size = (unsigned long long)_mmcamcorder_get_container_size(buf);
			buf_size = buf_size - 8; /* include tag */

			do {
				if (buf_size > _MMCAMCORDER_MAX_INT) {
					_mmcam_dbg_log("seek %d", _MMCAMCORDER_MAX_INT);
					if (fseek(f, _MMCAMCORDER_MAX_INT, SEEK_CUR) != 0) {
						_mmcam_dbg_err("fseek() fail");
						return FALSE;
					}

					buf_size -= _MMCAMCORDER_MAX_INT;
				} else {
					_mmcam_dbg_log("seek %d", buf_size);
					if (fseek(f, buf_size, SEEK_CUR) != 0) {
						_mmcam_dbg_err("fseek() fail");
						return FALSE;
					}
					break;
				}
			} while (TRUE);
		}
	}

	_mmcam_dbg_log("cannot find tag : %c%c%c%c", MMCAM_FOURCC_ARGS(tag_fourcc));

	return FALSE;
}

gboolean _mmcamcorder_update_size(FILE *f, gint64 prev_pos, gint64 curr_pos)
{
	_mmcam_dbg_log("size : %"G_GINT64_FORMAT"", curr_pos-prev_pos);
	if(fseek(f, prev_pos, SEEK_SET) != 0)
	{
		_mmcam_dbg_err("fseek() fail");
		return FALSE;
	}

	if (!write_to_32(f, curr_pos -prev_pos))
		return FALSE;
	
	if(fseek(f, curr_pos, SEEK_SET) != 0)
	{
		_mmcam_dbg_err("fseek() fail");
		return FALSE;
	}
	
	return TRUE;
}

gboolean _mmcamcorder_write_loci(FILE *f, _MMCamcorderLocationInfo info)
{
	gint64 current_pos, pos;
	gchar *str = NULL;

	_mmcam_dbg_log("");

	if((pos = ftell(f))<0)
	{
		_mmcam_dbg_err("ftell() returns negative value");	
		return FALSE;
	}
	
	if(!write_to_32(f, 0)) //size
		return FALSE;
	
	if(!write_tag(f, "loci")) 	// type
		return FALSE;
	
	FPUTC_CHECK(0, f);		// version

	if(!write_to_24(f, 0))	// flags
		return FALSE;
	
	if(!write_to_16(f, get_language_code("eng"))) // language
		return FALSE;
	
	str = str_to_utf8("location_name");
	
	FPUTS_CHECK(str, f); // name
	SAFE_FREE(str);
	
	FPUTC_CHECK('\0', f);
	FPUTC_CHECK(0, f);		//role
	
	if(!write_to_32(f, info.longitude))	// Longitude
		return FALSE;
	
	if(!write_to_32(f, info.latitude)) // Latitude
		return FALSE;
	
	if(! write_to_32(f, info.altitude))	// Altitude
		return FALSE;
	
	str = str_to_utf8("Astronomical_body");
	FPUTS_CHECK(str, f);//Astronomical_body
	SAFE_FREE(str);
	
	FPUTC_CHECK('\0', f);
	
	str = str_to_utf8("Additional_notes");
	FPUTS_CHECK(str, f); // Additional_notes
	SAFE_FREE(str);
	
	FPUTC_CHECK('\0', f);
	
	if((current_pos = ftell(f))<0)
	{
		_mmcam_dbg_err("ftell() returns negative value");	
		return FALSE;
	}
	
	if(! _mmcamcorder_update_size(f, pos, current_pos))
		return FALSE;

	return TRUE;
}

void _mmcamcorder_write_Latitude(FILE *f,int degreex10000)
{
	bool isNegative = (degreex10000 < 0);
	char sign = isNegative? '-': '+';

	// Handle the whole part
	char str[9];
	int wholePart = 0;
	int fractionalPart = 0;

	wholePart = degreex10000 / 10000;
	if (wholePart == 0) {
		snprintf(str, 5, "%c%.2d.", sign, wholePart);
	} else {
		snprintf(str, 5, "%+.2d.", wholePart);
	}

	// Handle the fractional part
	fractionalPart = degreex10000 - (wholePart * 10000);
	if (fractionalPart < 0) {
		fractionalPart = -fractionalPart;
	}
	snprintf(&str[4], 5, "%.4d", fractionalPart);

	// Do not write the null terminator
	write_tag(f, str);

	return;
}


gboolean _mmcamcorder_write_udta(FILE *f, _MMCamcorderLocationInfo info)
{
	gint64 current_pos, pos;

	_mmcam_dbg_log("");

	if((pos = ftell(f))<0)
	{
		_mmcam_dbg_err("ftell() returns negative value");
		return FALSE;
	}

	if(!write_to_32(f, 0)) 	//size
		return FALSE;

	if(!write_tag(f, "udta")) 	// type
		return FALSE;

	if(! _mmcamcorder_write_loci(f, info))
		return FALSE;

	if((current_pos = ftell(f))<0)
	{
		_mmcam_dbg_err("ftell() returns negative value");
		return FALSE;
	}

	if(! _mmcamcorder_update_size(f, pos, current_pos))
		return FALSE;

	return TRUE;
}


guint64 _mmcamcorder_get_container_size(const guchar *size)
{
	guint64 result = 0;
	guint64 temp = 0;

	temp = size[0];
	result = temp << 24;
	temp = size[1];
	result = result | (temp << 16);
	temp = size[2];
	result = result | (temp << 8);
	result = result | size[3];

	_mmcam_dbg_log("result : %lld", (unsigned long long)result);

	return result;
}


gboolean _mmcamcorder_update_composition_matrix(FILE *f, int orientation)
{
	/* for 0 degree */
	guint32 a = 0x00010000;
	guint32 b = 0;
	guint32 c = 0;
	guint32 d = 0x00010000;

	switch (orientation) {
	case MM_CAMCORDER_TAG_VIDEO_ORT_90:/* 90 degree */
		a = 0;
		b = 0x00010000;
		c = 0xffff0000;
		d = 0;
		break;
	case MM_CAMCORDER_TAG_VIDEO_ORT_180:/* 180 degree */
		a = 0xffff0000;
		d = 0xffff0000;
		break;
	case MM_CAMCORDER_TAG_VIDEO_ORT_270:/* 270 degree */
		a = 0;
		b = 0xffff0000;
		c = 0x00010000;
		d = 0;
		break;
	case MM_CAMCORDER_TAG_VIDEO_ORT_NONE:/* 0 degree */
	default:
		break;
	}

	write_to_32(f, a);
	write_to_32(f, b);
	write_to_32(f, 0);
	write_to_32(f, c);
	write_to_32(f, d);
	write_to_32(f, 0);
	write_to_32(f, 0);
	write_to_32(f, 0);
	write_to_32(f, 0x40000000);

	_mmcam_dbg_log("orientation : %d, write data 0x%x 0x%x 0x%x 0x%x",
	               orientation, a, b, c, d);

	return TRUE;
}


int _mmcamcorder_get_freespace(const gchar *path, guint64 *free_space)
{
	struct statfs fs;

	g_assert(path);

	if (!g_file_test(path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		_mmcam_dbg_log("File(%s) doesn't exist.", path);
		return -2;
	}

	if (-1 == statfs(path, &fs)) {
		_mmcam_dbg_log("Getting free space is failed.(%s)", path);
		return -1;
	}

	*free_space = (guint64)fs.f_bsize * fs.f_bavail;
	return 1;
}


int _mmcamcorder_get_file_system_type(const gchar *path, int *file_system_type)
{
	struct statfs fs;

	g_assert(path);

	if (!g_file_test(path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		_mmcam_dbg_log("File(%s) doesn't exist.", path);
		return -2;
	}

	if (-1 == statfs(path, &fs)) {
		_mmcam_dbg_log("statfs failed.(%s)", path);
		return -1;
	}

	*file_system_type = (int)fs.f_type;

	return 0;
}


int _mmcamcorder_get_file_size(const char *filename, guint64 *size)
{
	struct stat buf;

	if (stat(filename, &buf) != 0)
		return -1;
	*size = (guint64)buf.st_size;
	return 1;
}


void _mmcamcorder_remove_buffer_probe(MMHandleType handle, _MMCamcorderHandlerCategory category)
{
	mmf_camcorder_t* hcamcorder = MMF_CAMCORDER(handle);
	GList *list = NULL;
	MMCamcorderHandlerItem *item = NULL;

	mmf_return_if_fail(hcamcorder);

	if (!hcamcorder->buffer_probes) {
		_mmcam_dbg_warn("list for buffer probe is NULL");
		return;
	}

	_mmcam_dbg_log("start - category : 0x%x", category);

	list = hcamcorder->buffer_probes;
	while (list) {
		item = list->data;
		if (!item) {
			_mmcam_dbg_err("Remove buffer probe faild, the item is NULL");
			list = g_list_next(list);
			continue;
		}

		if (item->category & category) {
			if (item->object && GST_IS_PAD(item->object)) {
				_mmcam_dbg_log("Remove buffer probe on [%s:%s] - [ID : %lu], [Category : %x]",
				               GST_DEBUG_PAD_NAME(item->object), item->handler_id,  item->category);
				gst_pad_remove_probe(GST_PAD(item->object), item->handler_id);
			} else {
				_mmcam_dbg_warn("Remove buffer probe faild, the pad is null or not pad, just remove item from list and free it");
			}

			list = g_list_next(list);
			hcamcorder->buffer_probes = g_list_remove(hcamcorder->buffer_probes, item);
			SAFE_FREE(item);
		} else {
			_mmcam_dbg_log("Skip item : [ID : %lu], [Category : %x] ", item->handler_id, item->category);
			list = g_list_next(list);
		}
	}

	if (category == _MMCAMCORDER_HANDLER_CATEGORY_ALL) {
		g_list_free(hcamcorder->buffer_probes);
		hcamcorder->buffer_probes = NULL;
	}

	_mmcam_dbg_log("done");

	return;
}


void _mmcamcorder_remove_one_buffer_probe(MMHandleType handle, void *object)
{
	mmf_camcorder_t* hcamcorder = MMF_CAMCORDER(handle);
	GList *list = NULL;
	MMCamcorderHandlerItem *item = NULL;

	mmf_return_if_fail(hcamcorder);

	if (!hcamcorder->buffer_probes) {
		_mmcam_dbg_warn("list for buffer probe is NULL");
		return;
	}

	_mmcam_dbg_log("start - object : %p", object);

	list = hcamcorder->buffer_probes;
	while (list) {
		item = list->data;
		if (!item) {
			_mmcam_dbg_err("Remove buffer probe faild, the item is NULL");
			list = g_list_next(list);
			continue;
		}

		if (item->object && item->object == object) {
			if (GST_IS_PAD(item->object)) {
				_mmcam_dbg_log("Remove buffer probe on [%s:%s] - [ID : %lu], [Category : %x]",
				               GST_DEBUG_PAD_NAME(item->object), item->handler_id,  item->category);
				gst_pad_remove_probe(GST_PAD(item->object), item->handler_id);
			} else {
				_mmcam_dbg_warn("Remove buffer probe faild, the pad is null or not pad, just remove item from list and free it");
			}

			list = g_list_next(list);
			hcamcorder->buffer_probes = g_list_remove(hcamcorder->buffer_probes, item);
			SAFE_FREE(item);

			break;
		} else {
			_mmcam_dbg_log("Skip item : [ID : %lu], [Category : %x] ", item->handler_id, item->category);
			list = g_list_next(list);
		}
	}

	_mmcam_dbg_log("done");

	return;
}


void _mmcamcorder_remove_event_probe(MMHandleType handle, _MMCamcorderHandlerCategory category)
{
	mmf_camcorder_t* hcamcorder = MMF_CAMCORDER(handle);
	GList *list = NULL;
	MMCamcorderHandlerItem *item = NULL;

	mmf_return_if_fail(hcamcorder);

	if (!hcamcorder->event_probes) {
		_mmcam_dbg_warn("list for event probe is NULL");
		return;
	}

	_mmcam_dbg_log("start - category : 0x%x", category);

	list = hcamcorder->event_probes;
	while (list) {
		item = list->data;
		if (!item) {
			_mmcam_dbg_err("Remove event probe faild, the item is NULL");
			list = g_list_next(list);
			continue;
		}

		if (item->category & category) {
			if (item->object && GST_IS_PAD(item->object)) {
				_mmcam_dbg_log("Remove event probe on [%s:%s] - [ID : %lu], [Category : %x]",
				               GST_DEBUG_PAD_NAME(item->object), item->handler_id,  item->category);
				gst_pad_remove_probe(GST_PAD(item->object), item->handler_id);
			} else {
				_mmcam_dbg_warn("Remove event probe faild, the pad is null or not pad, just remove item from list and free it");
			}

			list = g_list_next(list);
			hcamcorder->event_probes = g_list_remove(hcamcorder->event_probes, item);
			SAFE_FREE(item);
		} else {
			_mmcam_dbg_log("Skip item : [ID : %lu], [Category : %x] ", item->handler_id, item->category);
			list = g_list_next(list);
		}
	}

	if (category == _MMCAMCORDER_HANDLER_CATEGORY_ALL) {
		g_list_free(hcamcorder->event_probes);
		hcamcorder->event_probes = NULL;
	}

	_mmcam_dbg_log("done");

	return;
}


void _mmcamcorder_disconnect_signal(MMHandleType handle, _MMCamcorderHandlerCategory category)
{
	mmf_camcorder_t* hcamcorder = MMF_CAMCORDER(handle);
	GList *list = NULL;
	MMCamcorderHandlerItem *item = NULL;

	mmf_return_if_fail(hcamcorder);

	if (!hcamcorder->signals) {
		_mmcam_dbg_warn("list for signal is NULL");
		return;
	}

	_mmcam_dbg_log("start - category : 0x%x", category);

	list = hcamcorder->signals;
	while (list) {
		item = list->data;
		if (!item) {
			_mmcam_dbg_err("Fail to Disconnecting signal, the item is NULL");
			list = g_list_next(list);
			continue;
		}

		if (item->category & category) {
			if (item->object && GST_IS_ELEMENT(item->object)) {
				if (g_signal_handler_is_connected(item->object, item->handler_id)) {
					_mmcam_dbg_log("Disconnect signal from [%s] : [ID : %lu], [Category : %x]",
					               GST_OBJECT_NAME(item->object), item->handler_id,  item->category);
					g_signal_handler_disconnect(item->object, item->handler_id);
				} else {
					_mmcam_dbg_warn("Signal was not connected, cannot disconnect it :  [%s]  [ID : %lu], [Category : %x]",
					                GST_OBJECT_NAME(item->object), item->handler_id,  item->category);
				}
			} else {
				_mmcam_dbg_err("Fail to Disconnecting signal, the element is null or not element, just remove item from list and free it");
			}

			list = g_list_next(list);
			hcamcorder->signals = g_list_remove(hcamcorder->signals, item);
			SAFE_FREE(item);
		} else {
			_mmcam_dbg_log("Skip item : [ID : %lu], [Category : %x] ", item->handler_id, item->category);
			list = g_list_next(list);
		}
	}

	if (category == _MMCAMCORDER_HANDLER_CATEGORY_ALL) {
		g_list_free(hcamcorder->signals);
		hcamcorder->signals = NULL;
	}

	_mmcam_dbg_log("done");

	return;
}


void _mmcamcorder_remove_all_handlers(MMHandleType handle,  _MMCamcorderHandlerCategory category)
{
	mmf_camcorder_t* hcamcorder = MMF_CAMCORDER(handle);

	_mmcam_dbg_log("ENTER");

	if(hcamcorder->signals)
		_mmcamcorder_disconnect_signal((MMHandleType)hcamcorder, category);
	if(hcamcorder->event_probes)
		_mmcamcorder_remove_event_probe((MMHandleType)hcamcorder, category);
	if(hcamcorder->buffer_probes)
		_mmcamcorder_remove_buffer_probe((MMHandleType)hcamcorder, category);

	_mmcam_dbg_log("LEAVE");
}


void _mmcamcorder_element_release_noti(gpointer data, GObject *where_the_object_was)
{
	int i=0;
	_MMCamcorderSubContext *sc = (_MMCamcorderSubContext *)data;

	mmf_return_if_fail(sc);
	mmf_return_if_fail(sc->element);

	for (i = 0 ; i < _MMCAMCORDER_PIPELINE_ELEMENT_NUM ; i++) {
		if (sc->element[i].gst && (G_OBJECT(sc->element[i].gst) == where_the_object_was)) {
			_mmcam_dbg_warn("The element[%d][%p] is finalized",
			                sc->element[i].id, sc->element[i].gst);
			sc->element[i].gst = NULL;
			sc->element[i].id = _MMCAMCORDER_NONE;
			return;
		}
	}

	mmf_return_if_fail(sc->encode_element);

	for (i = 0 ; i < _MMCAMCORDER_ENCODE_PIPELINE_ELEMENT_NUM ; i++) {
		if (sc->encode_element[i].gst && (G_OBJECT(sc->encode_element[i].gst) == where_the_object_was)) {
			_mmcam_dbg_warn("The encode element[%d][%p] is finalized",
			                sc->encode_element[i].id, sc->encode_element[i].gst);
			sc->encode_element[i].gst = NULL;
			sc->encode_element[i].id = _MMCAMCORDER_ENCODE_NONE;
			return;
		}
	}

	_mmcam_dbg_warn("there is no matching element %p", where_the_object_was);

	return;
}


gboolean
_mmcamcroder_msg_callback(void *data)
{
	_MMCamcorderMsgItem *item = (_MMCamcorderMsgItem*)data;
	mmf_camcorder_t *hcamcorder = NULL;
	mmf_return_val_if_fail(item, FALSE);

	g_mutex_lock(&item->lock);

	hcamcorder = MMF_CAMCORDER(item->handle);
	if (hcamcorder == NULL) {
		_mmcam_dbg_warn("msg id:%x, item:%p, handle is NULL", item->id, item);
		goto MSG_CALLBACK_DONE;
	}


	/*_mmcam_dbg_log("msg id:%x, msg_cb:%p, msg_data:%p, item:%p", item->id, hcamcorder->msg_cb, hcamcorder->msg_data, item);*/

	_MMCAMCORDER_LOCK((MMHandleType)hcamcorder);

	/* remove item from msg data */
	if (hcamcorder->msg_data) {
		hcamcorder->msg_data = g_list_remove(hcamcorder->msg_data, item);
	} else {
		_mmcam_dbg_warn("msg_data is NULL but item[%p] will be removed", item);
	}

	_MMCAMCORDER_UNLOCK((MMHandleType)hcamcorder);

	_MMCAMCORDER_LOCK_MESSAGE_CALLBACK(hcamcorder);

	if ((hcamcorder) && (hcamcorder->msg_cb)) {
		hcamcorder->msg_cb(item->id, (MMMessageParamType*)(&(item->param)), hcamcorder->msg_cb_param);
	}

	_MMCAMCORDER_UNLOCK_MESSAGE_CALLBACK(hcamcorder);

MSG_CALLBACK_DONE:
	/* release allocated memory */
	if (item->id == MM_MESSAGE_CAMCORDER_FACE_DETECT_INFO) {
		MMCamFaceDetectInfo *cam_fd_info = (MMCamFaceDetectInfo *)item->param.data;
		if (cam_fd_info) {
			SAFE_FREE(cam_fd_info->face_info);
			free(cam_fd_info);
			cam_fd_info = NULL;
		}

		item->param.data = NULL;
		item->param.size = 0;
	}

	g_mutex_unlock(&item->lock);
	g_mutex_clear(&item->lock);

	free(item);
	item = NULL;

	/* For not being called again */
	return FALSE;
}


gboolean
_mmcamcroder_send_message(MMHandleType handle, _MMCamcorderMsgItem *data)
{
	mmf_camcorder_t* hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderMsgItem *item = NULL;

	mmf_return_val_if_fail(hcamcorder, FALSE);
	mmf_return_val_if_fail(data, FALSE);

	switch (data->id)
	{
		case MM_MESSAGE_CAMCORDER_STATE_CHANGED:
		case MM_MESSAGE_CAMCORDER_STATE_CHANGED_BY_ASM:
			data->param.union_type = MM_MSG_UNION_STATE;
			break;
		case MM_MESSAGE_CAMCORDER_RECORDING_STATUS:
			data->param.union_type = MM_MSG_UNION_RECORDING_STATUS;
			break;
		case MM_MESSAGE_CAMCORDER_FIRMWARE_UPDATE:
			data->param.union_type = MM_MSG_UNION_FIRMWARE;
			break;
		case MM_MESSAGE_CAMCORDER_CURRENT_VOLUME:
			data->param.union_type = MM_MSG_UNION_REC_VOLUME_DB;
			break;
		case MM_MESSAGE_CAMCORDER_TIME_LIMIT:
		case MM_MESSAGE_CAMCORDER_MAX_SIZE:
		case MM_MESSAGE_CAMCORDER_NO_FREE_SPACE:
		case MM_MESSAGE_CAMCORDER_ERROR:
		case MM_MESSAGE_CAMCORDER_FOCUS_CHANGED:
		case MM_MESSAGE_CAMCORDER_CAPTURED:
		case MM_MESSAGE_CAMCORDER_VIDEO_CAPTURED:
		case MM_MESSAGE_CAMCORDER_AUDIO_CAPTURED:
		case MM_MESSAGE_READY_TO_RESUME:
		default:
			data->param.union_type = MM_MSG_UNION_CODE;
			break;
	}

	item = g_malloc(sizeof(_MMCamcorderMsgItem));
	memcpy(item, data, sizeof(_MMCamcorderMsgItem));
	item->handle = handle;
	g_mutex_init(&item->lock);

	_MMCAMCORDER_LOCK(handle);
	hcamcorder->msg_data = g_list_append(hcamcorder->msg_data, item);
//	_mmcam_dbg_log("item[%p]", item);

	/* Use DEFAULT priority */
	g_idle_add_full(G_PRIORITY_DEFAULT, _mmcamcroder_msg_callback, item, NULL);

	_MMCAMCORDER_UNLOCK(handle);

	return TRUE;
}


void
_mmcamcroder_remove_message_all(MMHandleType handle)
{
	mmf_camcorder_t* hcamcorder = MMF_CAMCORDER(handle);
	_MMCamcorderMsgItem *item = NULL;
	gboolean ret = TRUE;
	GList *list = NULL;

	mmf_return_if_fail(hcamcorder);

	_MMCAMCORDER_LOCK(handle);

	if (!hcamcorder->msg_data) {
		_mmcam_dbg_log("No message data is remained.");
	} else {
		list = hcamcorder->msg_data;

		while (list) {
			item = list->data;
			list = g_list_next(list);

			if (!item) {
				_mmcam_dbg_err("Fail to remove message. The item is NULL");
			} else {
				if (g_mutex_trylock(&item->lock)) {
					ret = g_idle_remove_by_data (item);
					_mmcam_dbg_log("Remove item[%p]. ret[%d]", item, ret);

					hcamcorder->msg_data = g_list_remove(hcamcorder->msg_data, item);

					if (ret ) {
						/* release allocated memory */
						if (item->id == MM_MESSAGE_CAMCORDER_FACE_DETECT_INFO) {
							MMCamFaceDetectInfo *cam_fd_info = (MMCamFaceDetectInfo *)item->param.data;
							if (cam_fd_info) {
								SAFE_FREE(cam_fd_info->face_info);
								free(cam_fd_info);
								cam_fd_info = NULL;
							}

							item->param.data = NULL;
							item->param.size = 0;
						}

						g_mutex_unlock(&item->lock);
						g_mutex_clear(&item->lock);
						SAFE_FREE(item);
					} else {
						item->handle = 0;
						_mmcam_dbg_warn("Fail to remove item[%p]", item);

						g_mutex_unlock(&item->lock);
					}
				} else {
					_mmcam_dbg_warn("item lock failed. it's being called...");
				}
			}
		}

		g_list_free(hcamcorder->msg_data);
		hcamcorder->msg_data = NULL;
	}

	/* remove idle function for playing capture sound */
	do {
		ret = g_idle_remove_by_data(hcamcorder);
		_mmcam_dbg_log("remove idle function for playing capture sound. ret[%d]", ret);
	} while (ret);

	_MMCAMCORDER_UNLOCK(handle);

	return;
}


int _mmcamcorder_get_pixel_format(GstCaps *caps)
{
	const GstStructure *structure;
	const char *media_type;
	GstVideoInfo media_info;
	MMPixelFormatType type = 0;
	unsigned int fourcc = 0;
	
	mmf_return_val_if_fail( caps != NULL, MM_PIXEL_FORMAT_INVALID );

	structure = gst_caps_get_structure (caps, 0);
	media_type = gst_structure_get_name (structure);

	if (!strcmp (media_type, "image/jpeg") ) {
		_mmcam_dbg_log("It is jpeg.");
		type = MM_PIXEL_FORMAT_ENCODED;
	} else if (!strcmp (media_type, "video/x-raw") &&
		   gst_video_info_from_caps(&media_info, caps) &&
		   GST_VIDEO_INFO_IS_YUV(&media_info)) {
		_mmcam_dbg_log("It is yuv.");
		fourcc = gst_video_format_to_fourcc(GST_VIDEO_INFO_FORMAT(&media_info));
		type = _mmcamcorder_get_pixtype(fourcc);
	} else if (!strcmp (media_type, "video/x-raw") &&
		   GST_VIDEO_INFO_IS_RGB(&media_info)) {
		_mmcam_dbg_log("It is rgb.");
		type = MM_PIXEL_FORMAT_RGB888;
	} else if (!strcmp (media_type, "video/x-h264")) {
		_mmcam_dbg_log("It is H264");
		type = MM_PIXEL_FORMAT_ENCODED_H264;
	} else {
		_mmcam_dbg_err("Not supported format");
		type = MM_PIXEL_FORMAT_INVALID;
	}
	
	/*_mmcam_dbg_log( "Type [%d]", type );*/

	return type;
}

unsigned int _mmcamcorder_get_fourcc(int pixtype, int codectype, int use_zero_copy_format)
{
	unsigned int fourcc = 0;

	_mmcam_dbg_log("pixtype(%d)", pixtype);

	switch (pixtype) {
	case MM_PIXEL_FORMAT_NV12:
		if (use_zero_copy_format) {
			fourcc = GST_MAKE_FOURCC ('S', 'N', '1', '2');
		} else {
			fourcc = GST_MAKE_FOURCC ('N', 'V', '1', '2');
		}
		break;
	case MM_PIXEL_FORMAT_NV21:
		if (use_zero_copy_format) {
			fourcc = GST_MAKE_FOURCC ('S', 'N', '2', '1');
		} else {
			fourcc = GST_MAKE_FOURCC ('N', 'V', '2', '1');
		}
		break;
	case MM_PIXEL_FORMAT_YUYV:
		if (use_zero_copy_format) {
			fourcc = GST_MAKE_FOURCC ('S', 'U', 'Y', 'V');
		} else {
			fourcc = GST_MAKE_FOURCC ('Y', 'U', 'Y', '2');
		}
		break;
	case MM_PIXEL_FORMAT_UYVY:
		if (use_zero_copy_format) {
			fourcc = GST_MAKE_FOURCC ('S', 'Y', 'V', 'Y');
		} else {
			fourcc = GST_MAKE_FOURCC ('U', 'Y', 'V', 'Y');
		}
		break;
	case MM_PIXEL_FORMAT_I420:
		if (use_zero_copy_format) {
			fourcc = GST_MAKE_FOURCC ('S', '4', '2', '0');
		} else {
			fourcc = GST_MAKE_FOURCC ('I', '4', '2', '0');
		}
		break;
	case MM_PIXEL_FORMAT_YV12:
		fourcc = GST_MAKE_FOURCC ('Y', 'V', '1', '2');
		break;
	case MM_PIXEL_FORMAT_422P:
		fourcc = GST_MAKE_FOURCC ('4', '2', '2', 'P');
		break;
	case MM_PIXEL_FORMAT_RGB565:
		fourcc = GST_MAKE_FOURCC ('R', 'G', 'B', 'P');
		break;
	case MM_PIXEL_FORMAT_RGB888:
		fourcc = GST_MAKE_FOURCC ('R', 'G', 'B', ' ');
		break;
	case MM_PIXEL_FORMAT_ENCODED:
		if (codectype == MM_IMAGE_CODEC_JPEG) {
			fourcc = GST_MAKE_FOURCC ('J', 'P', 'E', 'G');
		} else if (codectype == MM_IMAGE_CODEC_JPEG_SRW) {
			fourcc = GST_MAKE_FOURCC ('J', 'P', 'E', 'G'); /*TODO: JPEG+SamsungRAW format */
		} else if (codectype == MM_IMAGE_CODEC_SRW) {
			fourcc = GST_MAKE_FOURCC ('J', 'P', 'E', 'G'); /*TODO: SamsungRAW format */
		} else if (codectype == MM_IMAGE_CODEC_PNG) {
			fourcc = GST_MAKE_FOURCC ('P', 'N', 'G', ' ');
		} else {
			/* Please let us know what other fourcces are. ex) BMP, GIF?*/
			fourcc = GST_MAKE_FOURCC ('J', 'P', 'E', 'G');
		}
		break;
	case MM_PIXEL_FORMAT_ITLV_JPEG_UYVY:
		fourcc = GST_MAKE_FOURCC('I','T','L','V');
		break;
	case MM_PIXEL_FORMAT_ENCODED_H264:
		fourcc = GST_MAKE_FOURCC('H','2','6','4');
		break;
	default:
		_mmcam_dbg_log("Not proper pixel type[%d]. Set default - I420", pixtype);
		if (use_zero_copy_format) {
			fourcc = GST_MAKE_FOURCC ('S', '4', '2', '0');
		} else {
			fourcc = GST_MAKE_FOURCC ('I', '4', '2', '0');
		}
		break;
	}

	return fourcc;
}


int _mmcamcorder_get_pixtype(unsigned int fourcc)
{
	int pixtype = MM_PIXEL_FORMAT_INVALID;
	char *pfourcc = (char*)&fourcc;
	_mmcam_dbg_log("fourcc(%c%c%c%c)", pfourcc[0], pfourcc[1], pfourcc[2], pfourcc[3]);

	switch (fourcc) {
	case GST_MAKE_FOURCC ('S', 'N', '1', '2'):
	case GST_MAKE_FOURCC ('N', 'V', '1', '2'):
		pixtype = MM_PIXEL_FORMAT_NV12;
		break;
	case GST_MAKE_FOURCC ('S', 'N', '2', '1'):
	case GST_MAKE_FOURCC ('N', 'V', '2', '1'):
		pixtype = MM_PIXEL_FORMAT_NV21;
		break;
	case GST_MAKE_FOURCC ('S', 'U', 'Y', 'V'):
	case GST_MAKE_FOURCC ('Y', 'U', 'Y', 'V'):
	case GST_MAKE_FOURCC ('Y', 'U', 'Y', '2'):
		pixtype = MM_PIXEL_FORMAT_YUYV;
		break;
	case GST_MAKE_FOURCC ('S', 'Y', 'V', 'Y'):
	case GST_MAKE_FOURCC ('U', 'Y', 'V', 'Y'):
		pixtype = MM_PIXEL_FORMAT_UYVY;
		break;
	case GST_MAKE_FOURCC ('S', '4', '2', '0'):
	case GST_MAKE_FOURCC ('I', '4', '2', '0'):
		pixtype = MM_PIXEL_FORMAT_I420;
		break;
	case GST_MAKE_FOURCC ('Y', 'V', '1', '2'):
		pixtype = MM_PIXEL_FORMAT_YV12;
		break;
	case GST_MAKE_FOURCC ('4', '2', '2', 'P'):
		pixtype = MM_PIXEL_FORMAT_422P;
		break;
	case GST_MAKE_FOURCC ('R', 'G', 'B', 'P'):
		pixtype = MM_PIXEL_FORMAT_RGB565;
		break;
	case GST_MAKE_FOURCC ('R', 'G', 'B', '3'):
		pixtype = MM_PIXEL_FORMAT_RGB888;
		break;
	case GST_MAKE_FOURCC ('A', 'R', 'G', 'B'):
	case GST_MAKE_FOURCC ('x', 'R', 'G', 'B'):
		pixtype = MM_PIXEL_FORMAT_ARGB;
		break;
	case GST_MAKE_FOURCC ('B', 'G', 'R', 'A'):
	case GST_MAKE_FOURCC ('B', 'G', 'R', 'x'):
	case GST_MAKE_FOURCC ('S', 'R', '3', '2'):
		pixtype = MM_PIXEL_FORMAT_RGBA;
		break;
	case GST_MAKE_FOURCC ('J', 'P', 'E', 'G'):
	case GST_MAKE_FOURCC ('P', 'N', 'G', ' '):
		pixtype = MM_PIXEL_FORMAT_ENCODED;
		break;
	/*FIXME*/
	case GST_MAKE_FOURCC ('I', 'T', 'L', 'V'):
		pixtype = MM_PIXEL_FORMAT_ITLV_JPEG_UYVY;
		break;
	case GST_MAKE_FOURCC ('H', '2', '6', '4'):
		pixtype = MM_PIXEL_FORMAT_ENCODED_H264;
		break;
	default:
		_mmcam_dbg_log("Not supported fourcc type(%c%c%c%c)",
		               fourcc, fourcc>>8, fourcc>>16, fourcc>>24);
		pixtype = MM_PIXEL_FORMAT_INVALID;
		break;
	}

	return pixtype;
}


gboolean _mmcamcorder_add_elements_to_bin(GstBin *bin, GList *element_list)
{
	GList *local_list = element_list;
	_MMCamcorderGstElement *element = NULL;

	mmf_return_val_if_fail(bin && local_list, FALSE);

	while (local_list) {
		element = (_MMCamcorderGstElement*)local_list->data;
		if (element && element->gst) {
			if (!gst_bin_add(bin, GST_ELEMENT(element->gst))) {
				_mmcam_dbg_err( "Add element [%s] to bin [%s] FAILED",
				                GST_ELEMENT_NAME(GST_ELEMENT(element->gst)),
				                GST_ELEMENT_NAME(GST_ELEMENT(bin)) );
				return FALSE;
			} else {
				_mmcam_dbg_log("Add element [%s] to bin [%s] OK",
				               GST_ELEMENT_NAME(GST_ELEMENT(element->gst)),
				               GST_ELEMENT_NAME(GST_ELEMENT(bin)));
			}
		}
		local_list = local_list->next;
	}

	return TRUE;
}

gboolean _mmcamcorder_link_elements(GList *element_list)
{
	GList                  *local_list  = element_list;
	_MMCamcorderGstElement *element     = NULL;
	_MMCamcorderGstElement *pre_element = NULL;

	mmf_return_val_if_fail(local_list, FALSE);

	pre_element = (_MMCamcorderGstElement*)local_list->data;
	local_list = local_list->next;

	while (local_list) {
		element = (_MMCamcorderGstElement*)local_list->data;
		if (element && element->gst) {
			if (_MM_GST_ELEMENT_LINK(GST_ELEMENT(pre_element->gst), GST_ELEMENT(element->gst))) {
				_mmcam_dbg_log("Link [%s] to [%s] OK",
				               GST_ELEMENT_NAME(GST_ELEMENT(pre_element->gst)),
				               GST_ELEMENT_NAME(GST_ELEMENT(element->gst)));
			} else {
				_mmcam_dbg_err("Link [%s] to [%s] FAILED",
				               GST_ELEMENT_NAME(GST_ELEMENT(pre_element->gst)),
				               GST_ELEMENT_NAME(GST_ELEMENT(element->gst)));
				return FALSE;
			}
		}

		pre_element = element;
		local_list = local_list->next;
	}

	return TRUE;
}


gboolean _mmcamcorder_resize_frame(unsigned char *src_data, unsigned int src_width, unsigned int src_height, unsigned int src_length, int src_format,
                                   unsigned char **dst_data, unsigned int *dst_width, unsigned int *dst_height, unsigned int *dst_length)
{
	int ret = TRUE;
	int mm_ret = MM_ERROR_NONE;
	int input_format = MM_UTIL_IMG_FMT_YUV420;
	unsigned char *dst_tmp_data = NULL;

	if (!src_data || !dst_data || !dst_width || !dst_height || !dst_length) {
		_mmcam_dbg_err("something is NULL %p,%p,%p,%p,%p",
		               src_data, dst_data, dst_width, dst_height, dst_length);
		return FALSE;
	}

	/* set input format for mm-util */
	switch (src_format) {
	case MM_PIXEL_FORMAT_I420:
		input_format = MM_UTIL_IMG_FMT_I420;
		break;
	case MM_PIXEL_FORMAT_YV12:
		input_format = MM_UTIL_IMG_FMT_YUV420;
		break;
	case MM_PIXEL_FORMAT_NV12:
		input_format = MM_UTIL_IMG_FMT_NV12;
		break;
	case MM_PIXEL_FORMAT_YUYV:
		input_format = MM_UTIL_IMG_FMT_YUYV;
		break;
	case MM_PIXEL_FORMAT_UYVY:
		input_format = MM_UTIL_IMG_FMT_UYVY;
		break;
	case MM_PIXEL_FORMAT_RGB888:
		input_format = MM_UTIL_IMG_FMT_RGB888;
		break;
	default:
		_mmcam_dbg_err("NOT supported format", src_format);
		return FALSE;
	}

	_mmcam_dbg_log("src size %dx%d -> dst size %dx%d",
	                src_width, src_height, *dst_width, *dst_height);

	/* get length of resized image */
	mm_ret = mm_util_get_image_size(input_format, *dst_width, *dst_height, dst_length);
	if (mm_ret != MM_ERROR_NONE) {
		GST_ERROR("mm_util_get_image_size failed 0x%x", ret);
		return FALSE;
	}

	_mmcam_dbg_log("dst_length : %d", *dst_length);

	dst_tmp_data = (unsigned char *)malloc(*dst_length);
	if (dst_tmp_data == NULL) {
		_mmcam_dbg_err("failed to alloc dst_thumb_size(size %d)", *dst_length);
		return FALSE;
	}

	mm_ret = mm_util_resize_image(src_data, src_width, src_height, input_format,
	                              dst_tmp_data, dst_width, dst_height);
	if (mm_ret != MM_ERROR_NONE) {
		GST_ERROR("mm_util_resize_image failed 0x%x", ret);
		free(dst_tmp_data);
		return FALSE;
	}

	*dst_data = dst_tmp_data;

	_mmcam_dbg_log("resize done %p, %dx%d", *dst_data, *dst_width, *dst_height);

	return TRUE;
}


gboolean _mmcamcorder_encode_jpeg(void *src_data, unsigned int src_width, unsigned int src_height,
				  int src_format, unsigned int src_length, unsigned int jpeg_quality,
				  void **result_data, unsigned int *result_length, int enc_type)
{
	int ret = 0;
	int i = 0;
	guint32 src_fourcc = 0;
	gboolean do_encode = FALSE;
	jpegenc_parameter enc_param;
	jpegenc_info enc_info;

	_mmcam_dbg_log("START - enc_type [%d]", enc_type);

	mmf_return_val_if_fail(src_data && result_data && result_length, FALSE);

	CLEAR(enc_param);
	CLEAR(enc_info);

	camsrcjpegenc_get_info(&enc_info);

	src_fourcc = _mmcamcorder_get_fourcc(src_format, 0, FALSE);
	camsrcjpegenc_get_src_fmt(src_fourcc, &(enc_param.src_fmt));

	if (enc_param.src_fmt == COLOR_FORMAT_NOT_SUPPORT) {
		_mmcam_dbg_err("Not Supported FOURCC(format:%d)", src_format);
		return FALSE;
	}

	/* check H/W encoder */
	if (enc_info.hw_support && enc_type == JPEG_ENCODER_HARDWARE) {
		_mmcam_dbg_log("check H/W encoder supported format list");
		/* Check supported format */
		for (i = 0 ; i < enc_info.hw_enc.input_fmt_num ; i++) {
			if (enc_param.src_fmt == enc_info.hw_enc.input_fmt_list[i]) {
				do_encode = TRUE;
				break;
			}
		}

		if (do_encode) {
			enc_type = JPEG_ENCODER_HARDWARE;
		}
	}

	/* check S/W encoder */
	if (!do_encode && enc_info.sw_support) {
		_mmcam_dbg_log("check S/W encoder supported format list");
		/* Check supported format */
		for (i = 0 ; i < enc_info.sw_enc.input_fmt_num ; i++) {
			if (enc_param.src_fmt == enc_info.sw_enc.input_fmt_list[i]) {
				do_encode = TRUE;
				break;
			}
		}

		if (do_encode) {
			enc_type = JPEG_ENCODER_SOFTWARE;
		}
	}

	if (do_encode) {
		enc_param.src_data = src_data;
		enc_param.width = src_width;
		enc_param.height = src_height;
		enc_param.src_len = src_length;
		enc_param.jpeg_mode = JPEG_MODE_BASELINE;
		enc_param.jpeg_quality = jpeg_quality;

		_mmcam_dbg_log("%ux%u, size %u, quality %u, type %d",
		               src_width, src_height, src_length,
		               jpeg_quality, enc_type);

		ret = camsrcjpegenc_encode(&enc_info, enc_type, &enc_param );
		if (ret == CAMSRC_JPEGENC_ERROR_NONE) {
			*result_data = enc_param.result_data;
			*result_length = enc_param.result_len;

			_mmcam_dbg_log("JPEG encode length(%d)", *result_length);

			return TRUE;
		} else {
			_mmcam_dbg_err("camsrcjpegenc_encode failed(%x)", ret);
			return FALSE;
		}
	}

	_mmcam_dbg_err("No encoder supports %d format", src_format);

	return FALSE;
}


/* make UYVY smaller as multiple size. ex: 640x480 -> 320x240 or 160x120 ... */
gboolean _mmcamcorder_downscale_UYVYorYUYV(unsigned char *src, unsigned int src_width, unsigned int src_height,
                                           unsigned char **dst, unsigned int dst_width, unsigned int dst_height)
{
	unsigned int i = 0;
	int j = 0;
	int k = 0;
	int src_index = 0;
	int ratio_width = 0;
	int ratio_height = 0;
	int line_base = 0;
	int line_width = 0;
	int jump_width = 0;
	unsigned char *result = NULL;

	if (src == NULL || dst == NULL) {
		_mmcam_dbg_err("src[%p] or dst[%p] is NULL", src, dst);
		return FALSE;
	}

	result = (unsigned char *)malloc((dst_width * dst_height)<<1);
	if (!result) {
		_mmcam_dbg_err("failed to alloc dst data");
		return FALSE;
	}

	ratio_width = src_width / dst_width;
	ratio_height = src_height / dst_height;
	line_width = src_width << 1;
	jump_width = ratio_width << 1;

	_mmcam_dbg_warn("[src %dx%d] [dst %dx%d] [line width %d] [ratio width %d, height %d]",
	                src_width, src_height, dst_width, dst_height,
	                line_width, ratio_width, ratio_height);

	for (i = 0 ; i < src_height ; i += ratio_height) {
		line_base = i * line_width;
		for (j = 0 ; j < line_width ; j += jump_width) {
			src_index = line_base + j;
			result[k++] = src[src_index];
			result[k++] = src[src_index+1];

			j += jump_width;
			src_index = line_base + j;
			if (src_index % 4 == 0) {
				result[k++] = src[src_index+2];
			} else {
				result[k++] = src[src_index];
			}
			result[k++] = src[src_index+1];
		}
	}

	*dst = result;

	_mmcam_dbg_warn("converting done - result %p", result);

	return TRUE;
}

gboolean _mmcamcorder_check_file_path(const gchar *path)
{
	if(strstr(path, "/opt/usr") != NULL) {
		return TRUE;
	}
	return FALSE;
}

static guint16 get_language_code(const char *str)
{
    return (guint16) (((str[0]-0x60) & 0x1F) << 10) + (((str[1]-0x60) & 0x1F) << 5) + ((str[2]-0x60) & 0x1F);
}

static gchar * str_to_utf8(const gchar *str)
{
	return g_convert (str, -1, "UTF-8", "ASCII", NULL, NULL, NULL);
}

static inline gboolean write_tag(FILE *f, const gchar *tag)
{
	while(*tag)
		FPUTC_CHECK(*tag++, f);

	return TRUE;	
}

static inline gboolean write_to_32(FILE *f, guint val)
{
	FPUTC_CHECK(val >> 24, f);
	FPUTC_CHECK(val >> 16, f);
	FPUTC_CHECK(val >> 8, f);
	FPUTC_CHECK(val, f);
	return TRUE;
}

static inline gboolean write_to_16(FILE *f, guint val)
{
	FPUTC_CHECK(val >> 8, f);
	FPUTC_CHECK(val, f);
	return TRUE;	
}

static inline gboolean write_to_24(FILE *f, guint val)
{
	write_to_16(f, val >> 8);
	FPUTC_CHECK(val, f);
	return TRUE;	
}

void *_mmcamcorder_util_task_thread_func(void *data)
{
	int ret = MM_ERROR_NONE;
	mmf_camcorder_t *hcamcorder = (mmf_camcorder_t *)data;

	if (!hcamcorder) {
		_mmcam_dbg_err("handle is NULL");
		return NULL;
	}

	_mmcam_dbg_warn("start thread");

	pthread_mutex_lock(&(hcamcorder->task_thread_lock));

	while (hcamcorder->task_thread_state != _MMCAMCORDER_TASK_THREAD_STATE_EXIT) {
		switch (hcamcorder->task_thread_state) {
		case _MMCAMCORDER_TASK_THREAD_STATE_NONE:
			_mmcam_dbg_warn("wait for task signal");
			pthread_cond_wait(&(hcamcorder->task_thread_cond), &(hcamcorder->task_thread_lock));
			_mmcam_dbg_warn("task signal received : state %d", hcamcorder->task_thread_state);
			break;
		case _MMCAMCORDER_TASK_THREAD_STATE_SOUND_PLAY_START:
			_mmcamcorder_sound_play((MMHandleType)hcamcorder, _MMCAMCORDER_SAMPLE_SOUND_NAME_CAPTURE, FALSE);
			hcamcorder->task_thread_state = _MMCAMCORDER_TASK_THREAD_STATE_NONE;
			break;
		case _MMCAMCORDER_TASK_THREAD_STATE_ENCODE_PIPE_CREATE:
			ret = _mmcamcorder_video_prepare_record((MMHandleType)hcamcorder);

			/* Play record start sound */
			_mmcamcorder_sound_solo_play((MMHandleType)hcamcorder, _MMCAMCORDER_FILEPATH_REC_START_SND, FALSE);

			_mmcam_dbg_log("_mmcamcorder_video_prepare_record return 0x%x", ret);
			hcamcorder->task_thread_state = _MMCAMCORDER_TASK_THREAD_STATE_NONE;
			break;
		default:
			_mmcam_dbg_warn("invalid task thread state %d", hcamcorder->task_thread_state);
			hcamcorder->task_thread_state = _MMCAMCORDER_TASK_THREAD_STATE_EXIT;
			break;
		}
	}

	pthread_mutex_unlock(&(hcamcorder->task_thread_lock));

	_mmcam_dbg_warn("exit thread");

	return NULL;
}
