/*
 * Copyright (C) 2001 Sistina Software (UK) Limited.
 *
 * This file is released under the GPL.
 */

#include "disk-rep.h"
#include "dbg_malloc.h"
#include "pool.h"
#include "hash.h"
#include "list.h"
#include "log.h"

#include <time.h>
#include <sys/utsname.h>

static int _check_vg_name(const char *name)
{
	return strlen(name) < NAME_LEN;
}

/*
 * Extracts the last part of a path.
 */
static char *_create_lv_name(struct pool *mem, const char *full_name)
{
	const char *ptr = strrchr(full_name, '/');

	if (!ptr)
		ptr = full_name;
	else
		ptr++;

	return pool_strdup(mem, ptr);
}

static int _fill_lv_array(struct logical_volume **lvs,
			  struct volume_group *vg, struct disk_list *dl)
{
	struct list_head *tmp;
	struct logical_volume *lv;
	int i = 0;

	list_for_each(tmp, &dl->lvs) {
		struct lvd_list *ll = list_entry(tmp, struct lvd_list, list);

		if (!(lv = find_lv(vg, ll->lv.lv_name))) {
			stack;
			return 0;
		}

		lvs[i] = lv;
		i++;
	}

	return 1;
}

int import_pv(struct pool *mem, struct device *dev,
	      struct physical_volume *pv, struct pv_disk *pvd)
{
	memset(pv, 0, sizeof(*pv));
	memcpy(&pv->id, pvd->pv_uuid, ID_LEN);

	pv->dev = dev;
	if (!(pv->vg_name = pool_strdup(mem, pvd->vg_name))) {
		stack;
		return 0;
	}

	if (pvd->pv_status & PV_ACTIVE)
		pv->status |= ACTIVE;

	if (pvd->pv_allocatable)
		pv->status |= ALLOCATED_PV;

	pv->size = pvd->pv_size;
	pv->pe_size = pvd->pe_size;
	pv->pe_start = pvd->pe_start;
	pv->pe_count = pvd->pe_total;
	pv->pe_allocated = pvd->pe_allocated;
	return 1;
}

static int _system_id(char *system_id)
{
	struct utsname uts;

	if (uname(&uts) != 0) {
		log_sys_error("uname", "_system_id");
		return 0;
	}

	sprintf(system_id, "%s%lu", uts.nodename, time(NULL));
	return 1;
}

int export_pv(struct pv_disk *pvd, struct physical_volume *pv)
{
	memset(pvd, 0, sizeof(*pvd));

	pvd->id[0] = 'H';
	pvd->id[1] = 'M';
	pvd->version = 1;

	memcpy(pvd->pv_uuid, pv->id.uuid, ID_LEN);

	if (!_check_vg_name(pv->vg_name)) {
		stack;
		return 0;
	}

	memset(pvd->vg_name, 0, sizeof(pvd->vg_name));

	if (pv->vg_name)
		strncpy(pvd->vg_name, pv->vg_name, sizeof(pvd->vg_name));

	//pvd->pv_major = MAJOR(pv->dev);

	if (pv->status & ACTIVE)
		pvd->pv_status |= PV_ACTIVE;

	if (pv->status & ALLOCATED_PV)
		pvd->pv_allocatable = PV_ALLOCATABLE;

	pvd->pv_size = pv->size;
	pvd->lv_cur = 0;	/* this is set when exporting the lv list */
	pvd->pe_size = pv->pe_size;
	pvd->pe_total = pv->pe_count;
	pvd->pe_allocated = pv->pe_allocated;
	pvd->pe_start = pv->pe_start;

	if (!_system_id(pvd->system_id)) {
		stack;
		return 0;
	}

	return 1;
}

int import_vg(struct pool *mem,
	      struct volume_group *vg, struct disk_list *dl)
{
	struct vg_disk *vgd = &dl->vg;
	memcpy(vg->id.uuid, vgd->vg_uuid, ID_LEN);

	if (!_check_vg_name(dl->pv.vg_name)) {
		stack;
		return 0;
	}

	if (!(vg->name = pool_strdup(mem, dl->pv.vg_name))) {
		stack;
		return 0;
	}

