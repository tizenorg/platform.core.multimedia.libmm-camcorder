/*
 * libmm-camcorder
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
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

#include "mm_camcorder_internal.h"
#include "mm_camcorder_resource.h"
#include <murphy/common/glib-glue.h>

#define MRP_APP_CLASS_FOR_CAMCORDER   "media"
#define MRP_RESOURCE_TYPE_MANDATORY TRUE
#define MRP_RESOURCE_TYPE_EXCLUSIVE FALSE

enum {
	MRP_RESOURCE_FOR_VIDEO_OVERLAY,
	MRP_RESOURCE_FOR_CAMERA,
	MRP_RESOURCE_MAX,
};
const char* resource_str[MRP_RESOURCE_MAX] = {
    "video_overlay",
    "camera",
};

#define MMCAMCORDER_CHECK_RESOURCE_MANAGER_INSTANCE(x_camcorder_resource_manager) \
do { \
	if (!x_camcorder_resource_manager) { \
		_mmcam_dbg_err("no resource manager instance"); \
		return MM_ERROR_INVALID_ARGUMENT; \
	} \
} while(0);

#define MMCAMCORDER_CHECK_CONNECTION_RESOURCE_MANAGER(x_camcorder_resource_manager) \
do { \
	if (!x_camcorder_resource_manager) { \
		_mmcam_dbg_err("no resource manager instance"); \
		return MM_ERROR_INVALID_ARGUMENT; \
	} else { \
		if (!x_camcorder_resource_manager->is_connected) { \
			_mmcam_dbg_err("not connected to resource server yet"); \
			return MM_ERROR_RESOURCE_NOT_INITIALIZED; \
		} \
	} \
} while(0);

static char *state_to_str(mrp_res_resource_state_t st)
{
	char *state = "unknown";
	switch (st) {
	case MRP_RES_RESOURCE_ACQUIRED:
		state = "acquired";
		break;
	case MRP_RES_RESOURCE_LOST:
		state = "lost";
		break;
	case MRP_RES_RESOURCE_AVAILABLE:
		state = "available";
		break;
	case MRP_RES_RESOURCE_PENDING:
		state = "pending";
		break;
	case MRP_RES_RESOURCE_ABOUT_TO_LOOSE:
		state = "about to loose";
		break;
	}
	return state;
}

static void mrp_state_callback(mrp_res_context_t *context, mrp_res_error_t err, void *user_data)
{
	int i = 0;
	const mrp_res_resource_set_t *rset;
	mrp_res_resource_t *resource;
	mmf_camcorder_t* camcorder = NULL;


	if (err != MRP_RES_ERROR_NONE) {
		_mmcam_dbg_err(" - error message received from Murphy, err(0x%x)", err);
		return;
	}

	camcorder = (mmf_camcorder_t*)user_data;

	mmf_return_if_fail((MMHandleType)camcorder);

	switch (context->state) {
	case MRP_RES_CONNECTED:
		_mmcam_dbg_log(" - connected to Murphy");
		if ((rset = mrp_res_list_resources(context)) != NULL) {
			mrp_res_string_array_t *resource_names;
			resource_names = mrp_res_list_resource_names(rset);
			if (!resource_names) {
				_mmcam_dbg_err(" - no resources available");
				return;
			}
			for (i = 0; i < resource_names->num_strings; i++) {
				resource = mrp_res_get_resource_by_name(rset, resource_names->strings[i]);
				if (resource) {
					_mmcam_dbg_log(" - available resource: %s", resource->name);
				}
			}
			mrp_res_free_string_array(resource_names);
		}
		camcorder->resource_manager.is_connected = TRUE;
		break;
	case MRP_RES_DISCONNECTED:
		_mmcam_dbg_log(" - disconnected from Murphy");
		if (camcorder->resource_manager.rset) {
			mrp_res_delete_resource_set(camcorder->resource_manager.rset);
			camcorder->resource_manager.rset = NULL;
		}
		if (camcorder->resource_manager.context) {
			mrp_res_destroy(camcorder->resource_manager.context);
			camcorder->resource_manager.context = NULL;
			camcorder->resource_manager.is_connected = FALSE;
		}
		break;
	}

	return;
}

static void mrp_rset_state_callback(mrp_res_context_t *cx, const mrp_res_resource_set_t *rs, void *user_data)
{
	int i = 0;
	mmf_camcorder_t *camcorder = (mmf_camcorder_t *)user_data;
	mrp_res_resource_t *res;

	mmf_return_if_fail((MMHandleType)camcorder);

	if (!mrp_res_equal_resource_set(rs, camcorder->resource_manager.rset)) {
		_mmcam_dbg_warn("- resource set(%p) is not same as this camcorder handle's(%p)", rs, camcorder->resource_manager.rset);
		return;
	}

	_mmcam_dbg_log(" - resource set state of camcorder(%p) is changed to [%s]", camcorder, state_to_str(rs->state));
	for (i = 0; i < MRP_RESOURCE_MAX; i++) {
		res = mrp_res_get_resource_by_name(rs, resource_str[i]);
		if (res == NULL) {
			_mmcam_dbg_warn(" -- %s not present in resource set", resource_str[i]);
		} else {
			_mmcam_dbg_log(" -- resource name [%s] -> [%s]", res->name, state_to_str(res->state));
		}
	}

	mrp_res_delete_resource_set(camcorder->resource_manager.rset);
	camcorder->resource_manager.rset = mrp_res_copy_resource_set(rs);

	return;
}


static void mrp_resource_release_cb (mrp_res_context_t *cx, const mrp_res_resource_set_t *rs, void *user_data)
{
	int i = 0;
	int result = MM_ERROR_NONE;
	int current_state = MM_CAMCORDER_STATE_NONE;
	mmf_camcorder_t* camcorder = (mmf_camcorder_t*)user_data;
	mrp_res_resource_t *res;

	mmf_return_if_fail((MMHandleType)camcorder);

	current_state = _mmcamcorder_get_state((MMHandleType)camcorder);
	if (current_state <= MM_CAMCORDER_STATE_NONE ||
	    current_state >= MM_CAMCORDER_STATE_NUM) {
		_mmcam_dbg_err("Abnormal state. Or null handle. (%p, %d)", camcorder, current_state);
		return;
	}

	/* set value to inform a status is changed by asm */
	camcorder->state_change_by_system = _MMCAMCORDER_STATE_CHANGE_BY_ASM;

	_MMCAMCORDER_LOCK_ASM(camcorder);

	if (!mrp_res_equal_resource_set(rs, camcorder->resource_manager.rset)) {
		_mmcam_dbg_warn("- resource set(%p) is not same as this camcorder handle's(%p)", rs, camcorder->resource_manager.rset);
		return;
	}

	_mmcam_dbg_log(" - resource set state of camcorder(%p) is changed to [%s]", camcorder, state_to_str(rs->state));
	for (i = 0; i < MRP_RESOURCE_MAX; i++) {
		res = mrp_res_get_resource_by_name(rs, resource_str[i]);
		if (res == NULL) {
			_mmcam_dbg_warn(" -- %s not present in resource set", resource_str[i]);
		} else {
			_mmcam_dbg_log(" -- resource name [%s] -> [%s]", res->name, state_to_str(res->state));
		}
	}

	/* Stop the camera */
	__mmcamcorder_force_stop(camcorder);

	/* restore value */
	camcorder->state_change_by_system = _MMCAMCORDER_STATE_CHANGE_NORMAL;

	_MMCAMCORDER_UNLOCK_ASM(camcorder);

	return;
}

