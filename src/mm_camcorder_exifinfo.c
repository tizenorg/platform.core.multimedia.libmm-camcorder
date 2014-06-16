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
 
#include "mm_camcorder_exifinfo.h"
#include "mm_camcorder_exifdef.h"

#include <libexif/exif-loader.h>
#include <libexif/exif-utils.h>
#include <libexif/exif-data.h>

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <mm_debug.h>
#include <mm_error.h>
#include <glib.h>

#define MM_EXIFINFO_USE_BINARY_EXIFDATA         1
#define JPEG_MAX_SIZE                           20000000
#define JPEG_THUMBNAIL_MAX_SIZE                 (128*1024)
#define JPEG_DATA_OFFSET                        2
#define EXIF_MARKER_SOI_LENGTH                  2
#define EXIF_MARKER_APP1_LENGTH                 2
#define EXIF_APP1_LENGTH                        2

#if MM_EXIFINFO_USE_BINARY_EXIFDATA
/**
 * Exif Binary Data.
 */
#include <string.h>
#define _EXIF_BIN_SIZE_		((unsigned int)174)
unsigned char g_exif_bin [_EXIF_BIN_SIZE_] = {
   0x45 , 0x78 , 0x69 , 0x66 , 0x00 , 0x00 , 0x49 , 0x49 , 0x2a , 0x00 , 0x08 , 0x00 , 0x00 , 0x00 , 0x05 , 0x00
 , 0x1a , 0x01 , 0x05 , 0x00 , 0x01 , 0x00 , 0x00 , 0x00 , 0x4a , 0x00 , 0x00 , 0x00 , 0x1b , 0x01 , 0x05 , 0x00
 , 0x01 , 0x00 , 0x00 , 0x00 , 0x52 , 0x00 , 0x00 , 0x00 , 0x28 , 0x01 , 0x03 , 0x00 , 0x01 , 0x00 , 0x00 , 0x00
 , 0x02 , 0x00 , 0x00 , 0x00 , 0x13 , 0x02 , 0x03 , 0x00 , 0x01 , 0x00 , 0x00 , 0x00 , 0x01 , 0x00 , 0x00 , 0x00
 , 0x69 , 0x87 , 0x04 , 0x00 , 0x01 , 0x00 , 0x00 , 0x00 , 0x5a , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00
 , 0x48 , 0x00 , 0x00 , 0x00 , 0x01 , 0x00 , 0x00 , 0x00 , 0x48 , 0x00 , 0x00 , 0x00 , 0x01 , 0x00 , 0x00 , 0x00
 , 0x06 , 0x00 , 0x00 , 0x90 , 0x07 , 0x00 , 0x04 , 0x00 , 0x00 , 0x00 , 0x30 , 0x32 , 0x31 , 0x30 , 0x01 , 0x91
 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0xa0 , 0x07 , 0x00 , 0x04 , 0x00
 , 0x00 , 0x00 , 0x30 , 0x31 , 0x30 , 0x30 , 0x01 , 0xa0 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00
 , 0x00 , 0x00 , 0x02 , 0xa0 , 0x04 , 0x00 , 0x01 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x03 , 0xa0
 , 0x04 , 0x00 , 0x01 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00
};
#endif

/**
 * Structure for exif entry.
 */
typedef struct  _mm_exif_entry_t
{
	ExifTag			tag;			/**< exif tag*/
	ExifFormat		format;			/**< exif format*/
	unsigned long	components;		/**< number of components*/
	unsigned char	*data;			/**< data*/
	unsigned int	size;			/**< size of data*/
} mm_exif_entry_type;


/**
 * local functions.
 */
static void
_exif_set_uint16 (int is_motorola, void * out, unsigned short in)
{
	if (is_motorola) {
		((unsigned char *)out)[0] = in & 0x00ff;
		((unsigned char *)out)[1] = in >> 8;
	} else {
		((unsigned char *)out)[0] = in >> 8;
		((unsigned char *)out)[1] = in & 0x00ff;
	}
}