	if (vgd->vg_status & VG_ACTIVE)
		vg->status |= ACTIVE;

	if (vgd->vg_status & VG_EXPORTED)
		vg->status |= EXPORTED_VG;

	if (vgd->vg_status & VG_EXTENDABLE)
		vg->status |= EXTENDABLE_VG;

	if (vgd->vg_access & VG_READ)
		vg->status |= LVM_READ;

	if (vgd->vg_access & VG_WRITE)
		vg->status |= LVM_WRITE;

	if (vgd->vg_access & VG_CLUSTERED)
		vg->status |= CLUSTERED;

	if (vgd->vg_access & VG_SHARED)
		vg->status |= SHARED;

	vg->extent_size = vgd->pe_size;
	vg->extent_count = vgd->pe_total;
	vg->free_count = vgd->pe_total - vgd->pe_allocated;
	vg->max_lv = vgd->lv_max;
	vg->max_pv = vgd->pv_max;
	return 1;
}

int export_vg(struct vg_disk *vgd, struct volume_group *vg)
{
	memset(vgd, 0, sizeof(*vgd));
	memcpy(vgd->vg_uuid, vg->id.uuid, ID_LEN);

	if (vg->status & LVM_READ)
		vgd->vg_access |= VG_READ;

	if (vg->status & LVM_WRITE)
		vgd->vg_access |= VG_WRITE;

	if (vg->status & CLUSTERED)
		vgd->vg_access |= VG_CLUSTERED;

	if (vg->status & SHARED)
		vgd->vg_access |= VG_SHARED;

	if (vg->status & ACTIVE)
		vgd->vg_status |= VG_ACTIVE;

	if (vg->status & EXPORTED_VG)
		vgd->vg_status |= VG_EXPORTED;

	if (vg->status & EXTENDABLE_VG)
		vgd->vg_status |= VG_EXTENDABLE;

	vgd->lv_max = vg->max_lv;
	vgd->lv_cur = vg->lv_count;

	vgd->pv_max = vg->max_pv;
	vgd->pv_cur = vg->pv_count;

	vgd->pe_size = vg->extent_size;
	vgd->pe_total = vg->extent_count;
	vgd->pe_allocated = vg->extent_count - vg->free_count;

	return 1;
}

int import_lv(struct pool *mem, struct logical_volume *lv, struct lv_disk *lvd)
{
	int len;
	memset(&lv->id, 0, sizeof(lv->id));
        if (!(lv->name = _create_lv_name(mem, lvd->lv_name))) {
		stack;
		return 0;
	}

	if (lvd->lv_status & LV_ACTIVE)
		lv->status |= ACTIVE;

	if (lvd->lv_status & LV_SPINDOWN)
		lv->status |= SPINDOWN_LV;

	if (lvd->lv_access & LV_READ)
		lv->status |= LVM_READ;

	if (lvd->lv_access & LV_WRITE)
		lv->status |= LVM_WRITE;

	if (lvd->lv_access & LV_SNAPSHOT)
		lv->status |= SNAPSHOT;

	if (lvd->lv_access & LV_SNAPSHOT_ORG)
		lv->status |= SNAPSHOT_ORG;

	if (lvd->lv_badblock)
		lv->status |= BADBLOCK_ON;

	if (lvd->lv_allocation == LV_STRICT)
		lv->status |= ALLOC_STRICT;
	else
		lv->status |= ALLOC_CONTIGUOUS;

        lv->size = lvd->lv_size;
        lv->le_count = lvd->lv_allocated_le;

	len = sizeof(struct pe_specifier) * lv->le_count;
	if (!(lv->map = pool_alloc(mem, len))) {
		stack;
		return 0;
	}
	memset(lv->map, 0, len);

	return 1;
}

void export_lv(struct lv_disk *lvd, struct volume_group *vg,
	       struct logical_volume *lv, const char *prefix)
{
	memset(lvd, 0, sizeof(*lvd));
	snprintf(lvd->lv_name, sizeof(lvd->lv_name), "%s%s/%s",
		 prefix, vg->name, lv->name);

	/* FIXME: Add 'if' test */
	_check_vg_name(vg->name);
	strcpy(lvd->vg_name, vg->name);

