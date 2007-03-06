#define llist_append(_ll,_obj) {                          \
    assert(!(_obj)->prev && !(_obj)->next);               \
    if ((_ll)->last) {                                    \
      (_obj)->prev = (_ll)->last;                         \
      (_ll)->last->next = (_obj);                         \
      (_ll)->last = (_obj);                               \
    }                                                     \
    else {                                                \
      (_ll)->first = (_ll)->last = (_obj);                \
    }                                                     \
  }

#define llist_remove(_ll,_obj) {                          \
    assert((_obj)->prev || ((_ll)->first == (_obj)));     \
    assert((_obj)->next || ((_ll)->last == (_obj)));      \
    if ((_ll)->first == (_obj))                           \
      (_ll)->first = (_obj)->next;                        \
    if ((_ll)->last == (_obj))                            \
      (_ll)->last = (_obj)->prev;                         \
    if ((_obj)->next)                                     \
      (_obj)->next->prev = (_obj)->prev;                  \
    if ((_obj)->prev)                                     \
      (_obj)->prev->next = (_obj)->next;                  \
    (_obj)->next = NULL;                                  \
    (_obj)->prev = NULL;                                  \
  }

int start_listening(struct in_addr ip, int port);
int start_listening_host(const char *host, int port);
int fdsetflag(int fd, int flag, int on);
int fdsetblocking(int fd, int blocking);

typedef struct array {
  int elemsize;
  int alloc;
  int nbytes;
  char *data;
} array;

array *array_new(int elemsize);
void array_append(array *arr, const void *data, int size);
void array_free(array *arr);
