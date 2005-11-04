/*
 * This file is part of the GridXSLT project
 * Copyright (C) 2005 Peter Kelly (pmk@cs.adelaide.edu.au)
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

#ifndef _UTIL_LIST_H
#define _UTIL_LIST_H

#include "Shared.h"

namespace GridXSLT {

/* FIXME: ManagedPtrList should delete the objects upon calls to remove() or replace() */

template <class type> class ListImpl;
template <class type> class List;
template <class type> class ManagedPtrList;

template <class type> class ListNode {
  DISABLE_COPY(ListNode)
public:
  ListNode(type v) : next(0), prev(0), data(v) {}
  ListNode<type> *next;
  ListNode<type> *prev;
  type data;
};

template <class type> class Iterator {
public:
  Iterator() : m_list(0), m_current(0) { }
  Iterator(const Iterator<type> &other) : m_list(other.m_list), m_current(other.m_current) { }
  Iterator(const List<type> &list) : m_list(list.impl), m_current(list.impl->first) { }

  inline type &operator*() const { return m_current->data; }
  inline type *operator->() const { return &m_current->data; }
  inline void operator++(int i) { m_current = m_current->next; }
  inline bool haveCurrent() const { return (0 != m_current); }
  inline bool haveNext() const { return (0 != m_current->next); }
  inline type &peek() const { return m_current->next->data; }
  inline void replace(type data) const { m_current->data = data; }
  type remove() {
    ListNode<type> *next = m_current->next;
    type data = m_current->data;

    if (m_list->first == m_current)
      m_list->first = m_list->first->next;

    if (m_list->last == m_current)
      m_list->last = m_list->last->prev;

    if (m_current->prev)
      m_current->prev->next = m_current->next;

    if (m_current->next)
      m_current->next->prev = m_current->prev;

    delete m_current;
    m_current = next;
    return data;
  }

  void insert(type data) {
    if (!m_current) {
      m_list->append(data);
      m_current = m_list->last;
      return;
    }

    ListNode<type> *n = new ListNode<type>(data);
    n->prev = m_current->prev;
    n->next = m_current;

    if (n->prev)
      n->prev->next = n;
    m_current->prev = n;

    if (m_list->first == m_current)
      m_list->first = n;

    m_current = n;
  }

//protected:
  ListImpl<type> *m_list;
  ListNode<type> *m_current;
};

template <class type> class ListImpl : public GridXSLT::Shared< ListImpl<type> > {
  friend class Iterator<type>;
  friend class List<type>;
  friend class ManagedPtrList<type>;
//private:
public:
  ListImpl() : first(0), last(0) {}
  virtual ~ListImpl() {
    clear();
  }

  virtual void clear() {
    ListNode<type> *n = first;
    while (n) {
      ListNode<type> *next = n->next;
      delete n;
      n = next;
    }
  }

  void copyFrom(const ListImpl<type> &other)
  {
    ListNode<type> *n = other.first;
    while (n) {
      append(n->data);
      n = n->next;
    }
  }

public:
  ListImpl(const ListImpl<type> &other) : first(0), last(0) {
    copyFrom(other);
  }

  ListImpl<type> &operator=(const ListImpl<type> &other) {
    clear();
    copyFrom(other);
    return *this;
  }

  bool isEmpty() const {
    return (0 == first);
  }

  unsigned int count() const {
    unsigned int count = 0;
    for (const ListNode<type> *n = first; n; n = n->next)
      count++;
    return count;
  }

  type operator[](unsigned int i) const {
    const ListNode<type> *n = first;
    while (0 < i--)
      n = n->next;
    return n->data;
  }

  void push(type data) {
    ListNode<type> *n = new ListNode<type>(data);
    n->prev = 0;
    n->next = first;

    if (n->next)
      n->next->prev = n;

    if (last == first)
      last = n;

    first = n;
  }

  type pop() {
    ListNode<type> *old = first;
    type data = first->data;

    if (first->next)
      first->next->prev = 0;

    if (last == first)
      last = 0;

    first = first->next;

    delete old;
    return data;
  }

  void append(type data) {
    ListNode<type> *n = new ListNode<type>(data);
    n->prev = last;
    n->next = 0;

    if (n->prev)
      n->prev->next = n;

    if (!first)
      first = n;

    last = n;
  }

  bool contains(type data) const {
    for (const ListNode<type> *n = first; n; n = n->next)
      if (n->data == data)
        return true;
    return false;
  }

  inline ListImpl<type> *unionWith(const ListImpl<type> *other) const {
    ListImpl<type> *result = new ListImpl<type>;
    Iterator<type> it;
    const ListNode<type> *n = first;

    for (n = first; n; n = n->next)
      if (!result->contains(n->data))
        result->append(n->data);

    for (n = other->first; n; n = n->next)
      if (!result->contains(n->data))
        result->append(n->data);

    return result;
  }

  inline ListImpl<type> *intersection(const ListImpl<type> *other) const {
    ListImpl<type> *result = new ListImpl<type>;
    const ListNode<type> *n;

    for (n = first; n; n = n->next)
      if (!result->contains(n->data) && other->contains(n->data))
        result->append(n->data);

    return result;
  }

protected:
  ListNode<type> *first;
  ListNode<type> *last;
};

