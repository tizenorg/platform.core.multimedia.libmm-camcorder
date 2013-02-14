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

#ifndef __MM_CAMCORDER_UTIL_H__
#define __MM_CAMCORDER_UTIL_H__

/*=======================================================================================
| INCLUDE FILES										|
========================================================================================*/

#ifdef __cplusplus
extern "C" {
#endif

/*=======================================================================================
| GLOBAL DEFINITIONS AND DECLARATIONS FOR CAMCORDER					|
========================================================================================*/

/*=======================================================================================
| MACRO DEFINITIONS									|
========================================================================================*/
#ifndef CLEAR
#define CLEAR(x)            memset (&(x), 0, sizeof (x))
#endif

#define MMCAMCORDER_ADD_BUFFER_PROBE(x_pad, x_category, x_callback, x_hcamcorder) \
do { \
	MMCamcorderHandlerItem *item = NULL; \
	item = (MMCamcorderHandlerItem *)g_malloc(sizeof(MMCamcorderHandlerItem)); \
	if (!item) {\
		_mmcam_dbg_err("Cannot connect buffer probe [malloc fail] \n"); \
	} else if (x_category == 0 || !(x_category & _MMCAMCORDER_HANDLER_CATEGORY_ALL)) { \
		_mmcam_dbg_err("Invalid handler category : %x \n", x_category); \
	} else { \
		item->object = G_OBJECT(x_pad); \
		item->category = x_category; \
		item->handler_id = gst_pad_add_buffer_probe(x_pad, G_CALLBACK(x_callback), x_hcamcorder); \
		x_hcamcorder->buffer_probes = g_list_append(x_hcamcorder->buffer_probes, item); \
		_mmcam_dbg_log("Adding buffer probe on [%s:%s] - [ID : %lu], [Category : %x] ", GST_DEBUG_PAD_NAME(item->object), item->handler_id, item->category); \
	} \
} while (0);

#define MMCAMCORDER_ADD_EVENT_PROBE(x_pad, x_category, x_callback, x_hcamcorder) \
do { \
	MMCamcorderHandlerItem *item = NULL; \
	item = (MMCamcorderHandlerItem *) g_malloc(sizeof(MMCamcorderHandlerItem)); \
	if (!item) { \
		_mmcam_dbg_err("Cannot connect buffer probe [malloc fail] \n"); \
	} \
	else if (x_category == 0 || !(x_category & _MMCAMCORDER_HANDLER_CATEGORY_ALL)) { \
		_mmcam_dbg_err("Invalid handler category : %x \n", x_category); \
	} else { \
		item->object =G_OBJECT(x_pad); \
		item->category = x_category; \
		item->handler_id = gst_pad_add_event_probe(x_pad, G_CALLBACK(x_callback), x_hcamcorder); \
		x_hcamcorder->event_probes = g_list_append(x_hcamcorder->event_probes, item); \
		_mmcam_dbg_log("Adding event probe on [%s:%s] - [ID : %lu], [Category : %x] ", GST_DEBUG_PAD_NAME(item->object), item->handler_id, item->category); \
	} \
} while (0);

#define MMCAMCORDER_ADD_DATA_PROBE(x_pad, x_category, x_callback, x_hcamcorder) \
do { \
	MMCamcorderHandlerItem *item = NULL; \
	item = (MMCamcorderHandlerItem *) g_malloc(sizeof(MMCamcorderHandlerItem)); \
	if (!item) { \
		_mmcam_dbg_err("Cannot connect buffer probe [malloc fail] \n"); \
	} else if (x_category == 0 || !(x_category & _MMCAMCORDER_HANDLER_CATEGORY_ALL)) { \
		_mmcam_dbg_err("Invalid handler category : %x \n", x_category); \
	} else { \
		item->object =G_OBJECT(x_pad); \
		item->category = x_category; \
		item->handler_id = gst_pad_add_data_probe(x_pad, G_CALLBACK(x_callback), x_hcamcorder); \
		x_hcamcorder->data_probes = g_list_append(x_hcamcorder->data_probes, item); \
		_mmcam_dbg_log("Adding data probe on [%s:%s] - [ID : %lu], [Category : %x] ", GST_DEBUG_PAD_NAME(item->object), item->handler_id, item->category); \
	} \
} while (0);

#define MMCAMCORDER_SIGNAL_CONNECT( x_object, x_category, x_signal, x_callback, x_hcamcorder) \
do { \
	MMCamcorderHandlerItem* item = NULL; \
	item = (MMCamcorderHandlerItem *) g_malloc(sizeof(MMCamcorderHandlerItem)); \
	if (!item) { \
		_mmcam_dbg_err("Cannot connect signal [%s]\n", x_signal ); \
	} else if (x_category == 0 || !(x_category & _MMCAMCORDER_HANDLER_CATEGORY_ALL)) { \
		_mmcam_dbg_err("Invalid handler category : %x \n", x_category); \
	} else { \
		item->object = G_OBJECT(x_object); \
		item->category = x_category; \
		item->handler_id = g_signal_connect(G_OBJECT(x_object), x_signal,\
						    G_CALLBACK(x_callback), x_hcamcorder ); \
		x_hcamcorder->signals = g_list_append(x_hcamcorder->signals, item); \
		_mmcam_dbg_log("Connecting signal on [%s] - [ID : %lu], [Category : %x] ", GST_OBJECT_NAME(item->object), item->handler_id, item->category); \
	} \
} while (0);

#define MMCAMCORDER_G_OBJECT_GET(obj, name, value) \
do { \
	if (obj) { \
		if(g_object_class_find_property(G_OBJECT_GET_CLASS(G_OBJECT(obj)), name)) { \
			g_object_get(G_OBJECT(obj), name, value, NULL); \
		} else { \
			_mmcam_dbg_warn ("The object doesn't have a property named(%s)", name); \
		} \
	} else { \
		_mmcam_dbg_err("Null object"); \
	} \
} while(0);

#define MMCAMCORDER_G_OBJECT_SET(obj, name, value) \
do { \
	if (obj) { \
		if(g_object_class_find_property(G_OBJECT_GET_CLASS(G_OBJECT(obj)), name)) { \
			g_object_set(G_OBJECT(obj), name, value, NULL); \
		} else { \
			_mmcam_dbg_warn ("The object doesn't have a property named(%s)", name); \
		} \
	} else { \
		_mmcam_dbg_err("Null object"); \
	} \
} while(0);

#define MMCAM_FOURCC(a,b,c,d)  (guint32)((a)|(b)<<8|(c)<<16|(d)<<24)
#define MMCAM_FOURCC_ARGS(fourcc) \
        ((gchar)((fourcc)&0xff)), \
        ((gchar)(((fourcc)>>8)&0xff)), \
        ((gchar)(((fourcc)>>16)&0xff)), \
        ((gchar)(((fourcc)>>24)&0xff))

#define MMCAM_SEND_MESSAGE(handle, msg_id, msg_code) \
{\
	_MMCamcorderMsgItem msg;\
	msg.id = msg_id;\
	msg.param.code = msg_code;\
	_mmcam_dbg_log("msg id : %x, code : %x", msg_id, msg_code);\
	_mmcamcroder_send_message((MMHandleType)handle, &msg);\
}


/*=======================================================================================
| ENUM DEFINITIONS									|
========================================================================================*/
/**
 *Type define of util.
 */
typedef enum {
	_MMCAMCORDER_HANDLER_PREVIEW = (1 << 0),
	_MMCAMCORDER_HANDLER_VIDEOREC = (1 << 1),
	_MMCAMCORDER_HANDLER_STILLSHOT = (1 << 2),
	_MMCAMCORDER_HANDLER_AUDIOREC = (1 << 3),
} _MMCamcorderHandlerCategory;

/*=======================================================================================
| STRUCTURE DEFINITIONS									|
========================================================================================*/

/**
 * Structure of location info
 */
typedef struct {
	gint32 longitude;
	gint32 latitude;
	gint32 altitude;
} _MMCamcorderLocationInfo;

/**
 * Structure of handler item
 */
typedef struct {
	GObject *object;
	_MMCamcorderHandlerCategory category;
	gulong handler_id;
} MMCamcorderHandlerItem;

/**
 * Structure of message item
 */
typedef struct {
	MMHandleType handle;		/**< handle */
	int id;				/**< message id */
	MMMessageParamType param;	/**< message parameter */
} _MMCamcorderMsgItem;

/**
 * Structure of zero copy image buffer
 */
#define SCMN_IMGB_MAX_PLANE         (4)

/* image buffer definition ***************************************************

    +------------------------------------------+ ---
    |                                          |  ^
    |     a[], p[]                             |  |
    |     +---------------------------+ ---    |  |
    |     |                           |  ^     |  |
    |     |<---------- w[] ---------->|  |     |  |
    |     |                           |  |     |  |
    |     |                           |        |
    |     |                           |  h[]   |  e[]
    |     |                           |        |
    |     |                           |  |     |  |
    |     |                           |  |     |  |
    |     |                           |  v     |  |
    |     +---------------------------+ ---    |  |
    |                                          |  v
    +------------------------------------------+ ---

    |<----------------- s[] ------------------>|
*/

typedef struct
{
	/* width of each image plane */
	int w[SCMN_IMGB_MAX_PLANE];
	/* height of each image plane */
	int h[SCMN_IMGB_MAX_PLANE];
	/* stride of each image plane */
	int s[SCMN_IMGB_MAX_PLANE];
	/* elevation of each image plane */
	int e[SCMN_IMGB_MAX_PLANE];
	/* user space address of each image plane */
	void *a[SCMN_IMGB_MAX_PLANE];
	/* physical address of each image plane, if needs */
	void *p[SCMN_IMGB_MAX_PLANE];
	/* color space type of image */
	int cs;
	/* left postion, if needs */
	int x;
	/* top position, if needs */
	int y;
	/* to align memory */
	int __dummy2;
	/* arbitrary data */
	int data[16];
} SCMN_IMGB;

/*=======================================================================================
| CONSTANT DEFINITIONS									|
========================================================================================*/
#define _MMCAMCORDER_HANDLER_CATEGORY_ALL \
	(_MMCAMCORDER_HANDLER_PREVIEW | _MMCAMCORDER_HANDLER_VIDEOREC |_MMCAMCORDER_HANDLER_STILLSHOT | _MMCAMCORDER_HANDLER_AUDIOREC)

/*=======================================================================================
| GLOBAL FUNCTION PROTOTYPES								|
========================================================================================*/
/* GStreamer */
void _mmcamcorder_remove_buffer_probe(MMHandleType handle, _MMCamcorderHandlerCategory category);
void _mmcamcorder_remove_event_probe(MMHandleType handle, _MMCamcorderHandlerCategory category);
void _mmcamcorder_remove_data_probe(MMHandleType handle, _MMCamcorderHandlerCategory category);
void _mmcamcorder_disconnect_signal(MMHandleType handle, _MMCamcorderHandlerCategory category);
void _mmcamcorder_remove_all_handlers(MMHandleType handle, _MMCamcorderHandlerCategory category);
void _mmcamcorder_element_release_noti(gpointer data, GObject *where_the_object_was);
gboolean _mmcamcorder_add_elements_to_bin(GstBin *bin, GList *element_list);
gboolean _mmcamcorder_link_elements(GList *element_list);

/* Message */
gboolean _mmcamcroder_msg_callback(void *data);
gboolean _mmcamcroder_send_message(MMHandleType handle, _MMCamcorderMsgItem *data);
void _mmcamcroder_remove_message_all(MMHandleType handle);

/* Pixel format */
int _mmcamcorder_get_pixel_format(GstBuffer *buffer);
int _mmcamcorder_get_pixtype(unsigned int fourcc);
unsigned int _mmcamcorder_get_fourcc(int pixtype, int codectype, int use_zero_copy_format);

/* JPEG encode */
gboolean _mmcamcorder_encode_jpeg(void *src_data, unsigned int src_width, unsigned int src_height,
                                  int src_format, unsigned int src_length, unsigned int jpeg_quality,
                                  void **result_data, unsigned int *result_length);
/* resize */
gboolean _mmcamcorder_resize_frame(unsigned char *src_data, int src_width, int src_height, int src_length, int src_format,
                                   unsigned char **dst_data, int *dst_width, int *dst_height, int *dst_length);

/* Recording */
/* find top level tag only, do not use this function for finding sub level tags.
   tag_fourcc is Four-character-code (FOURCC) */
gint _mmcamcorder_find_tag(FILE *f, guint32 tag_fourcc, gboolean do_rewind);
gint32 _mmcamcorder_double_to_fix(gdouble d_number);
gboolean _mmcamcorder_update_size(FILE *f, gint64 prev_pos, gint64 curr_pos);
gboolean _mmcamcorder_write_loci(FILE *f, _MMCamcorderLocationInfo info);
gboolean _mmcamcorder_write_udta(FILE *f, _MMCamcorderLocationInfo info);
guint64 _mmcamcorder_get_container_size(const guchar *size);
gboolean _mmcamcorder_update_composition_matrix(FILE *f, int orientation);

/* File system */
int _mmcamcorder_get_freespace(const gchar *path, guint64 *free_space);
int _mmcamcorder_get_file_size(const char *filename, guint64 *size);

/* Debug */
void _mmcamcorder_err_trace_write(char *str_filename, char *func_name, int line_num, char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* __MM_CAMCORDER_UTIL_H__ */
