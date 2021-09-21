// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include <asm/memory.h>
#include <linux/coresight-stm.h>
#include <linux/delay.h>
#include <linux/devfreq.h>
#include <linux/hash.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iommu.h>
#include <linux/iopoll.h>
#include <linux/of.h>
#include <linux/pm_qos.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/soc/qcom/llcc-qcom.h>
#include <linux/qcom_scm.h>
#include <linux/soc/qcom/smem.h>
#include <linux/dma-mapping.h>
#include <linux/reset.h>
#include <soc/qcom/minidump.h>
#include "hfi_packetization.h"
#include "msm_cvp_debug.h"
#include "cvp_core_hfi.h"
#include "cvp_hfi_helper.h"
#include "cvp_hfi_io.h"
#include "msm_cvp_dsp.h"
#include "msm_cvp_clocks.h"
#include "cvp_dump.h"

/*Declare and init the head node of the linked list
for queue va_md dump*/
LIST_HEAD(head_node_hfi_queue);

/*Declare and init the head node of the linked list
 for debug struct va_md dump*/
LIST_HEAD(head_node_dbg_struct);

int md_eva_dump(const char* name, u64 virt, u64 phys, u64 size)
{
	struct md_region md_entry;
	if (msm_minidump_enabled())
	{
		dprintk(CVP_INFO, "Minidump is enabled!\n");

		// add minidump region for EVA-FW image
		strlcpy(md_entry.name, name, sizeof(md_entry.name));
		md_entry.virt_addr = (uintptr_t)virt;
		md_entry.phys_addr = phys;
		md_entry.size = size;
		if (msm_minidump_add_region(&md_entry) < 0)
		{
			dprintk(CVP_ERR, "Failed to add \"%s\" data in \
                        Minidump\n", name);
			return 1;
		}
		else
		{
			dprintk(CVP_INFO,
				"add region success for \"%s\" with virt addr:\
                                0x%x, phy addr: 0x%x, size: %d",
				md_entry.name, md_entry.virt_addr,
				md_entry.phys_addr, md_entry.size);
			return 0;
		}
	}
	else
	{
		dprintk(CVP_ERR, "Minidump is NOT enabled!\n");
		return 1;
	}
}

void cvp_va_md_register(char* name, void* notf_blk_ptr)
{
	int rc = 0;
	struct notifier_block* notf_blk = (struct notifier_block*)notf_blk_ptr;
	rc = qcom_va_md_register(name, notf_blk);
	if (rc) {
		dprintk(CVP_ERR,
			"\"%s\" : qcom_va_md_register failed rc = %d\n",
			name, rc);
	}
	else {
		dprintk(CVP_INFO,
			"\"%s\" : eva_queue qcom_va_md_register success rc = %d\n",
			name, rc);
	}
}

void cvp_register_va_md_region()
{
	if(qcom_va_md_enabled())
	{
		cvp_va_md_register("eva_queues", &eva_hfiq_list_notif_blk);
		cvp_va_md_register("dbg_struct", &eva_struct_list_notif_blk);
	}
	else
		dprintk(CVP_ERR, "VA_Minidump is NOT enabled!\n");
}

void cvp_free_va_md_list(void)
{
	struct eva_va_md_queue *cursor, *temp;

	/* Iterate over each item of the hfi quesue va_md_list and add del the node */
	list_for_each_entry_safe(cursor, temp, &head_node_hfi_queue, list)
	{
		list_del(&cursor->list);
		kfree(cursor);
	}

	/* Iterate over each item of the hfi debug_struct
        va_md_list and add del the node */
	list_for_each_entry_safe(cursor, temp, &head_node_dbg_struct, list)
	{
		list_del(&cursor->list);
		kfree(cursor);
	}
}