template <class type> class ManagedPtrListImpl : public ListImpl<type> {
  friend class Iterator<type>;
  friend class List<type>;
  friend class ManagedPtrList<type>;
public:
  ManagedPtrListImpl() : ListImpl<type>() { }
  virtual ~ManagedPtrListImpl() {
    for (ListNode<type> *n = first; n; n = n->next)
      delete n->data;
  }

  /* FIXME: deal with copying.... this will be a problem with ManagedPtrList since only
     one list can "own" the objects */
};

template <class type> class List {
  friend class Iterator<type>;
public:
  List() { impl = new ListImpl<type>(); impl->ref(); }
  List(ListImpl<type> *_impl) { impl = _impl ? _impl->ref() : 0; }
  List(const List<type> &other) { impl = other.impl->ref(); }
  List &operator=(const List<type> &other) { impl->deref(); impl = other.impl->ref(); return *this;}
  virtual ~List() { impl->deref(); }

  virtual List copy() { return new ListImpl<type>(*impl); }

  inline bool isEmpty() const { return impl->isEmpty(); }
  inline unsigned int count() const { return impl->count(); }
  type operator[](unsigned int i) const { return (*impl)[i]; }
  inline void push(type data) { impl->push(data); }
  inline type pop() { return impl->pop(); }
  inline void append(type data) { impl->append(data); }
  inline bool contains(type data) const { return impl->contains(data); }
  inline List<type> unionWith(List<type> &other) const { return impl->unionWith(other.impl); }
  inline List<type> intersection(List<type> &other) const { return impl->intersection(other.impl); }

//protected:
  ListImpl<type> *impl;
};

template <class type> class ManagedPtrList : public List<type> {
public:
  ManagedPtrList() : List<type>(0)
    { impl = new ManagedPtrListImpl<type>(); impl->ref(); }
  ManagedPtrList(ManagedPtrListImpl<type> *_impl) : List<type>(0)
    { impl = _impl->ref(); }
  ManagedPtrList(const ManagedPtrList<type> &other) : List<type>(0)
    { impl = other.impl->ref(); }
  ManagedPtrList &operator=(const ManagedPtrList<type> &other)
    { impl->deref(); impl = other.impl->ref(); return *this; }
  virtual ~ManagedPtrList() { }

/*   virtual List<type> copy() { return new ManagedPtrListImpl<type>(*impl); } */
};

};

typedef struct list list;

typedef void (*list_d_t)(void *a);
typedef void* (*list_copy_t)(void *a);

struct list {
  void *data;
  list *next;
};

list *list_copy(list *orig, list_copy_t copy);
void list_append(list **l, void *data);
void list_push(list **l, void *data);
void *list_pop(list **l);
int list_count(list *l);
void list_free(list *l, list_d_t d);

int list_contains_string(list *l, const char *str);
int list_contains_ptr(list *l, const void *data);
void list_remove_ptr(list **l, void *ptr);

#endif /* _UTIL_LIST_H */
