/* Copyright (C) 2013-2015 Free Software Foundation, Inc.
   Contributed by Jakub Jelinek <jakub@redhat.com>.

   This file is part of the GNU Offloading and Multi Processing Library
   (libgomp).

   Libgomp is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   Libgomp is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
   more details.

   Under Section 7 of GPL version 3, you are granted additional
   permissions described in the GCC Runtime Library Exception, version
   3.1, as published by the Free Software Foundation.

   You should have received a copy of the GNU General Public License and
   a copy of the GCC Runtime Library Exception along with this program;
   see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
   <http://www.gnu.org/licenses/>.  */

/* This file contains the support of offloading.  */

#include "config.h"
#include "libgomp.h"
#include "oacc-plugin.h"
#include "oacc-int.h"
#include "gomp-constants.h"
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef PLUGIN_SUPPORT
#include <dlfcn.h>
#include "plugin-suffix.h"
#endif

static void gomp_target_init (void);

/* The whole initialization code for offloading plugins is only run one.  */
static pthread_once_t gomp_is_initialized = PTHREAD_ONCE_INIT;

/* This structure describes an offload image.
   It contains type of the target device, pointer to host table descriptor, and
   pointer to target data.  */
struct offload_image_descr {
  enum offload_target_type type;
  void *host_table;
  void *target_data;
};

/* Array of descriptors of offload images.  */
static struct offload_image_descr *offload_images;

/* Total number of offload images.  */
static int num_offload_images;

/* Array of descriptors for all available devices.  */
static struct gomp_device_descr *devices;

#ifdef PLUGIN_SUPPORT
/* Total number of available devices.  */
static int num_devices;
#endif

/* Number of GOMP_OFFLOAD_CAP_OPENMP_400 devices.  */
static int num_devices_openmp;

/* The comparison function.  */

attribute_hidden int
splay_compare (splay_tree_key x, splay_tree_key y)
{
  if (x->host_start == x->host_end
      && y->host_start == y->host_end)
    return 0;
  if (x->host_end <= y->host_start)
    return -1;
  if (x->host_start >= y->host_end)
    return 1;
  return 0;
}

#include "splay-tree.h"

attribute_hidden void
gomp_init_targets_once (void)
{
  (void) pthread_once (&gomp_is_initialized, gomp_target_init);
}

attribute_hidden int
gomp_get_num_devices (void)
{
  gomp_init_targets_once ();
  return num_devices_openmp;
}

static struct gomp_device_descr *
resolve_device (int device_id)
{
  if (device_id == GOMP_DEVICE_ICV)
    {
      struct gomp_task_icv *icv = gomp_icv (false);
      device_id = icv->default_device_var;
    }

  if (device_id < 0 || device_id >= gomp_get_num_devices ())
    return NULL;

  return &devices[device_id];
}


/* Handle the case where splay_tree_lookup found oldn for newn.
   Helper function of gomp_map_vars.  */

static inline void
gomp_map_vars_existing (splay_tree_key oldn, splay_tree_key newn,
			unsigned char kind)
{
  if ((kind & GOMP_MAP_FLAG_FORCE)
      || oldn->host_start > newn->host_start
      || oldn->host_end < newn->host_end)
    gomp_fatal ("Trying to map into device [%p..%p) object when "
		"[%p..%p) is already mapped",
		(void *) newn->host_start, (void *) newn->host_end,
		(void *) oldn->host_start, (void *) oldn->host_end);
  oldn->refcount++;
}

static int
get_kind (bool is_openacc, void *kinds, int idx)
{
  return is_openacc ? ((unsigned short *) kinds)[idx]
		    : ((unsigned char *) kinds)[idx];
}

