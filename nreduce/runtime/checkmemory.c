/*
 * This file is part of the nreduce project
 * Copyright (C) 2006-2009 Peter Kelly (pmk@cs.adelaide.edu.au)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * $Id$
 *
 */

#include "runtime.h"
#include "messages.h"
#include <stdlib.h>

static int oldgen_cell_valid(task *tsk, int cnewgen, cell *c)
{
  block *bl;
  int found = 0;
  for (bl = tsk->oldgen; bl; bl = bl->next) {
    if (((char*)c >= BLOCK_START+(char*)bl) && ((char*)c < BLOCK_END+(char*)bl)) {
      found = 1;
      break;
    }
  }
  if (cnewgen && !found) {
    for (bl = tsk->newgen; bl; bl = bl->next) {
      if (((char*)c >= BLOCK_START+(char*)bl) && ((char*)c < BLOCK_END+(char*)bl)) {
        found = 1;
        break;
      }
    }
  }
  return found;
}

static int oldgen_pntr_valid(task *tsk, int cnewgen, pntr p)
{
  if (!is_pntr(p))
    return 1;
  return oldgen_cell_valid(tsk,cnewgen,get_pntr(p));
}

/* Check the validity of all pointers in the heap... they should only point to other
   cells in the old generation. For use after a minor or major collection cycle. */
static void check_all_refs(task *tsk, int cnewgen)
{
  block *bl;
  for (bl = tsk->oldgen; bl; bl = bl->next) {
    unsigned int off;
    for (off = BLOCK_START; off < BLOCK_END; off += object_size(off+(char*)bl)) {
      cell *c = (cell*)(off+(char*)bl);
      if (c->type >= CELL_OBJS)
        continue;

      switch (c->type) {
      case CELL_APPLICATION:
        assert(oldgen_pntr_valid(tsk,cnewgen,c->field1));
        assert(oldgen_pntr_valid(tsk,cnewgen,c->field2));
        break;
      case CELL_CONS:
        assert(oldgen_pntr_valid(tsk,cnewgen,c->field1));
        assert(oldgen_pntr_valid(tsk,cnewgen,c->field2));
        break;
      case CELL_IND: {
        assert(oldgen_pntr_valid(tsk,cnewgen,c->field1));
        break;
      }
      case CELL_AREF: {
        assert(oldgen_pntr_valid(tsk,cnewgen,c->field1));
        assert(oldgen_pntr_valid(tsk,cnewgen,c->field2));
        break;
      }
      case CELL_O_ARRAY: {
        carray *carr = (carray*)c;
        if (sizeof(pntr) == carr->elemsize) {
          int i;
          for (i = 0; i < carr->size; i++)
            assert(oldgen_pntr_valid(tsk,cnewgen,((pntr*)carr->elements)[i]));
        }
        break;
      }
      case CELL_FRAME: {
        frame *f = (frame*)get_pntr(c->field1);
        int i;
        if ((STATE_NEW == f->state) ||
            (STATE_ACTIVE == f->state) ||
            (STATE_SPARKED == f->state)) {
          for (i = 0; i < f->instr->expcount; i++)
            assert(oldgen_pntr_valid(tsk,cnewgen,f->data[i]));
          assert(oldgen_cell_valid(tsk,cnewgen,f->c));
          assert(c == f->c);
        }
        break;
      }
      case CELL_CAP: {
        assert(oldgen_pntr_valid(tsk,cnewgen,c->field1));
        cap *cp = (cap*)get_pntr(c->field1);
        int i;
        for (i = 0; i < cp->count; i++)
          assert(oldgen_pntr_valid(tsk,cnewgen,cp->data[i]));
        break;
      }
      case CELL_SYSOBJECT: {
        sysobject *so = (sysobject*)get_pntr(c->field1);
        assert(oldgen_cell_valid(tsk,cnewgen,so->c));
        assert(c == so->c);
        assert(oldgen_pntr_valid(tsk,cnewgen,so->listenerso));
        assert(oldgen_pntr_valid(tsk,cnewgen,so->p));
        break;
      }
      case CELL_REMOTEREF: {
        global *glo = (global*)get_pntr(c->field1);
        assert(oldgen_pntr_valid(tsk,cnewgen,glo->p));
        break;
      }
      }
    }
  }

  /* Check all globals... any that don't reference a valid pointer shouldn't exist any more */
  global *glo;
  for (glo = tsk->globals.first; glo; glo = glo->next) {
    if (!oldgen_pntr_valid(tsk,cnewgen,glo->p)) {
      node_log(tsk->n,LOG_ERROR,"Global %d@%d is not in valid memory region",
               glo->addr.lid,glo->addr.tid);
    }
    assert(oldgen_pntr_valid(tsk,cnewgen,glo->p));
  }

  node_log(tsk->n,LOG_INFO,"check_all_refs: all references ok (cnewgen=%d)",cnewgen);
}