void add_va_node_to_list(void *list_head_node, void *buff_va, u32 buff_size,
                        const char *region_name, bool copy)
{
	struct list_head *head_node = (struct list_head *)list_head_node;
	struct eva_va_md_queue *temp_node = NULL;
	/*Creating Node*/
	temp_node = kzalloc(sizeof(struct eva_va_md_queue), GFP_KERNEL);
	if (!temp_node)
	{
		dprintk(CVP_ERR, "Memory allocation failed for list node\n");
		return;
	}

	/*Init the list within the struct*/
	INIT_LIST_HEAD(&temp_node->list);

	/* Store back the data to linked list node data */
	temp_node->va_md_buff = buff_va;
	temp_node->va_md_buff_size = buff_size;
	strlcpy(temp_node->region_name, region_name,
                sizeof(temp_node->region_name));
	temp_node->copy = copy;

	/*Add Node to Linked List*/
	list_add_tail(&temp_node->list, head_node);
	dprintk(CVP_INFO,
			"\"%s\" added to buffer list, vaddr: %px size: 0x%x\n",
			temp_node->region_name, temp_node->va_md_buff,
			temp_node->va_md_buff_size);
}

void add_hfi_queue_to_va_md_list(void *device)
{
	struct cvp_iface_q_info *iface_q;
	struct iris_hfi_device *dev;

	dev = (struct iris_hfi_device*)device;

	/* Adding CPU QUEUES*/
	iface_q = &dev->iface_queues[CVP_IFACEQ_CMDQ_IDX];
	add_va_node_to_list(&head_node_hfi_queue,
                                iface_q->q_array.align_virtual_addr,
                                iface_q->q_array.mem_size,
                                "eva_cmdq_cpu", false);
	iface_q = &dev->iface_queues[CVP_IFACEQ_MSGQ_IDX];
	add_va_node_to_list(&head_node_hfi_queue,
                                iface_q->q_array.align_virtual_addr,
                                iface_q->q_array.mem_size,
                                "eva_msgq_cpu", false);

	/* Adding DSP QUEUES*/
	iface_q = &dev->dsp_iface_queues[CVP_IFACEQ_CMDQ_IDX];
	add_va_node_to_list(&head_node_hfi_queue,
                                iface_q->q_array.align_virtual_addr,
                                iface_q->q_array.mem_size,
                                "eva_cmdq_dsp", false);
	iface_q = &dev->dsp_iface_queues[CVP_IFACEQ_MSGQ_IDX];
	add_va_node_to_list(&head_node_hfi_queue,
                                iface_q->q_array.align_virtual_addr,
                                iface_q->q_array.mem_size,
                                "eva_msgq_dsp", false);
}

void add_queue_header_to_va_md_list(void *device)
{
	struct cvp_iface_q_info *iface_q;
	struct iris_hfi_device *dev;
	struct cvp_hfi_queue_header *queue;

	dev = (struct iris_hfi_device*)device;

	// Add node for cvp_hfi_queue_header: cpucmdQ
	iface_q = &dev->iface_queues[CVP_IFACEQ_CMDQ_IDX];
	queue = (struct cvp_hfi_queue_header *)iface_q->q_hdr;
	add_va_node_to_list(&head_node_dbg_struct,
			queue, sizeof(struct cvp_hfi_queue_header),
			"cvp_hfi_queue_header-cpucmdQ", false);

	// Add node for cvp_hfi_queue_header: cpumsgQ
	iface_q = &dev->iface_queues[CVP_IFACEQ_MSGQ_IDX];
	queue = (struct cvp_hfi_queue_header *)iface_q->q_hdr;
	add_va_node_to_list(&head_node_dbg_struct,
			queue, sizeof(struct cvp_hfi_queue_header),
			"cvp_hfi_queue_header-cpumsgQ", false);

	// Add node for cvp_hfi_queue_header: dspcmdQ
	iface_q = &dev->dsp_iface_queues[CVP_IFACEQ_CMDQ_IDX];
	queue = (struct cvp_hfi_queue_header *)iface_q->q_hdr;
	add_va_node_to_list(&head_node_dbg_struct,
			queue, sizeof(struct cvp_hfi_queue_header),
			"cvp_hfi_queue_header-dspcmdQ", false);

	// Add node for cvp_hfi_queue_header: dspmsgQ
	iface_q = &dev->dsp_iface_queues[CVP_IFACEQ_MSGQ_IDX];
	queue = (struct cvp_hfi_queue_header *)iface_q->q_hdr;
	add_va_node_to_list(&head_node_dbg_struct,
			queue, sizeof(struct cvp_hfi_queue_header),
			"cvp_hfi_queue_header-dspmsgQ", false);
}