attribute_hidden struct target_mem_desc *
gomp_map_vars (struct gomp_device_descr *devicep, size_t mapnum,
	       void **hostaddrs, void **devaddrs, size_t *sizes, void *kinds,
	       bool is_openacc, bool is_target)
{
  size_t i, tgt_align, tgt_size, not_found_cnt = 0;
  const int rshift = is_openacc ? 8 : 3;
  const int typemask = is_openacc ? 0xff : 0x7;
  struct gomp_memory_mapping *mm = &devicep->mem_map;
  struct splay_tree_key_s cur_node;
  struct target_mem_desc *tgt
    = gomp_malloc (sizeof (*tgt) + sizeof (tgt->list[0]) * mapnum);
  tgt->list_count = mapnum;
  tgt->refcount = 1;
  tgt->device_descr = devicep;
  tgt->mem_map = mm;

  if (mapnum == 0)
    return tgt;

  tgt_align = sizeof (void *);
  tgt_size = 0;
  if (is_target)
    {
      size_t align = 4 * sizeof (void *);
      tgt_align = align;
      tgt_size = mapnum * sizeof (void *);
    }

  gomp_mutex_lock (&mm->lock);

  for (i = 0; i < mapnum; i++)
    {
      int kind = get_kind (is_openacc, kinds, i);
      if (hostaddrs[i] == NULL)
	{
	  tgt->list[i] = NULL;
	  continue;
	}
      cur_node.host_start = (uintptr_t) hostaddrs[i];
      if (!GOMP_MAP_POINTER_P (kind & typemask))
	cur_node.host_end = cur_node.host_start + sizes[i];
      else
	cur_node.host_end = cur_node.host_start + sizeof (void *);
      splay_tree_key n = splay_tree_lookup (&mm->splay_tree, &cur_node);
      if (n)
	{
	  tgt->list[i] = n;
	  gomp_map_vars_existing (n, &cur_node, kind & typemask);
	}
      else
	{
	  tgt->list[i] = NULL;

	  size_t align = (size_t) 1 << (kind >> rshift);
	  not_found_cnt++;
	  if (tgt_align < align)
	    tgt_align = align;
	  tgt_size = (tgt_size + align - 1) & ~(align - 1);
	  tgt_size += cur_node.host_end - cur_node.host_start;
	  if ((kind & typemask) == GOMP_MAP_TO_PSET)
	    {
	      size_t j;
	      for (j = i + 1; j < mapnum; j++)
		if (!GOMP_MAP_POINTER_P (get_kind (is_openacc, kinds, j)
					 & typemask))
		  break;
		else if ((uintptr_t) hostaddrs[j] < cur_node.host_start
			 || ((uintptr_t) hostaddrs[j] + sizeof (void *)
			     > cur_node.host_end))
		  break;
		else
		  {
		    tgt->list[j] = NULL;
		    i++;
		  }
	    }
	}
    }

  if (devaddrs)
    {
      if (mapnum != 1)
        gomp_fatal ("unexpected aggregation");
      tgt->to_free = devaddrs[0];
      tgt->tgt_start = (uintptr_t) tgt->to_free;
      tgt->tgt_end = tgt->tgt_start + sizes[0];
    }
  else if (not_found_cnt || is_target)
    {
      /* Allocate tgt_align aligned tgt_size block of memory.  */
      /* FIXME: Perhaps change interface to allocate properly aligned
	 memory.  */
      tgt->to_free = devicep->alloc_func (devicep->target_id,
					  tgt_size + tgt_align - 1);
      tgt->tgt_start = (uintptr_t) tgt->to_free;
      tgt->tgt_start = (tgt->tgt_start + tgt_align - 1) & ~(tgt_align - 1);
      tgt->tgt_end = tgt->tgt_start + tgt_size;
    }
  else
    {
      tgt->to_free = NULL;
      tgt->tgt_start = 0;
      tgt->tgt_end = 0;
    }

  tgt_size = 0;
  if (is_target)
    tgt_size = mapnum * sizeof (void *);

  tgt->array = NULL;
  if (not_found_cnt)
    {
      tgt->array = gomp_malloc (not_found_cnt * sizeof (*tgt->array));
      splay_tree_node array = tgt->array;
      size_t j;

      for (i = 0; i < mapnum; i++)
	if (tgt->list[i] == NULL)
	  {
	    int kind = get_kind (is_openacc, kinds, i);
	    if (hostaddrs[i] == NULL)
	      continue;
	    splay_tree_key k = &array->key;
	    k->host_start = (uintptr_t) hostaddrs[i];
	    if (!GOMP_MAP_POINTER_P (kind & typemask))
	      k->host_end = k->host_start + sizes[i];
	    else
	      k->host_end = k->host_start + sizeof (void *);
	    splay_tree_key n = splay_tree_lookup (&mm->splay_tree, k);
	    if (n)
	      {
		tgt->list[i] = n;
		gomp_map_vars_existing (n, k, kind & typemask);
	      }
	    else
	      {
		size_t align = (size_t) 1 << (kind >> rshift);
		tgt->list[i] = k;
		tgt_size = (tgt_size + align - 1) & ~(align - 1);
		k->tgt = tgt;
		k->tgt_offset = tgt_size;
		tgt_size += k->host_end - k->host_start;
		k->copy_from = GOMP_MAP_COPY_FROM_P (kind & typemask);
		k->refcount = 1;
		k->async_refcount = 0;
		tgt->refcount++;
		array->left = NULL;
		array->right = NULL;
		splay_tree_insert (&mm->splay_tree, array);
		switch (kind & typemask)
		  {
		  case GOMP_MAP_ALLOC:
		  case GOMP_MAP_FROM:
		  case GOMP_MAP_FORCE_ALLOC:
		  case GOMP_MAP_FORCE_FROM:
		    break;
		  case GOMP_MAP_TO:
		  case GOMP_MAP_TOFROM:
		  case GOMP_MAP_FORCE_TO:
		  case GOMP_MAP_FORCE_TOFROM:
		    /* FIXME: Perhaps add some smarts, like if copying
		       several adjacent fields from host to target, use some
		       host buffer to avoid sending each var individually.  */
		    devicep->host2dev_func (devicep->target_id,
					    (void *) (tgt->tgt_start
						      + k->tgt_offset),
					    (void *) k->host_start,
					    k->host_end - k->host_start);
		    break;
		  case GOMP_MAP_POINTER:
		    cur_node.host_start
		      = (uintptr_t) *(void **) k->host_start;
		    if (cur_node.host_start == (uintptr_t) NULL)
		      {
			cur_node.tgt_offset = (uintptr_t) NULL;
			/* FIXME: see above FIXME comment.  */
			devicep->host2dev_func (devicep->target_id,
						(void *) (tgt->tgt_start
							  + k->tgt_offset),
						(void *) &cur_node.tgt_offset,
						sizeof (void *));
			break;
		      }
		    /* Add bias to the pointer value.  */
		    cur_node.host_start += sizes[i];
		    cur_node.host_end = cur_node.host_start + 1;
		    n = splay_tree_lookup (&mm->splay_tree, &cur_node);
		    if (n == NULL)
		      {
			/* Could be possibly zero size array section.  */
			cur_node.host_end--;
			n = splay_tree_lookup (&mm->splay_tree, &cur_node);
			if (n == NULL)
			  {
			    cur_node.host_start--;
			    n = splay_tree_lookup (&mm->splay_tree, &cur_node);
			    cur_node.host_start++;
			  }
		      }
		    if (n == NULL)
		      gomp_fatal ("Pointer target of array section "
				  "wasn't mapped");
		    cur_node.host_start -= n->host_start;
		    cur_node.tgt_offset = n->tgt->tgt_start + n->tgt_offset
					  + cur_node.host_start;
		    /* At this point tgt_offset is target address of the
		       array section.  Now subtract bias to get what we want
		       to initialize the pointer with.  */
		    cur_node.tgt_offset -= sizes[i];
		    /* FIXME: see above FIXME comment.  */
		    devicep->host2dev_func (devicep->target_id,
					    (void *) (tgt->tgt_start
						      + k->tgt_offset),
					    (void *) &cur_node.tgt_offset,
					    sizeof (void *));
		    break;
		  case GOMP_MAP_TO_PSET:
		    /* FIXME: see above FIXME comment.  */
		    devicep->host2dev_func (devicep->target_id,
					    (void *) (tgt->tgt_start
						      + k->tgt_offset),
					    (void *) k->host_start,
					    k->host_end - k->host_start);

		    for (j = i + 1; j < mapnum; j++)
		      if (!GOMP_MAP_POINTER_P (get_kind (is_openacc, kinds, j)
					       & typemask))
			break;
		      else if ((uintptr_t) hostaddrs[j] < k->host_start
			       || ((uintptr_t) hostaddrs[j] + sizeof (void *)
				   > k->host_end))
			break;
		      else
			{
			  tgt->list[j] = k;
			  k->refcount++;
			  cur_node.host_start
			    = (uintptr_t) *(void **) hostaddrs[j];
			  if (cur_node.host_start == (uintptr_t) NULL)
			    {
			      cur_node.tgt_offset = (uintptr_t) NULL;
			      /* FIXME: see above FIXME comment.  */
			      devicep->host2dev_func (devicep->target_id,
				 (void *) (tgt->tgt_start + k->tgt_offset
					   + ((uintptr_t) hostaddrs[j]
					      - k->host_start)),
				 (void *) &cur_node.tgt_offset,
				 sizeof (void *));
			      i++;
			      continue;
			    }
			  /* Add bias to the pointer value.  */
			  cur_node.host_start += sizes[j];
			  cur_node.host_end = cur_node.host_start + 1;
			  n = splay_tree_lookup (&mm->splay_tree, &cur_node);
			  if (n == NULL)
			    {
			      /* Could be possibly zero size array section.  */
			      cur_node.host_end--;
			      n = splay_tree_lookup (&mm->splay_tree,
						     &cur_node);
			      if (n == NULL)
				{
				  cur_node.host_start--;
				  n = splay_tree_lookup (&mm->splay_tree,
							 &cur_node);
				  cur_node.host_start++;
				}
			    }
			  if (n == NULL)
			    gomp_fatal ("Pointer target of array section "
					"wasn't mapped");
			  cur_node.host_start -= n->host_start;
			  cur_node.tgt_offset = n->tgt->tgt_start
						+ n->tgt_offset
						+ cur_node.host_start;
			  /* At this point tgt_offset is target address of the
			     array section.  Now subtract bias to get what we
			     want to initialize the pointer with.  */
			  cur_node.tgt_offset -= sizes[j];
			  /* FIXME: see above FIXME comment.  */
			  devicep->host2dev_func (devicep->target_id,
			     (void *) (tgt->tgt_start + k->tgt_offset
				       + ((uintptr_t) hostaddrs[j]
					  - k->host_start)),
			     (void *) &cur_node.tgt_offset,
			     sizeof (void *));
			  i++;
			}
		    break;
		  case GOMP_MAP_FORCE_PRESENT:
		    {
		      /* We already looked up the memory region above and it
			 was missing.  */
		      size_t size = k->host_end - k->host_start;
		      gomp_fatal ("present clause: !acc_is_present (%p, "
				  "%zd (0x%zx))", (void *) k->host_start,
				  size, size);
		    }
		    break;
		  case GOMP_MAP_FORCE_DEVICEPTR:
		    assert (k->host_end - k->host_start == sizeof (void *));

		    devicep->host2dev_func (devicep->target_id,
					    (void *) (tgt->tgt_start
						      + k->tgt_offset),
					    (void *) k->host_start,
					    sizeof (void *));
		    break;
		  default:
		    gomp_fatal ("%s: unhandled kind 0x%.2x", __FUNCTION__,
				kind);
		  }
		array++;
	      }
	  }
    }

  if (is_target)
    {
      for (i = 0; i < mapnum; i++)
	{
	  if (tgt->list[i] == NULL)
	    cur_node.tgt_offset = (uintptr_t) NULL;
	  else
	    cur_node.tgt_offset = tgt->list[i]->tgt->tgt_start
				  + tgt->list[i]->tgt_offset;
	  /* FIXME: see above FIXME comment.  */
	  devicep->host2dev_func (devicep->target_id,
				  (void *) (tgt->tgt_start
					    + i * sizeof (void *)),
				  (void *) &cur_node.tgt_offset,
				  sizeof (void *));
	}
    }

  gomp_mutex_unlock (&mm->lock);
  return tgt;
}