#ifdef _MMCAMCORDER_EXIF_GET_JPEG_MARKER_OFFSET
static unsigned long
_exif_get_jpeg_marker_offset (void *jpeg, int jpeg_size, unsigned short marker)
{
	unsigned char	*p = NULL;
	unsigned char	*src = jpeg;
	int				src_sz = jpeg_size;
	unsigned char	m[2];
	unsigned long	ret;
	int i;

	// mmf_debug (MMF_DEBUG_LOG, "%s()\n", __func__);

	m[0] = marker >> 8;
	m[1] = marker & 0x00FF;

	mmf_debug (MMF_DEBUG_LOG,"[%05d][%s] marker: 0x%02X 0x%02X\n\n", __LINE__, __func__,m[0], m[1]);

	if (*src == 0xff && *(src + 1) == 0xd8) 
	{
		p = src + 2; /* SOI(start of image) */
	} 
	else 
	{
		mmf_debug (MMF_DEBUG_ERROR,"[%05d][%s] invalid JPEG file.\n", __LINE__, __func__);
		return 0UL;
	}

	for (i = 0; i < src_sz - (1 + 2); i++, p++) 
	{
		if (*p == 0xff) 
		{
			/*marker is 0xFFxx*/
			if (*(p + 1) == m[1]) 
			{
				ret = p - src;
				mmf_debug (MMF_DEBUG_LOG,"[%05d][%s]marker offset: %lu %p %p.\n", __LINE__, __func__,ret, (p+1), src);
				return ret;
			}
		} 
	}
	mmf_debug (MMF_DEBUG_ERROR,"[%05d][%s]Marker not found.\n", __LINE__, __func__);
	return 0UL;
}
#endif /* _MMCAMCORDER_EXIF_GET_JPEG_MARKER_OFFSET */


ExifData*
mm_exif_get_exif_data_from_data (mm_exif_info_t *info)
{
	ExifData 		*ed = NULL;

	//mmf_debug (MMF_DEBUG_LOG, "%s()\n", __func__);

	ed = exif_data_new_from_data(info->data, info->size);
	if( ed == NULL )
	{
		mmf_debug (MMF_DEBUG_ERROR,"[%05d][%s]Null exif data. (ed:%p)\n", __LINE__, __func__, ed);
	}

	return ed;
}


ExifData*
mm_exif_get_exif_from_info (mm_exif_info_t *info)
{
	ExifData 		*ed = NULL;
	ExifLoader		*loader = NULL;

	unsigned char	size[2];
	unsigned int	i;

	//mmf_debug (MMF_DEBUG_LOG, "%s()\n", __func__);
	
	/*get ExifData from info*/
	loader = exif_loader_new ();

	size[0] = (unsigned char) (info->size);
	size[1] = (unsigned char) (info->size >> 8);
	exif_loader_write (loader, size, 2);

	for (i = 0; i < info->size && exif_loader_write (loader, info->data + i, 1); i++);

	ed = exif_loader_get_data (loader);
	exif_loader_unref (loader);
	return ed;
}


int
mm_exif_set_exif_to_info (mm_exif_info_t *info, ExifData *exif)
{
	unsigned char	*eb = NULL;
	unsigned int	ebs;

	if (!exif)
	{
		mmf_debug (MMF_DEBUG_ERROR,"[%05d][%s]exif Null\n", __LINE__, __func__);
		return MM_ERROR_CAMCORDER_NOT_INITIALIZED;
	}

	mmf_debug (MMF_DEBUG_LOG,"[%05d][%s]exif(ifd :%p)\n", __LINE__, __func__, exif->ifd);

	if(info->data)
	{
		free (info->data); 
		info->data = NULL; 
		info->size = 0; 
	}

	exif_data_save_data (exif, &eb, &ebs);
	if(eb==NULL)
	{
		mmf_debug (MMF_DEBUG_ERROR,"[%05d][%s]MM_ERROR_CAMCORDER_LOW_MEMORY\n", __LINE__, __func__);
		return MM_ERROR_CAMCORDER_LOW_MEMORY;
	}
	info->data = eb;
	info->size = ebs;
	return MM_ERROR_NONE;
}