void check_all_refs_in_oldgen(task *tsk)
{
  check_all_refs(tsk,0);
}

void check_all_refs_in_eithergen(task *tsk)
{
  check_all_refs(tsk,1);
}

static int is_new_ref(pntr p)
{
  return (is_pntr(p) && !(get_pntr(p)->flags & FLAG_MATURE));
}

static int is_new_cell(cell *c)
{
  return !(c->flags & FLAG_MATURE);
}

static int cell_has_new_ref(task *tsk, cell *c)
{
  switch (c->type) {
  case CELL_APPLICATION:
    return (is_new_ref(c->field1) || is_new_ref(c->field2));
  case CELL_CONS:
    return (is_new_ref(c->field1) || is_new_ref(c->field2));
  case CELL_IND:
    return is_new_ref(c->field1);
    break;
  case CELL_AREF:
    if (is_new_ref(c->field1))
      return 1;
    return is_new_ref(c->field2);
  case CELL_O_ARRAY: {
    carray *carr = (carray*)c;
    if (sizeof(pntr) == carr->elemsize) {
      int i;
      for (i = 0; i < carr->size; i++) {
        if (is_new_ref(((pntr*)carr->elements)[i]))
          return 1;
      }
    }
    return 0;
  }
  case CELL_FRAME: {
    frame *f = (frame*)get_pntr(c->field1);
    if (STATE_NEW == f->state) {
      int i;
      for (i = 0; i < f->instr->expcount; i++) {
        if (is_new_ref(f->data[i]))
          return 1;
      }
      return is_new_cell(f->c);
    }
    return 0;
  }
  case CELL_CAP: {
    if (is_new_ref(c->field1))
      return 1;
    cap *cp = (cap*)get_pntr(c->field1);
    int i;
    for (i = 0; i < cp->count; i++) {
      if (is_new_ref(cp->data[i]))
        return 1;
    }
    return 0;
  }
  case CELL_SYSOBJECT: {
    sysobject *so = (sysobject*)get_pntr(c->field1);
    return (is_new_cell(so->c) || is_new_ref(so->listenerso) || is_new_ref(so->p));
  }
  case CELL_REMOTEREF:
  case CELL_HOLE:
  case CELL_NIL:
    return 0;
  }
  fprintf(stderr,"type %d\n",c->type);
  assert(!"invalid cell type");
  return 0;
}

void check_remembered_set(task *tsk)
{
  block *bl;
  int refs = 0;

  /* Check that the INRSET flag is set on every cell that has a reference to an object
     in the new generation */
  for (bl = tsk->oldgen; bl; bl = bl->next) {
    unsigned int off;
    for (off = BLOCK_START; off < BLOCK_END; off += object_size(off+(char*)bl)) {
      header *h = (header*)(off+(char*)bl);
      if (h->type >= CELL_OBJS)
        continue;

      cell *c = (cell*)h;
      if (CELL_EMPTY == c->type)
        break;

      assert(REF_FLAGS != c->flags);
      assert(CELL_COUNT > c->type);
      if (cell_has_new_ref(tsk,c)) {
        int inset = (c->flags & FLAG_INRSET);
        if (!inset) {
          global *glo;
          pntr p;
          make_pntr(p,c);

          array *tmp = array_new(1,0);
          array_printf(tmp,"cell %p (%s) with new ref is not in remembered set",
                       c,cell_types[c->type]);
          if (NULL != (glo = physhash_lookup(tsk,p)))
            array_printf(tmp," (physical address %d@%d)",glo->addr.lid,glo->addr.tid);
          if (NULL != (glo = targethash_lookup(tsk,p)))
            array_printf(tmp," (target address %d@%d)",glo->addr.lid,glo->addr.tid);
          node_log(tsk->n,LOG_ERROR,"%s",tmp->data);
          array_free(tmp);

          if (CELL_AREF == c->type) {
            carray *carr = (carray*)get_pntr(c->field1);
            pntr tail = c->field2;
            if (sizeof(pntr) == carr->elemsize) {
              int i;
              for (i = 0; i < carr->size; i++) {
                if (is_new_ref(((pntr*)carr->elements)[i])) {
                  node_log(tsk->n,LOG_ERROR,"new ref: array item %d %s",
                           i,cell_types[pntrtype(((pntr*)carr->elements)[i])]);
                }
              }
            }
            if (is_new_ref(tail)) {
              node_log(tsk->n,LOG_ERROR,"new ref: array tail %s %p",
                       cell_types[pntrtype(tail)],
                       is_pntr(tail) ? get_pntr(tail) : 0);
            }
          }
        }
        assert(inset);
        refs++;
      }
    }
  }
  node_log(tsk->n,LOG_INFO,"remembered set ok, refs = %d, rsetsize = %d",
           refs,array_count(tsk->remembered));
}