static int create_rset(MMCamcorderResourceManager *resource_manager)
{
	if (resource_manager->rset) {
		_mmcam_dbg_err(" - resource set was already created\n");
		return MM_ERROR_RESOURCE_INVALID_STATE;
	}

	resource_manager->rset = mrp_res_create_resource_set(resource_manager->context,
				MRP_APP_CLASS_FOR_CAMCORDER,
				mrp_rset_state_callback,
				(void*)resource_manager->user_data);
	if (resource_manager->rset == NULL) {
		_mmcam_dbg_err(" - could not create resource set\n");
		return MM_ERROR_RESOURCE_INTERNAL;
	}

	if (!mrp_res_set_autorelease(TRUE, resource_manager->rset)) {
		_mmcam_dbg_warn(" - could not set autorelease flag!\n");
	}

	return MM_ERROR_NONE;
}

static int include_resource(MMCamcorderResourceManager *resource_manager, const char *resource_name)
{
	mrp_res_resource_t *resource = NULL;
	resource = mrp_res_create_resource(resource_manager->rset,
				resource_name,
				MRP_RESOURCE_TYPE_MANDATORY,
				MRP_RESOURCE_TYPE_EXCLUSIVE);
	if (resource == NULL) {
		_mmcam_dbg_err(" - could not include resource[%s]\n", resource_name);
		return MM_ERROR_RESOURCE_INTERNAL;
	}

	_mmcam_dbg_log(" - include resource[%s]\n", resource_name);

	return MM_ERROR_NONE;
}