int
mm_exif_set_add_entry (ExifData *exif, ExifIfd ifd, ExifTag tag,ExifFormat format,unsigned long components,unsigned char* data)
{
	/*mmf_debug (MMF_DEBUG_LOG, "%s()\n", __func__);*/
	ExifData *ed = (ExifData *)exif;
	ExifEntry *e = NULL;

	if (exif == NULL || format <= 0 || components <= 0 || data == NULL) {
		mmf_debug(MMF_DEBUG_ERROR,"[%05d][%s] invalid argument exif=%p format=%d, components=%lu data=%p!\n",
		                          __LINE__, __func__,exif,format,components,data);
		return MM_ERROR_CAMCORDER_INVALID_ARGUMENT;
	}

	/*remove same tag in EXIF*/
	exif_content_remove_entry(ed->ifd[ifd], exif_content_get_entry(ed->ifd[ifd], tag));

	/*create new tag*/
	e = exif_entry_new();
	if (e == NULL) {
		mmf_debug(MMF_DEBUG_ERROR,"[%05d][%s] entry create error!\n", __LINE__, __func__);
		return MM_ERROR_CAMCORDER_LOW_MEMORY;
	}

	exif_entry_initialize(e, tag);

	e->tag = tag;
	e->format = format;
	e->components = components;

	if (e->size == 0) {
		e->data = NULL;
		e->data = malloc(exif_format_get_size(format) * e->components);
		if (!e->data) {
			exif_entry_unref(e);
			return MM_ERROR_CAMCORDER_LOW_MEMORY;
		}

		if (format == EXIF_FORMAT_ASCII) {
			memset(e->data, '\0', exif_format_get_size(format) * e->components);
		}
	}

	e->size = exif_format_get_size(format) * e->components;
	memcpy(e->data,data,e->size);
	exif_content_add_entry(ed->ifd[ifd], e);
	exif_entry_unref(e);

	return MM_ERROR_NONE;
}




/**
 * global functions.
 */


int
mm_exif_create_exif_info (mm_exif_info_t **info)
{
	mm_exif_info_t *x = NULL;
#if (MM_EXIFINFO_USE_BINARY_EXIFDATA == 0)
	ExifData *ed = NULL;
	unsigned char *eb = NULL;
	unsigned int ebs;
#endif
	mmf_debug (MMF_DEBUG_LOG,"[%05d][%s]\n", __LINE__, __func__);

	if (!info) {
		mmf_debug(MMF_DEBUG_ERROR,"[%05d][%s] NULL pointer\n", __LINE__, __func__);
		return MM_ERROR_CAMCORDER_INVALID_ARGUMENT;
	}

	x = malloc(sizeof(mm_exif_info_t));
	if (!x) {
		mmf_debug(MMF_DEBUG_ERROR,"[%05d][%s]malloc error\n", __LINE__, __func__);
		return MM_ERROR_CAMCORDER_LOW_MEMORY;
	}
#if MM_EXIFINFO_USE_BINARY_EXIFDATA
	x->data = NULL;
	x->data = malloc(_EXIF_BIN_SIZE_);
	if (!x->data) {
		mmf_debug(MMF_DEBUG_ERROR,"[%05d][%s]malloc error\n", __LINE__, __func__);
		free(x);
		return MM_ERROR_CAMCORDER_LOW_MEMORY;
	}
	memcpy(x->data, g_exif_bin, _EXIF_BIN_SIZE_);
	x->size = _EXIF_BIN_SIZE_;
#else
	ed = exif_data_new();
	if (!ed) {
		mmf_debug(MMF_DEBUG_ERROR,"[%05d][%s]exif data new error\n", __LINE__, __func__);
		return MM_ERROR_CAMCORDER_LOW_MEMORY;
	}

	exif_data_set_byte_order(ed, EXIF_BYTE_ORDER_INTEL);
	exif_data_set_data_type(ed, EXIF_DATA_TYPE_COMPRESSED);
	exif_data_set_option(ed, EXIF_DATA_OPTION_FOLLOW_SPECIFICATION);

	exif_data_fix(ed);

	exif_data_save_data(ed, &eb, &ebs);
	if (eb == NULL) {
		mmf_debug(MMF_DEBUG_ERROR,"[%05d][%s]exif_data_save_data error\n", __LINE__, __func__);
		free(x->data);
		free(x);
		exif_data_unref(ed);
		return MM_ERROR_CAMCORDER_INTERNAL;
	}
	exif_data_unref(ed);

	x->data = eb;
	x->size = ebs;
#endif
	*info = x;

	mmf_debug(MMF_DEBUG_LOG, "%s() Data:%p Size:%d\n", __func__, x->data, x->size);

	return MM_ERROR_NONE;
}