static void
gomp_unmap_tgt (struct target_mem_desc *tgt)
{
  /* Deallocate on target the tgt->tgt_start .. tgt->tgt_end region.  */
  if (tgt->tgt_end)
    tgt->device_descr->free_func (tgt->device_descr->target_id, tgt->to_free);

  free (tgt->array);
  free (tgt);
}

/* Decrease the refcount for a set of mapped variables, and queue asychronous
   copies from the device back to the host after any work that has been issued.
   Because the regions are still "live", increment an asynchronous reference
   count to indicate that they should not be unmapped from host-side data
   structures until the asynchronous copy has completed.  */

attribute_hidden void
gomp_copy_from_async (struct target_mem_desc *tgt)
{
  struct gomp_device_descr *devicep = tgt->device_descr;
  struct gomp_memory_mapping *mm = tgt->mem_map;
  size_t i;

  gomp_mutex_lock (&mm->lock);

  for (i = 0; i < tgt->list_count; i++)
    if (tgt->list[i] == NULL)
      ;
    else if (tgt->list[i]->refcount > 1)
      {
	tgt->list[i]->refcount--;
	tgt->list[i]->async_refcount++;
      }
    else
      {
	splay_tree_key k = tgt->list[i];
	if (k->copy_from)
	  devicep->dev2host_func (devicep->target_id, (void *) k->host_start,
				  (void *) (k->tgt->tgt_start + k->tgt_offset),
				  k->host_end - k->host_start);
      }

  gomp_mutex_unlock (&mm->lock);
}