static int set_resource_release_cb(MMCamcorderResourceManager *resource_manager)
{
	int ret = MM_ERROR_NONE;
	bool mrp_ret = FALSE;

	if (resource_manager->rset) {
		mrp_ret = mrp_res_set_release_callback(resource_manager->rset, mrp_resource_release_cb, resource_manager->user_data);
		if (!mrp_ret) {
			_mmcam_dbg_err(" - could not set release callback\n");
			ret = MM_ERROR_RESOURCE_INTERNAL;
		}
	} else {
		_mmcam_dbg_err(" - resource set is null\n");
		ret = MM_ERROR_RESOURCE_INVALID_STATE;
	}

	return ret;
}

int _mmcamcorder_resource_manager_init(MMCamcorderResourceManager *resource_manager, void *user_data)
{
	MMCAMCORDER_CHECK_RESOURCE_MANAGER_INSTANCE(resource_manager);

	resource_manager->mloop = mrp_mainloop_glib_get(g_main_loop_new(NULL, TRUE));
	if (resource_manager->mloop) {
		resource_manager->context = mrp_res_create(resource_manager->mloop, mrp_state_callback, user_data);
		if (resource_manager->context == NULL) {
			_mmcam_dbg_err(" - could not get context for resource manager\n");
			mrp_mainloop_destroy(resource_manager->mloop);
			resource_manager->mloop = NULL;
			return MM_ERROR_RESOURCE_INTERNAL;
		}
		resource_manager->user_data = user_data;
	} else {
		_mmcam_dbg_err("- could not get mainloop for resource manager\n");
		return MM_ERROR_RESOURCE_INTERNAL;
	}

	return MM_ERROR_NONE;
}

int _mmcamcorder_resource_manager_prepare(MMCamcorderResourceManager *resource_manager, MMCamcorderResourceType resource_type)
{
	int ret = MM_ERROR_NONE;
	MMCAMCORDER_CHECK_RESOURCE_MANAGER_INSTANCE(resource_manager);
	MMCAMCORDER_CHECK_CONNECTION_RESOURCE_MANAGER(resource_manager);

	if (!resource_manager->rset) {
		ret = create_rset(resource_manager);
	}
	if (ret == MM_ERROR_NONE) {
		switch (resource_type) {
		case RESOURCE_TYPE_VIDEO_OVERLAY:
			ret = include_resource(resource_manager, resource_str[MRP_RESOURCE_FOR_VIDEO_OVERLAY]);
			break;
		case RESOURCE_TYPE_CAMERA:
			ret = include_resource(resource_manager, resource_str[MRP_RESOURCE_FOR_CAMERA]);
			break;
		}
	}

	return ret;
}