	if (lv->status & LVM_READ)
		lvd->lv_access |= LV_READ;

	if (lv->status & LVM_WRITE)
		lvd->lv_access |= LV_WRITE;

	if (lv->status & SNAPSHOT)
		lvd->lv_access |= LV_SNAPSHOT;

	if (lv->status & SNAPSHOT_ORG)
		lvd->lv_access |= LV_SNAPSHOT_ORG;

	if (lv->status & ACTIVE)
		lvd->lv_status |= LV_ACTIVE;

	if (lv->status & SPINDOWN_LV)
		lvd->lv_status |= LV_SPINDOWN;

        lvd->lv_size = lv->size;
        lvd->lv_allocated_le = lv->le_count;

	if (lv->status & BADBLOCK_ON)
		lvd->lv_badblock = LV_BADBLOCK_ON;

	if (lv->status & ALLOC_STRICT)
		lvd->lv_allocation = LV_STRICT;
	else
		lvd->lv_allocation = LV_CONTIGUOUS;
}

int import_extents(struct pool *mem, struct volume_group *vg,
		   struct list_head *pvs)
{
	struct list_head *tmp;
	struct disk_list *dl;
	struct logical_volume *lv, *lvs[MAX_LV];
	struct physical_volume *pv;
	struct pe_disk *e;
	int i;
	uint32_t lv_num, le;

	list_for_each(tmp, pvs) {
		dl = list_entry(tmp, struct disk_list, list);
		pv = _find_pv(vg, dl->dev);
		e = dl->extents;

		/* build an array of lv's for this pv */
		if (!_fill_lv_array(lvs, vg, dl)) {
			stack;
			return 0;
		}

		for (i = 0; i < dl->pv.pe_total; i++) {
			lv_num = e[i].lv_num;

			if (lv_num == UNMAPPED_EXTENT)
				continue;

			else if(lv_num > dl->pv.lv_cur) {
				log_err("invalid lv in extent map\n");
				return 0;

			} else {
				lv_num--;
				lv = lvs[lv_num];
				le = e[i].le_num;

				lv->map[le].pv = pv;
				lv->map[le].pe = i;
			}
		}
	}

	return 1;
}

int export_extents(struct disk_list *dl, int lv_num,
		   struct logical_volume *lv,
		   struct physical_volume *pv)
{
	struct pe_disk *ped;
	int le;

	for (le = 0; le < lv->le_count; le++) {
		if (lv->map[le].pv == pv) {
			ped = &dl->extents[lv->map[le].pe];
			ped->lv_num = lv_num;
			ped->le_num = le;
		}
	}
	return 1;
}

int import_pvs(struct pool *mem, struct list_head *pvs,
	       struct list_head *results, int *count)
{
	struct list_head *tmp;
	struct disk_list *dl;
	struct pv_list *pvl;

	*count = 0;
	list_for_each(tmp, pvs) {
		dl = list_entry(tmp, struct disk_list, list);
		pvl = pool_alloc(mem, sizeof(*pvl));

		if (!pvl) {
			stack;
			return 0;
		}

		if (!import_pv(mem, dl->dev, &pvl->pv, &dl->pv)) {
			stack;
			return 0;
		}

		list_add(&pvl->list, results);
		(*count)++;
	}

	return 1;
}

static struct logical_volume *_add_lv(struct pool *mem,
				      struct volume_group *vg,
				      struct lv_disk *lvd)
{
	struct lv_list *ll = pool_zalloc(mem, sizeof(*ll));
	struct logical_volume *lv;

	if (!ll) {
		stack;
		return NULL;
	}
	lv = &ll->lv;

	if (!import_lv(mem, &ll->lv, lvd)) {
		stack;
		return NULL;
	}

	list_add(&ll->list, &vg->lvs);
	vg->lv_count++;

	return lv;
}

int import_lvs(struct pool *mem, struct volume_group *vg,
	       struct list_head *pvs)
{
	struct list_head *tmp, *tmp2;
	struct disk_list *dl;
	struct lvd_list *ll;
	struct lv_disk *lvd;