void
mm_exif_destory_exif_info (mm_exif_info_t *info)
{
	//mmf_debug (MMF_DEBUG_LOG, "%s()\n", __func__);

#if MM_EXIFINFO_USE_BINARY_EXIFDATA
	if (info) {
		if (info->data) 
			free (info->data);
		free (info);
	}
#else
	if (info) {
		if (info->data) 
			exif_mem_free (info->data);
		free (info);
	}
#endif
}


int
mm_exif_add_thumbnail_info (mm_exif_info_t *info, void *thumbnail, int width, int height, int len)
{
	ExifData *ed = NULL;
	static ExifLong elong[10];

	unsigned char *p_compressed = NULL;
	int ret = MM_ERROR_NONE;
	int cntl = 0;

	mmf_debug (MMF_DEBUG_LOG,"[%05d][%s] Thumbnail size:%d, width:%d, height:%d\n", __LINE__, __func__, len, width, height);

	if( len > JPEG_THUMBNAIL_MAX_SIZE )
	{
		mmf_debug (MMF_DEBUG_ERROR,"[%05d][%s] Thumbnail size[%d] over!!! Skip inserting thumbnail...\n", __LINE__, __func__, len);
		return MM_ERROR_NONE;
	}

	/* get ExifData from info*/
	ed = mm_exif_get_exif_from_info(info);
	ed->data = thumbnail;
	ed->size = len;

	/* set thumbnail data */
	p_compressed = malloc(sizeof(ExifShort));
	if (p_compressed != NULL) {
		exif_set_short(p_compressed, exif_data_get_byte_order(ed), 6);
		ret = mm_exif_set_add_entry(ed, EXIF_IFD_1, EXIF_TAG_COMPRESSION, EXIF_FORMAT_SHORT, 1, p_compressed);
		if (ret != MM_ERROR_NONE) {
			goto exit;
		}
	} else {
		ret = MM_ERROR_CAMCORDER_LOW_MEMORY;
		goto exit;
	}

	/* set thumbnail size */
	exif_set_long ((unsigned char *)&elong[cntl], exif_data_get_byte_order(ed), width);
	ret = mm_exif_set_add_entry(ed, EXIF_IFD_1, EXIF_TAG_IMAGE_WIDTH, EXIF_FORMAT_LONG, 1, (unsigned char*)&elong[cntl++]);
	if (ret != MM_ERROR_NONE) {
		goto exit;
	}
	exif_set_long ((unsigned char *)&elong[cntl], exif_data_get_byte_order(ed), height);
	ret = mm_exif_set_add_entry(ed, EXIF_IFD_1, EXIF_TAG_IMAGE_LENGTH, EXIF_FORMAT_LONG, 1, (unsigned char*)&elong[cntl++]);
	if (ret != MM_ERROR_NONE) {
		goto exit;
	}

	ret = mm_exif_set_exif_to_info (info, ed);
	if (ret != MM_ERROR_NONE) {
		goto exit;
	}

	ed->data = NULL;
	ed->size = 0;
	exif_data_unref (ed);	

exit :
	if(p_compressed != NULL)
		free(p_compressed);
	return ret;
}


int mm_exif_mnote_create (ExifData *exif)
{
	ExifData* ed = exif;
	ExifDataOption o = 0;
	if(!ed){
		mmf_debug (MMF_DEBUG_ERROR,"[%05d][%s] invalid argument exif=%p \n", __LINE__, __func__,ed);
		return MM_ERROR_CAMCORDER_INVALID_ARGUMENT;
	}

	if(!exif_data_mnote_data_new(ed, MAKER_SAMSUNG,  o )){
		mmf_debug (MMF_DEBUG_ERROR,"[%05d][%s] exif_mnote_data_samsung_new() failed. \n", __LINE__, __func__);
		return MM_ERROR_CAMCORDER_MNOTE_CREATION;
	}

	return MM_ERROR_NONE;
}