/* Unmap variables described by TGT.  If DO_COPYFROM is true, copy relevant
   variables back from device to host: if it is false, it is assumed that this
   has been done already, i.e. by gomp_copy_from_async above.  */

attribute_hidden void
gomp_unmap_vars (struct target_mem_desc *tgt, bool do_copyfrom)
{
  struct gomp_device_descr *devicep = tgt->device_descr;
  struct gomp_memory_mapping *mm = tgt->mem_map;

  if (tgt->list_count == 0)
    {
      free (tgt);
      return;
    }

  gomp_mutex_lock (&mm->lock);

  size_t i;
  for (i = 0; i < tgt->list_count; i++)
    if (tgt->list[i] == NULL)
      ;
    else if (tgt->list[i]->refcount > 1)
      tgt->list[i]->refcount--;
    else if (tgt->list[i]->async_refcount > 0)
      tgt->list[i]->async_refcount--;
    else
      {
	splay_tree_key k = tgt->list[i];
	if (k->copy_from && do_copyfrom)
	  devicep->dev2host_func (devicep->target_id, (void *) k->host_start,
				  (void *) (k->tgt->tgt_start + k->tgt_offset),
				  k->host_end - k->host_start);
	splay_tree_remove (&mm->splay_tree, k);
	if (k->tgt->refcount > 1)
	  k->tgt->refcount--;
	else
	  gomp_unmap_tgt (k->tgt);
      }

  if (tgt->refcount > 1)
    tgt->refcount--;
  else
    gomp_unmap_tgt (tgt);

  gomp_mutex_unlock (&mm->lock);
}