	list_for_each(tmp, pvs) {
		dl = list_entry(tmp, struct disk_list, list);
		list_for_each(tmp2, &dl->lvs) {
			ll = list_entry(tmp2, struct lvd_list, list);
			lvd = &ll->lv;

			if (!find_lv(vg, lvd->lv_name) &&
			    !_add_lv(mem, vg, lvd)) {
				stack;
				return 0;
			}
		}
	}

	return 1;
}

int export_lvs(struct disk_list *dl, struct volume_group *vg,
	       struct physical_volume *pv, const char *prefix)
{
	struct list_head *tmp;
	struct lv_list *ll;
	struct lvd_list *lvdl;
	int lv_num = 1, len;

	/*
	 * setup the pv's extents array
	 */
	len = sizeof(struct pe_disk) * dl->pv.pe_total;
	if (!(dl->extents = pool_alloc(dl->mem, len))) {
		stack;
		return 0;
	}
	memset(dl->extents, 0, len);


	list_for_each(tmp, &vg->lvs) {
		ll = list_entry(tmp, struct lv_list, list);
		if (!(lvdl = pool_alloc(dl->mem, sizeof(*lvdl)))) {
			stack;
			return 0;
		}

		export_lv(&lvdl->lv, vg, &ll->lv, prefix);
		if (!export_extents(dl, lv_num++, &ll->lv, pv)) {
			stack;
			return 0;
		}

		list_add(&lvdl->list, &dl->lvs);
		dl->pv.lv_cur++;
	}
	return 1;
}

int export_uuids(struct disk_list *dl, struct volume_group *vg)
{
	struct list_head *tmp;
	struct uuid_list *ul;
	struct pv_list *pvl;

	list_for_each(tmp, &vg->pvs) {
		pvl = list_entry(tmp, struct pv_list, list);
		if (!(ul = pool_alloc(dl->mem, sizeof(*ul)))) {
			stack;
			return 0;
		}

		memset(ul->uuid, 0, sizeof(ul->uuid));
		memcpy(ul->uuid, pvl->pv.id.uuid, ID_LEN);

		list_add(&ul->list, &dl->uuids);
	}
	return 1;
}

static int _get_lv_number(struct volume_group *vg, const char *name)
{
	/* FIXME: inefficient */
	struct list_head *tmp;
	struct lv_list *ll;
	int r = 0;

	list_for_each (tmp, &vg->lvs) {
		ll = list_entry(tmp, struct lv_list, list);
		if (!strcmp(ll->lv.name, name))
			break;
		r++;
	}

	return r;
}

/*
 * This calculates the nasty pv_number and
 * lv_number fields used by LVM1.  Very
 * inefficient code.
 */
void export_numbers(struct list_head *pvs, struct volume_group *vg)
{
	struct list_head *tmp, *tmp2;
	struct disk_list *dl;
	struct lvd_list *ll;
	int pv_num = 1;

	list_for_each (tmp, pvs) {
		dl = list_entry(tmp, struct disk_list, list);
		dl->pv.pv_number = pv_num++;

		list_for_each (tmp2, &dl->lvs) {
			ll = list_entry(tmp2, struct lvd_list, list);
			ll->lv.lv_number = _get_lv_number(vg, ll->lv.lv_name);
		}
	}
}

/*
 * Calculate vg_disk->pv_act.
 */
void export_pv_act(struct list_head *pvs)
{
	struct list_head *tmp;
	struct disk_list *dl;
	int act = 0;

	list_for_each (tmp, pvs) {
		dl = list_entry(tmp, struct disk_list, list);
		if (dl->pv.pv_status & PV_ACTIVE)
			act++;
	}

	list_for_each (tmp, pvs) {
		dl = list_entry(tmp, struct disk_list, list);
		dl->vg.pv_act = act;
	}
}

int export_vg_number(struct list_head *pvs, const char *vg_name, 
		     struct dev_filter *filter)
{
	struct list_head *tmp;
	struct disk_list *dl;
	int vg_num;

	if (!get_free_vg_number(filter, vg_name, &vg_num)) {
		stack;
		return 0;
	}

	list_for_each (tmp, pvs) {
		dl = list_entry(tmp, struct disk_list, list);
		dl->vg.vg_number = vg_num;
	}

	return 1;
}