int check_oldgen_valid(task *tsk)
{
  block *bl;
  for (bl = tsk->oldgen; bl; bl = bl->next) {
    unsigned int off;
    for (off = BLOCK_START; off < BLOCK_END; off += object_size(off+(char*)bl)) {
      header *h = (header*)(off+(char*)bl);
      if (CELL_EMPTY == h->type)
        break;
      if ((REF_FLAGS == h->flags) || (CELL_COUNT <= h->type)) {
        return 0;
      }
    }
  }
  return 1;
}

int count_inds(task *tsk)
{
  block *bl;
  int count = 0;
  for (bl = tsk->oldgen; bl; bl = bl->next) {
    unsigned int off;
    for (off = BLOCK_START; off < BLOCK_END; off += object_size(off+(char*)bl)) {
      header *h = (header*)(off+(char*)bl);
      assert(REF_FLAGS != h->flags);
      assert(CELL_COUNT > h->type);
      if (CELL_IND == h->type)
        count++;
    }
  }
  return count;
}

void update_lifetimes(task *tsk, block *gen)
{
#ifdef OBJECT_LIFETIMES
  block *bl;
  for (bl = gen; bl; bl = bl->next) {
    unsigned int off = BLOCK_START;

    while (off < BLOCK_END) {
      header *h = (header*)(off+(char*)bl);
      unsigned int size;

      if (REF_FLAGS == h->flags) {
        header *target = (header*)h->type;
        assert(CELL_COUNT <= h->type);
        assert(REF_FLAGS != target->flags);
        assert(CELL_COUNT > target->type);
        size = object_size(target);
      }
      else {
        assert(CELL_COUNT > h->type);
        size = object_size(h);

        unsigned int age = tsk->total_bytes_allocated - h->birth;
        unsigned int bucket = age/LIFETIME_INCR;
        unsigned int zero = 0;
        while (array_count(tsk->lifetimes) <= bucket) {
          array_append(tsk->lifetimes,&zero,sizeof(int));
        }
        ((int*)tsk->lifetimes->data)[bucket]++;
      }

      off += size;
    }
  }
#endif
}

int calc_newgenbytes(task *tsk)
{
  /* FIXME: this may add a bit extra, since there may be a gap at the end of
     a block which was not used because the object could not fit in the remainer
     of the block. Maybe need to add a "used" field to each block so that we can
     efficiently calculate the total space used */
  int bytes = 0;
  if (tsk->newgen && tsk->newgen->next) {
    block *bl;
    for (bl = tsk->newgen->next; bl; bl = bl->next) {
      bytes += bl->used;
    }
  }
  bytes += (int)(tsk->newgenoffset)-BLOCK_START;
  return bytes;
}

void send_checkrefs(task *tsk)
{
  int i;
  for (i = 0; i < tsk->groupsize; i++) {
    if (i != tsk->tid) {
      array *arr = array_new(1,0);
      global *glo;
      for (glo = tsk->globals.first; glo; glo = glo->next) {
        if (glo->addr.tid == i)
          array_append(arr,&glo->addr,sizeof(gaddr));
      }

      endpoint_send(tsk->endpt,tsk->idmap[i],MSG_CHECKREFS,arr->data,arr->nbytes);
      array_free(arr);
    }
  }
}
