#ifndef INCLUDE_CONTAINERS_LIST_H_
#define INCLUDE_CONTAINERS_LIST_H_

template <class T>
struct list {
  struct node {
    node* next;
    T data;
  };

  node* head;
  void push(core::arenaT t);
};

#endif // INCLUDE_CONTAINERS_LIST_H_
