/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * DOC: contains interface manager public api
 */

#ifndef _WLAN_IF_MGR_ROAM_H_
#define _WLAN_IF_MGR_ROAM_H_

#include "wlan_objmgr_psoc_obj.h"
#include "wlan_objmgr_pdev_obj.h"
#include "wlan_objmgr_vdev_obj.h"
#include "wlan_policy_mgr_api.h"
#include "wlan_if_mgr_roam.h"

/**
 * if_mgr_enable_roaming() - interface manager enable roaming
 * @vdev: vdev object
 * @pdev: pdev object
 * @requestor: RSO disable requestor
 *
 * Interface manager api to enable roaming for all other active vdev id's
 *
 * Context: It should run in thread context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS if_mgr_enable_roaming(struct wlan_objmgr_vdev *vdev,
				 struct wlan_objmgr_pdev *pdev,
				 enum wlan_cm_rso_control_requestor requestor);

/**
 * if_mgr_disable_roaming() - interface manager disable roaming
 * @vdev: vdev object
 * @pdev: pdev object
 * @requestor: RSO disable requestor
 *
 * Interface manager api to disable roaming for all other active vdev id's
 *
 * Context: It should run in thread context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS if_mgr_disable_roaming(struct wlan_objmgr_vdev *vdev,
				  struct wlan_objmgr_pdev *pdev,
				  enum wlan_cm_rso_control_requestor requestor);

#endif