int mm_exif_mnote_set_add_entry (ExifData *exif, MnoteSamsungTag tag, int index, int subindex1, int subindex2)
{
	ExifData *ed = exif;
	ExifMnoteData *md;

	ExifShort product_id = 32768;	//should be modified
	char serialNum[] = "SerialNum123"; //should be modified

	if(!ed){
		mmf_debug (MMF_DEBUG_ERROR,"[%05d][%s] invalid argument exif=%p \n", __LINE__, __func__,ed);
		return MM_ERROR_CAMCORDER_INVALID_ARGUMENT;
	}

	md = exif_data_get_mnote_data (ed);
	if(!md){
		mmf_debug (MMF_DEBUG_ERROR,"[%05d][%s] exif_data_get_mnote_data() failed. \n", __LINE__, __func__);
		return MM_ERROR_CAMCORDER_MNOTE_CREATION;
	}

	if(!exif_data_mnote_set_mem_for_adding_entry(md, MAKER_SAMSUNG)){
		mmf_debug (MMF_DEBUG_ERROR,"[%05d][%s] exif_data_mnote_set_mem_for_adding_entry() failed. \n", __LINE__, __func__);
		return MM_ERROR_CAMCORDER_MNOTE_MALLOC;
	}

	switch(tag){
		case MNOTE_SAMSUNG_TAG_MNOTE_VERSION:
			if(!exif_data_mnote_set_add_entry(md, MAKER_SAMSUNG, tag, EXIF_FORMAT_UNDEFINED, 4, index)){
				mmf_debug (MMF_DEBUG_ERROR,"[%05d][%s] exif_data_mnote_set_add_entry() failed. \n", __LINE__, __func__);
				return MM_ERROR_CAMCORDER_MNOTE_ADD_ENTRY;
			}
			break;
		case MNOTE_SAMSUNG_TAG_DEVICE_ID:
			if(!exif_data_mnote_set_add_entry(md, MAKER_SAMSUNG, tag, EXIF_FORMAT_LONG, 1, index)){
				mmf_debug (MMF_DEBUG_ERROR,"[%05d][%s] exif_data_mnote_set_add_entry() failed. \n", __LINE__, __func__);
				return MM_ERROR_CAMCORDER_MNOTE_ADD_ENTRY;
			}
			break;
		case MNOTE_SAMSUNG_TAG_MODEL_ID:
			if(!exif_data_mnote_set_add_entry_subtag(md, MAKER_SAMSUNG, tag, EXIF_FORMAT_LONG, 1, MNOTE_SAMSUNG_SUBTAG_MODEL_ID_CLASS, subindex1, MNOTE_SAMSUNG_SUBTAG_MODEL_ID_DEVEL, subindex2, product_id )){
				mmf_debug (MMF_DEBUG_ERROR,"[%05d][%s] exif_data_mnote_set_add_entry_subtag() failed. \n", __LINE__, __func__);
				return MM_ERROR_CAMCORDER_MNOTE_ADD_ENTRY;
			}	
			break;
		case MNOTE_SAMSUNG_TAG_COLOR_INFO:
		case MNOTE_SAMSUNG_TAG_SERIAL_NUM:
			if(!exif_data_mnote_set_add_entry_string(md, MAKER_SAMSUNG, tag, EXIF_FORMAT_ASCII, strlen(serialNum), serialNum)){
				mmf_debug (MMF_DEBUG_ERROR,"[%05d][%s] exif_data_mnote_set_add_entry_string() failed. \n", __LINE__, __func__);
				return MM_ERROR_CAMCORDER_MNOTE_ADD_ENTRY;
			}
			break;
		case MNOTE_SAMSUNG_TAG_IMAGE_COUNT:
		case MNOTE_SAMSUNG_TAG_GPS_INFO01:
		case MNOTE_SAMSUNG_TAG_GPS_INFO02:
		case MNOTE_SAMSUNG_TAG_PREVIEW_IMAGE:
		case MNOTE_SAMSUNG_TAG_FAVOR_TAGGING:
		case MNOTE_SAMSUNG_TAG_SRW_COMPRESS:
		case MNOTE_SAMSUNG_TAG_COLOR_SPACE:
			if(!exif_data_mnote_set_add_entry(md, MAKER_SAMSUNG, tag, EXIF_FORMAT_LONG, 1, index)){
				mmf_debug (MMF_DEBUG_ERROR,"[%05d][%s] exif_data_mnote_set_add_entry() failed. \n", __LINE__, __func__);
				return MM_ERROR_CAMCORDER_MNOTE_ADD_ENTRY;
			}
			break;
		case MNOTE_SAMSUNG_TAG_AE:
		case MNOTE_SAMSUNG_TAG_AF:
		case MNOTE_SAMSUNG_TAG_AWB01:
		case MNOTE_SAMSUNG_TAG_AWB02:
		case MNOTE_SAMSUNG_TAG_IPC:
		case MNOTE_SAMSUNG_TAG_SCENE_RESULT:
		case MNOTE_SAMSUNG_TAG_SADEBUG_INFO01:
		case MNOTE_SAMSUNG_TAG_SADEBUG_INFO02:
		case MNOTE_SAMSUNG_TAG_FACE_DETECTION:
			if(!exif_data_mnote_set_add_entry(md, MAKER_SAMSUNG, tag, EXIF_FORMAT_LONG, 1, index)){
				mmf_debug (MMF_DEBUG_ERROR,"[%05d][%s] exif_data_mnote_set_add_entry() failed. \n", __LINE__, __func__);
				return MM_ERROR_CAMCORDER_MNOTE_ADD_ENTRY;
			}
			break;
		case MNOTE_SAMSUNG_TAG_FACE_FEAT01:
		case MNOTE_SAMSUNG_TAG_FACE_FEAT02:
		case MNOTE_SAMSUNG_TAG_FACE_RECOG:
		case MNOTE_SAMSUNG_TAG_LENS_INFO:
		case MNOTE_SAMSUNG_TAG_THIRDPARTY:
			break;
		default:
			break;
	}
	return MM_ERROR_NONE;
}