int _mmcamcorder_resource_manager_acquire(MMCamcorderResourceManager *resource_manager)
{
	int ret = MM_ERROR_NONE;
	MMCAMCORDER_CHECK_RESOURCE_MANAGER_INSTANCE(resource_manager);
	MMCAMCORDER_CHECK_CONNECTION_RESOURCE_MANAGER(resource_manager);

	if (resource_manager->rset == NULL) {
		_mmcam_dbg_err("- could not acquire resource, resource set is null\n");
		ret = MM_ERROR_RESOURCE_INVALID_STATE;
	} else {
		ret = set_resource_release_cb(resource_manager);
		if (ret) {
			_mmcam_dbg_err("- could not set resource release cb, ret(%d)\n", ret);
			ret = MM_ERROR_RESOURCE_INTERNAL;
		} else {
			ret = mrp_res_acquire_resource_set(resource_manager->rset);
			if (ret) {
				_mmcam_dbg_err("- could not acquire resource, ret(%d)\n", ret);
				ret = MM_ERROR_RESOURCE_INTERNAL;
			}
		}
	}

	return ret;
}

int _mmcamcorder_resource_manager_release(MMCamcorderResourceManager *resource_manager)
{
	int ret = MM_ERROR_NONE;
	MMCAMCORDER_CHECK_RESOURCE_MANAGER_INSTANCE(resource_manager);
	MMCAMCORDER_CHECK_CONNECTION_RESOURCE_MANAGER(resource_manager);

	if (resource_manager->rset == NULL) {
		_mmcam_dbg_err("- could not release resource, resource set is null\n");
		ret = MM_ERROR_RESOURCE_INVALID_STATE;
	} else {
		if (resource_manager->rset->state != MRP_RES_RESOURCE_ACQUIRED) {
			_mmcam_dbg_err("- could not release resource, resource set state is [%s]\n", state_to_str(resource_manager->rset->state));
			ret = MM_ERROR_RESOURCE_INVALID_STATE;
		} else {
			ret = mrp_res_release_resource_set(resource_manager->rset);
			if (ret) {
				_mmcam_dbg_err("- could not release resource, ret(%d)\n", ret);
				ret = MM_ERROR_RESOURCE_INTERNAL;
			}
		}
	}

	return ret;
}

int _mmcamcorder_resource_manager_unprepare(MMCamcorderResourceManager *resource_manager)
{
	int ret = MM_ERROR_NONE;
	MMCAMCORDER_CHECK_RESOURCE_MANAGER_INSTANCE(resource_manager);
	MMCAMCORDER_CHECK_CONNECTION_RESOURCE_MANAGER(resource_manager);

	if (resource_manager->rset == NULL) {
		_mmcam_dbg_err("- could not unprepare for resource_manager, _mmcamcorder_resource_manager_prepare() first\n");
		ret = MM_ERROR_RESOURCE_INVALID_STATE;
	} else {
		mrp_res_delete_resource_set(resource_manager->rset);
		resource_manager->rset = NULL;
	}

	return ret;
}

int _mmcamcorder_resource_manager_deinit(MMCamcorderResourceManager *resource_manager)
{
	MMCAMCORDER_CHECK_RESOURCE_MANAGER_INSTANCE(resource_manager);
	MMCAMCORDER_CHECK_CONNECTION_RESOURCE_MANAGER(resource_manager);

	if (resource_manager->rset) {
		if (resource_manager->rset->state == MRP_RES_RESOURCE_ACQUIRED) {
			if (mrp_res_release_resource_set(resource_manager->rset))
				_mmcam_dbg_err("- could not release resource\n");
		}
		mrp_res_delete_resource_set(resource_manager->rset);
		resource_manager->rset = NULL;
	}
	if (resource_manager->context) {
		mrp_res_destroy(resource_manager->context);
		resource_manager->context = NULL;
	}
	if (resource_manager->mloop) {
		mrp_mainloop_destroy(resource_manager->mloop);
		resource_manager->mloop = NULL;
	}

	return MM_ERROR_NONE;
}