int eva_hfiq_list_notif_handler(struct notifier_block *this,
                                unsigned long event, void *ptr)
{
	struct va_md_entry entry;
	struct eva_va_md_queue *cursor, *temp;
	int rc = 0;
	void *temp_data;

	/* Iterate over each item of the list and
        add that data to va_md_entry */
	list_for_each_entry_safe(cursor, temp, &head_node_hfi_queue, list)
	{
		if(cursor->copy)
		{
			//Copying the content to local kzmalloc buffer...
			dprintk(CVP_INFO, "Copying \"%s\"(%d Bytes)\
                                to intermediate buffer\n",
                                cursor->region_name, cursor->va_md_buff_size);
			temp_data = kzalloc(cursor->va_md_buff_size,
                                                        GFP_KERNEL);
			memcpy(temp_data, cursor->va_md_buff,
                                        cursor->va_md_buff_size);

			entry.vaddr = (unsigned long)temp_data;
		}
		else
			entry.vaddr = (unsigned long)cursor->va_md_buff;
		entry.size = cursor->va_md_buff_size;
		strlcpy(entry.owner, cursor->region_name, sizeof(entry.owner));
		entry.cb = NULL;

		if(msm_cvp_minidump_enable)
		{
			rc = qcom_va_md_add_region(&entry);
			if(rc)
				dprintk(CVP_ERR, "Add region \"failed\" for \
                                \"%s\", vaddr: %px size: 0x%x\n", entry.owner,
                                cursor->va_md_buff, entry.size);
			else
				dprintk(CVP_INFO, "Add region \"success\" for \
                                \"%s\", vaddr: %px size: 0x%x\n", entry.owner,
                                cursor->va_md_buff, entry.size);
		}
	}
	return NOTIFY_OK;
}

int eva_struct_list_notif_handler(struct notifier_block *this,
                unsigned long event, void *ptr)
{
	struct va_md_entry entry;
	struct eva_va_md_queue *cursor, *temp;
	int rc = 0;
	void *temp_data;

	/* Iterate over each item of the list
        and add that data to va_md_entry */
	list_for_each_entry_safe(cursor, temp, &head_node_dbg_struct, list)
	{
		if(cursor->copy)
		{
			//Copying the content to local kzmalloc buffer...
			dprintk(CVP_INFO, "Copying \"%s\"(%d Bytes) to \
                                intermediate buffer\n", cursor->region_name,
                                cursor->va_md_buff_size);
			temp_data = kzalloc(cursor->va_md_buff_size,
                                                        GFP_KERNEL);
			memcpy(temp_data, cursor->va_md_buff,
                                cursor->va_md_buff_size);

			entry.vaddr = (unsigned long)temp_data;
		}
		else
			entry.vaddr = (unsigned long)cursor->va_md_buff;
		entry.size = cursor->va_md_buff_size;
		strlcpy(entry.owner, cursor->region_name, sizeof(entry.owner));
		entry.cb = NULL;

		if(msm_cvp_minidump_enable)
		{
			rc = qcom_va_md_add_region(&entry);
			if(rc)
				dprintk(CVP_ERR, "Add region \"failed\" for \
                                        \"%s\", vaddr: %px size: 0x%x\n",
                                        entry.owner, cursor->va_md_buff,
                                        entry.size);
			else
				dprintk(CVP_INFO, "Add region \"success\" for \
                                \"%s\", vaddr: %px size: 0x%x\n", entry.owner,
                                cursor->va_md_buff, entry.size);
		}
	}
	return NOTIFY_OK;
}

struct notifier_block eva_struct_list_notif_blk = {
		.notifier_call = eva_struct_list_notif_handler,
		.priority = INT_MAX-1,
};

struct notifier_block eva_hfiq_list_notif_blk = {
		.notifier_call = eva_hfiq_list_notif_handler,
		.priority = INT_MAX,
};