int
mm_exif_write_exif_jpeg_to_file (char *filename, mm_exif_info_t *info,  void *jpeg, int jpeg_len)
{
	FILE *fp = NULL;
	unsigned short head[2] = {0,};
	unsigned short head_len = 0;
	unsigned char *eb = NULL;
	unsigned int ebs;

	mmf_debug (MMF_DEBUG_LOG,"[%05d][%s]\n", __LINE__, __func__);

	eb = info->data;
	ebs = info->size;

	/*create file*/
	fp = fopen (filename, "wb");
	if (!fp) {
		mmf_debug (MMF_DEBUG_ERROR, "%s(), fopen() failed [%s].\n", __func__, filename);
		return MM_ERROR_IMAGE_FILEOPEN;
	}

	/*set SOI, APP1*/
	_exif_set_uint16 (0, &head[0], 0xffd8);
	_exif_set_uint16 (0, &head[1], 0xffe1);
	/*set header length*/
	_exif_set_uint16 (0, &head_len, (unsigned short)(ebs + 2));

	if(head[0]==0 || head[1]==0 || head_len==0)
	{
		mmf_debug (MMF_DEBUG_ERROR,"[%05d][%s]setting error\n", __LINE__, __func__);
		fclose (fp);
		return -1;
	}

	fwrite (&head[0], 1, EXIF_MARKER_SOI_LENGTH, fp);       /*SOI marker*/
	fwrite (&head[1], 1, EXIF_MARKER_APP1_LENGTH, fp);      /*APP1 marker*/
	fwrite (&head_len, 1, EXIF_APP1_LENGTH, fp);            /*length of APP1*/
	fwrite (eb, 1, ebs, fp);                                /*EXIF*/
	fwrite (jpeg + JPEG_DATA_OFFSET, 1, jpeg_len - JPEG_DATA_OFFSET, fp);   /*IMAGE*/

	fclose (fp);

	return MM_ERROR_NONE;
}

