#include <sys/reent.h>

extern struct _reent *_impure_ptr;
extern struct _reent _impure_data;
extern void __sinit(struct _reent *);

void donsdos_newlib_init(void) {
    _impure_ptr = &_impure_data;
    __sinit(_impure_ptr);
}