static void
gomp_update (struct gomp_device_descr *devicep, struct gomp_memory_mapping *mm,
	     size_t mapnum, void **hostaddrs, size_t *sizes, void *kinds,
	     bool is_openacc)
{
  size_t i;
  struct splay_tree_key_s cur_node;
  const int typemask = is_openacc ? 0xff : 0x7;

  if (!devicep)
    return;

  if (mapnum == 0)
    return;

  gomp_mutex_lock (&mm->lock);
  for (i = 0; i < mapnum; i++)
    if (sizes[i])
      {
	cur_node.host_start = (uintptr_t) hostaddrs[i];
	cur_node.host_end = cur_node.host_start + sizes[i];
	splay_tree_key n = splay_tree_lookup (&mm->splay_tree,
					      &cur_node);
	if (n)
	  {
	    int kind = get_kind (is_openacc, kinds, i);
	    if (n->host_start > cur_node.host_start
		|| n->host_end < cur_node.host_end)
	      gomp_fatal ("Trying to update [%p..%p) object when"
			  "only [%p..%p) is mapped",
			  (void *) cur_node.host_start,
			  (void *) cur_node.host_end,
			  (void *) n->host_start,
			  (void *) n->host_end);
	    if (GOMP_MAP_COPY_TO_P (kind & typemask))
	      devicep->host2dev_func (devicep->target_id,
				      (void *) (n->tgt->tgt_start
						+ n->tgt_offset
						+ cur_node.host_start
						- n->host_start),
				      (void *) cur_node.host_start,
				      cur_node.host_end - cur_node.host_start);
	    if (GOMP_MAP_COPY_FROM_P (kind & typemask))
	      devicep->dev2host_func (devicep->target_id,
				      (void *) cur_node.host_start,
				      (void *) (n->tgt->tgt_start
						+ n->tgt_offset
						+ cur_node.host_start
						- n->host_start),
				      cur_node.host_end - cur_node.host_start);
	  }
	else
	  gomp_fatal ("Trying to update [%p..%p) object that is not mapped",
		      (void *) cur_node.host_start,
		      (void *) cur_node.host_end);
      }
  gomp_mutex_unlock (&mm->lock);
}

/* This function should be called from every offload image.
   It gets the descriptor of the host func and var tables HOST_TABLE, TYPE of
   the target, and TARGET_DATA needed by target plugin.  */

void
GOMP_offload_register (void *host_table, enum offload_target_type target_type,
		       void *target_data)
{
  offload_images = gomp_realloc (offload_images,
				 (num_offload_images + 1)
				 * sizeof (struct offload_image_descr));

  offload_images[num_offload_images].type = target_type;
  offload_images[num_offload_images].host_table = host_table;
  offload_images[num_offload_images].target_data = target_data;

  num_offload_images++;
}

/* This function initializes the target device, specified by DEVICEP.  DEVICEP
   must be locked on entry, and remains locked on return.  */

attribute_hidden void
gomp_init_device (struct gomp_device_descr *devicep)
{
  devicep->init_device_func (devicep->target_id);
  devicep->is_initialized = true;
}

/* Initialize address mapping tables.  MM must be locked on entry, and remains
   locked on return.  */

attribute_hidden void
gomp_init_tables (struct gomp_device_descr *devicep,
		  struct gomp_memory_mapping *mm)
{
  /* Get address mapping table for device.  */
  struct mapping_table *table = NULL;
  int num_entries = devicep->get_table_func (devicep->target_id, &table);

  /* Insert host-target address mapping into dev_splay_tree.  */
  int i;
  for (i = 0; i < num_entries; i++)
    {
      struct target_mem_desc *tgt = gomp_malloc (sizeof (*tgt));
      tgt->refcount = 1;
      tgt->array = gomp_malloc (sizeof (*tgt->array));
      tgt->tgt_start = table[i].tgt_start;
      tgt->tgt_end = table[i].tgt_end;
      tgt->to_free = NULL;
      tgt->list_count = 0;
      tgt->device_descr = devicep;
      splay_tree_node node = tgt->array;
      splay_tree_key k = &node->key;
      k->host_start = table[i].host_start;
      k->host_end = table[i].host_end;
      k->tgt_offset = 0;
      k->refcount = 1;
      k->copy_from = false;
      k->tgt = tgt;
      node->left = NULL;
      node->right = NULL;
      splay_tree_insert (&mm->splay_tree, node);
    }

  free (table);
  mm->is_initialized = true;
}

/* Free address mapping tables.  MM must be locked on entry, and remains locked
   on return.  */

attribute_hidden void
gomp_free_memmap (struct gomp_memory_mapping *mm)
{
  while (mm->splay_tree.root)
    {
      struct target_mem_desc *tgt = mm->splay_tree.root->key.tgt;

      splay_tree_remove (&mm->splay_tree, &mm->splay_tree.root->key);
      free (tgt->array);
      free (tgt);
    }

  mm->is_initialized = false;
}

/* This function de-initializes the target device, specified by DEVICEP.
   DEVICEP must be locked on entry, and remains locked on return.  */

attribute_hidden void
gomp_fini_device (struct gomp_device_descr *devicep)
{
  if (devicep->is_initialized)
    devicep->fini_device_func (devicep->target_id);

  devicep->is_initialized = false;
}

/* Called when encountering a target directive.  If DEVICE
   is GOMP_DEVICE_ICV, it means use device-var ICV.  If it is
   GOMP_DEVICE_HOST_FALLBACK (or any value
   larger than last available hw device), use host fallback.
   FN is address of host code, UNUSED is part of the current ABI, but
   we're not actually using it.  HOSTADDRS, SIZES and KINDS are arrays
   with MAPNUM entries, with addresses of the host objects,
   sizes of the host objects (resp. for pointer kind pointer bias
   and assumed sizeof (void *) size) and kinds.  */