int
mm_exif_write_exif_jpeg_to_memory (void **mem, unsigned int *length, mm_exif_info_t *info,  void *jpeg, unsigned int jpeg_len)
{
	unsigned short head[2] = {0,};
	unsigned short head_len = 0;
	unsigned char *eb = NULL;
	unsigned int ebs;

	/*output*/
	unsigned char *m = NULL;
	int m_len = 0;

	mmf_debug (MMF_DEBUG_LOG,"[%05d][%s]\n", __LINE__, __func__);

	if(info==NULL || jpeg==NULL)
	{
		mmf_debug (MMF_DEBUG_ERROR, "%s(), MM_ERROR_CAMCORDER_INVALID_ARGUMENT info=%p, jpeg=%p\n", __func__,info,jpeg);
		return MM_ERROR_CAMCORDER_INVALID_ARGUMENT;
	}

	if(jpeg_len>JPEG_MAX_SIZE)
	{
		mmf_debug (MMF_DEBUG_ERROR, "%s(),jpeg_len is worng jpeg_len=%d\n", __func__,jpeg_len);
		return MM_ERROR_CAMCORDER_DEVICE_WRONG_JPEG;	
	}

	eb = info->data;
	ebs = info->size;

	/*length of output image*/
	/*SOI + APP1 + length of APP1 + length of EXIF + IMAGE*/
	m_len = EXIF_MARKER_SOI_LENGTH + EXIF_MARKER_APP1_LENGTH + EXIF_APP1_LENGTH + ebs + (jpeg_len - JPEG_DATA_OFFSET);
	/*alloc output image*/
	m = malloc (m_len);
	if (!m) {
		mmf_debug (MMF_DEBUG_ERROR, "%s(), malloc() failed.\n", __func__);
		return MM_ERROR_CAMCORDER_LOW_MEMORY;	
	}

	/*set SOI, APP1*/
	_exif_set_uint16 (0, &head[0], 0xffd8);
	_exif_set_uint16 (0, &head[1], 0xffe1);
	/*set header length*/
	_exif_set_uint16 (0, &head_len, (unsigned short)(ebs + 2));
	if (head[0] == 0 || head[1] == 0 || head_len == 0) {
		mmf_debug (MMF_DEBUG_ERROR,"[%05d][%s]setting error\n", __LINE__, __func__);
		free(m);
		return MM_ERROR_CAMCORDER_INVALID_ARGUMENT;
	}

	/* Complete JPEG+EXIF */
	/*SOI marker*/
	memcpy(m, &head[0], EXIF_MARKER_SOI_LENGTH);
	/*APP1 marker*/
	memcpy(m + EXIF_MARKER_SOI_LENGTH,
	       &head[1], EXIF_MARKER_APP1_LENGTH);
	/*length of APP1*/
	memcpy(m + EXIF_MARKER_SOI_LENGTH + EXIF_MARKER_APP1_LENGTH,
	       &head_len, EXIF_APP1_LENGTH);
	/*EXIF*/
	memcpy(m + EXIF_MARKER_SOI_LENGTH + EXIF_MARKER_APP1_LENGTH + EXIF_APP1_LENGTH,
	       eb, ebs);
	/*IMAGE*/
	memcpy(m + EXIF_MARKER_SOI_LENGTH + EXIF_MARKER_APP1_LENGTH + EXIF_APP1_LENGTH + ebs,
	       jpeg + JPEG_DATA_OFFSET, jpeg_len - JPEG_DATA_OFFSET);

	mmf_debug(MMF_DEBUG_LOG,"[%05d][%s] JPEG+EXIF Copy DONE(original:%d, copied:%d)\n",
	                        __LINE__, __func__, jpeg_len, jpeg_len - JPEG_DATA_OFFSET);

	/*set ouput param*/
	*mem    = m;
	*length = m_len;

	return MM_ERROR_NONE;
}