void
GOMP_target (int device, void (*fn) (void *), const void *unused,
	     size_t mapnum, void **hostaddrs, size_t *sizes,
	     unsigned char *kinds)
{
  struct gomp_device_descr *devicep = resolve_device (device);

  if (devicep == NULL
      || !(devicep->capabilities & GOMP_OFFLOAD_CAP_OPENMP_400))
    {
      /* Host fallback.  */
      struct gomp_thread old_thr, *thr = gomp_thread ();
      old_thr = *thr;
      memset (thr, '\0', sizeof (*thr));
      if (gomp_places_list)
	{
	  thr->place = old_thr.place;
	  thr->ts.place_partition_len = gomp_places_list_len;
	}
      fn (hostaddrs);
      gomp_free_thread (thr);
      *thr = old_thr;
      return;
    }

  gomp_mutex_lock (&devicep->lock);
  if (!devicep->is_initialized)
    gomp_init_device (devicep);
  gomp_mutex_unlock (&devicep->lock);

  void *fn_addr;

  if (devicep->capabilities & GOMP_OFFLOAD_CAP_NATIVE_EXEC)
    fn_addr = (void *) fn;
  else
    {
      struct gomp_memory_mapping *mm = &devicep->mem_map;
      gomp_mutex_lock (&mm->lock);

      if (!mm->is_initialized)
	gomp_init_tables (devicep, mm);

      struct splay_tree_key_s k;
      k.host_start = (uintptr_t) fn;
      k.host_end = k.host_start + 1;
      splay_tree_key tgt_fn = splay_tree_lookup (&mm->splay_tree, &k);
      if (tgt_fn == NULL)
	gomp_fatal ("Target function wasn't mapped");

      gomp_mutex_unlock (&mm->lock);

      fn_addr = (void *) tgt_fn->tgt->tgt_start;
    }

  struct target_mem_desc *tgt_vars
    = gomp_map_vars (devicep, mapnum, hostaddrs, NULL, sizes, kinds, false,
		     true);
  struct gomp_thread old_thr, *thr = gomp_thread ();
  old_thr = *thr;
  memset (thr, '\0', sizeof (*thr));
  if (gomp_places_list)
    {
      thr->place = old_thr.place;
      thr->ts.place_partition_len = gomp_places_list_len;
    }
  devicep->run_func (devicep->target_id, fn_addr, (void *) tgt_vars->tgt_start);
  gomp_free_thread (thr);
  *thr = old_thr;
  gomp_unmap_vars (tgt_vars, true);
}

void
GOMP_target_data (int device, const void *unused, size_t mapnum,
		  void **hostaddrs, size_t *sizes, unsigned char *kinds)
{
  struct gomp_device_descr *devicep = resolve_device (device);

  if (devicep == NULL
      || !(devicep->capabilities & GOMP_OFFLOAD_CAP_OPENMP_400))
    {
      /* Host fallback.  */
      struct gomp_task_icv *icv = gomp_icv (false);
      if (icv->target_data)
	{
	  /* Even when doing a host fallback, if there are any active
	     #pragma omp target data constructs, need to remember the
	     new #pragma omp target data, otherwise GOMP_target_end_data
	     would get out of sync.  */
	  struct target_mem_desc *tgt
	    = gomp_map_vars (NULL, 0, NULL, NULL, NULL, NULL, false, false);
	  tgt->prev = icv->target_data;
	  icv->target_data = tgt;
	}
      return;
    }

  gomp_mutex_lock (&devicep->lock);
  if (!devicep->is_initialized)
    gomp_init_device (devicep);
  gomp_mutex_unlock (&devicep->lock);

  struct gomp_memory_mapping *mm = &devicep->mem_map;
  gomp_mutex_lock (&mm->lock);
  if (!mm->is_initialized)
    gomp_init_tables (devicep, mm);
  gomp_mutex_unlock (&mm->lock);

  struct target_mem_desc *tgt
    = gomp_map_vars (devicep, mapnum, hostaddrs, NULL, sizes, kinds, false,
		     false);
  struct gomp_task_icv *icv = gomp_icv (true);
  tgt->prev = icv->target_data;
  icv->target_data = tgt;
}

void
GOMP_target_end_data (void)
{
  struct gomp_task_icv *icv = gomp_icv (false);
  if (icv->target_data)
    {
      struct target_mem_desc *tgt = icv->target_data;
      icv->target_data = tgt->prev;
      gomp_unmap_vars (tgt, true);
    }
}

void
GOMP_target_update (int device, const void *unused, size_t mapnum,
		    void **hostaddrs, size_t *sizes, unsigned char *kinds)
{
  struct gomp_device_descr *devicep = resolve_device (device);

  if (devicep == NULL
      || !(devicep->capabilities & GOMP_OFFLOAD_CAP_OPENMP_400))
    return;

  gomp_mutex_lock (&devicep->lock);
  if (!devicep->is_initialized)
    gomp_init_device (devicep);
  gomp_mutex_unlock (&devicep->lock);

  struct gomp_memory_mapping *mm = &devicep->mem_map;
  gomp_mutex_lock (&mm->lock);
  if (!mm->is_initialized)
    gomp_init_tables (devicep, mm);
  gomp_mutex_unlock (&mm->lock);

  gomp_update (devicep, mm, mapnum, hostaddrs, sizes, kinds, false);
}

void
GOMP_teams (unsigned int num_teams, unsigned int thread_limit)
{
  if (thread_limit)
    {
      struct gomp_task_icv *icv = gomp_icv (true);
      icv->thread_limit_var
	= thread_limit > INT_MAX ? UINT_MAX : thread_limit;
    }
  (void) num_teams;
}

#ifdef PLUGIN_SUPPORT

/* This function tries to load a plugin for DEVICE.  Name of plugin is passed
   in PLUGIN_NAME.
   The handles of the found functions are stored in the corresponding fields
   of DEVICE.  The function returns TRUE on success and FALSE otherwise.  */

static bool
gomp_load_plugin_for_device (struct gomp_device_descr *device,
			     const char *plugin_name)
{
  const char *err = NULL, *last_missing = NULL;
  int optional_present, optional_total;

  /* Clear any existing error.  */
  dlerror ();

  void *plugin_handle = dlopen (plugin_name, RTLD_LAZY);
  if (!plugin_handle)
    {
      err = dlerror ();
      goto out;
    }

  /* Check if all required functions are available in the plugin and store
     their handlers.  */
#define DLSYM(f)							\
  do									\
    {									\
      device->f##_func = dlsym (plugin_handle, "GOMP_OFFLOAD_" #f);	\
      err = dlerror ();							\
      if (err != NULL)							\
	goto out;							\
    }									\
  while (0)
  /* Similar, but missing functions are not an error.  */
#define DLSYM_OPT(f, n)						\
  do									\
    {									\
      const char *tmp_err;							\
      device->f##_func = dlsym (plugin_handle, "GOMP_OFFLOAD_" #n);	\
      tmp_err = dlerror ();						\
      if (tmp_err == NULL)						\
        optional_present++;						\
      else								\
        last_missing = #n;						\
      optional_total++;							\
    }									\
  while (0)

  DLSYM (get_name);
  DLSYM (get_caps);
  DLSYM (get_type);
  DLSYM (get_num_devices);
  DLSYM (register_image);
  DLSYM (init_device);
  DLSYM (fini_device);
  DLSYM (get_table);
  DLSYM (alloc);
  DLSYM (free);
  DLSYM (dev2host);
  DLSYM (host2dev);
  device->capabilities = device->get_caps_func ();
  if (device->capabilities & GOMP_OFFLOAD_CAP_OPENMP_400)
    DLSYM (run);
  if (device->capabilities & GOMP_OFFLOAD_CAP_OPENACC_200)
    {
      optional_present = optional_total = 0;
      DLSYM_OPT (openacc.exec, openacc_parallel);
      DLSYM_OPT (openacc.open_device, openacc_open_device);
      DLSYM_OPT (openacc.close_device, openacc_close_device);
      DLSYM_OPT (openacc.get_device_num, openacc_get_device_num);
      DLSYM_OPT (openacc.set_device_num, openacc_set_device_num);
      DLSYM_OPT (openacc.register_async_cleanup,
		 openacc_register_async_cleanup);
      DLSYM_OPT (openacc.async_test, openacc_async_test);
      DLSYM_OPT (openacc.async_test_all, openacc_async_test_all);
      DLSYM_OPT (openacc.async_wait, openacc_async_wait);
      DLSYM_OPT (openacc.async_wait_async, openacc_async_wait_async);
      DLSYM_OPT (openacc.async_wait_all, openacc_async_wait_all);
      DLSYM_OPT (openacc.async_wait_all_async, openacc_async_wait_all_async);
      DLSYM_OPT (openacc.async_set_async, openacc_async_set_async);
      DLSYM_OPT (openacc.create_thread_data, openacc_create_thread_data);
      DLSYM_OPT (openacc.destroy_thread_data, openacc_destroy_thread_data);
      /* Require all the OpenACC handlers if we have
	 GOMP_OFFLOAD_CAP_OPENACC_200.  */
      if (optional_present != optional_total)
	{
	  err = "plugin missing OpenACC handler function";
	  goto out;
	}
      optional_present = optional_total = 0;
      DLSYM_OPT (openacc.cuda.get_current_device,
		 openacc_get_current_cuda_device);
      DLSYM_OPT (openacc.cuda.get_current_context,
		 openacc_get_current_cuda_context);
      DLSYM_OPT (openacc.cuda.get_stream, openacc_get_cuda_stream);
      DLSYM_OPT (openacc.cuda.set_stream, openacc_set_cuda_stream);
      /* Make sure all the CUDA functions are there if any of them are.  */
      if (optional_present && optional_present != optional_total)
	{
	  err = "plugin missing OpenACC CUDA handler function";
	  goto out;
	}
    }
#undef DLSYM
#undef DLSYM_OPT

 out:
  if (err != NULL)
    {
      gomp_error ("while loading %s: %s", plugin_name, err);
      if (last_missing)
        gomp_error ("missing function was %s", last_missing);
      if (plugin_handle)
	dlclose (plugin_handle);
    }
  return err == NULL;
}

/* This function adds a compatible offload image IMAGE to an accelerator device
   DEVICE.  DEVICE must be locked on entry, and remains locked on return.  */

static void
gomp_register_image_for_device (struct gomp_device_descr *device,
				struct offload_image_descr *image)
{
  if (!device->offload_regions_registered
      && (device->type == image->type
	  || device->type == OFFLOAD_TARGET_TYPE_HOST))
    {
      device->register_image_func (image->host_table, image->target_data);
      device->offload_regions_registered = true;
    }
}

/* This function initializes the runtime needed for offloading.
   It parses the list of offload targets and tries to load the plugins for
   these targets.  On return, the variables NUM_DEVICES and NUM_DEVICES_OPENMP
   will be set, and the array DEVICES initialized, containing descriptors for
   corresponding devices, first the GOMP_OFFLOAD_CAP_OPENMP_400 ones, follows
   by the others.  */

static void
gomp_target_init (void)
{
  const char *prefix ="libgomp-plugin-";
  const char *suffix = SONAME_SUFFIX (1);
  const char *cur, *next;
  char *plugin_name;
  int i, new_num_devices;

  num_devices = 0;
  devices = NULL;

  cur = OFFLOAD_TARGETS;
  if (*cur)
    do
      {
	struct gomp_device_descr current_device;

	next = strchr (cur, ',');

	plugin_name = (char *) malloc (1 + (next ? next - cur : strlen (cur))
				       + strlen (prefix) + strlen (suffix));
	if (!plugin_name)
	  {
	    num_devices = 0;
	    break;
	  }

	strcpy (plugin_name, prefix);
	strncat (plugin_name, cur, next ? next - cur : strlen (cur));
	strcat (plugin_name, suffix);

	if (gomp_load_plugin_for_device (&current_device, plugin_name))
	  {
	    new_num_devices = current_device.get_num_devices_func ();
	    if (new_num_devices >= 1)
	      {
		/* Augment DEVICES and NUM_DEVICES.  */

		devices = realloc (devices, (num_devices + new_num_devices)
				   * sizeof (struct gomp_device_descr));
		if (!devices)
		  {
		    num_devices = 0;
		    free (plugin_name);
		    break;
		  }

		current_device.name = current_device.get_name_func ();
		/* current_device.capabilities has already been set.  */
		current_device.type = current_device.get_type_func ();
		current_device.mem_map.is_initialized = false;
		current_device.mem_map.splay_tree.root = NULL;
		current_device.is_initialized = false;
		current_device.offload_regions_registered = false;
		current_device.openacc.data_environ = NULL;
		current_device.openacc.target_data = NULL;
		for (i = 0; i < new_num_devices; i++)
		  {
		    current_device.target_id = i;
		    devices[num_devices] = current_device;
		    gomp_mutex_init (&devices[num_devices].mem_map.lock);
		    gomp_mutex_init (&devices[num_devices].lock);
		    num_devices++;
		  }
	      }
	  }

	free (plugin_name);
	cur = next + 1;
      }
    while (next);

  /* In DEVICES, sort the GOMP_OFFLOAD_CAP_OPENMP_400 ones first, and set
     NUM_DEVICES_OPENMP.  */
  struct gomp_device_descr *devices_s
    = malloc (num_devices * sizeof (struct gomp_device_descr));
  if (!devices_s)
    {
      num_devices = 0;
      free (devices);
      devices = NULL;
    }
  num_devices_openmp = 0;
  for (i = 0; i < num_devices; i++)
    if (devices[i].capabilities & GOMP_OFFLOAD_CAP_OPENMP_400)
      devices_s[num_devices_openmp++] = devices[i];
  int num_devices_after_openmp = num_devices_openmp;
  for (i = 0; i < num_devices; i++)
    if (!(devices[i].capabilities & GOMP_OFFLOAD_CAP_OPENMP_400))
      devices_s[num_devices_after_openmp++] = devices[i];
  free (devices);
  devices = devices_s;

  for (i = 0; i < num_devices; i++)
    {
      int j;

      for (j = 0; j < num_offload_images; j++)
	gomp_register_image_for_device (&devices[i], &offload_images[j]);

      /* The 'devices' array can be moved (by the realloc call) until we have
	 found all the plugins, so registering with the OpenACC runtime (which
	 takes a copy of the pointer argument) must be delayed until now.  */
      if (devices[i].capabilities & GOMP_OFFLOAD_CAP_OPENACC_200)
	goacc_register (&devices[i]);
    }

  free (offload_images);
  offload_images = NULL;
  num_offload_images = 0;
}

#else /* PLUGIN_SUPPORT */
/* If dlfcn.h is unavailable we always fallback to host execution.
   GOMP_target* routines are just stubs for this case.  */
static void
gomp_target_init (void)
{
}
#endif /* PLUGIN_SUPPORT */
