## libpartial

Partially applied functions in C (for ignorant callbacks)

## Example

### The problem
```
extern void some_callbacker(void (*cb)());

volatile int g = 0; /* This is ugly */

void my_callback() {
    /* Need a global since some_callbacker does not pass state */
    g++;
}

int main() {
    some_callbacker(my_callback);
}
```

This situation one runs into many times working with legacy code bases and
libraries. The modern approach is to declare callbacks to take functions expecting
user data in the form of `void*`. You don't need to look far to find them either,
the standard C library has a few of its own, like `atexit` and `signal`.

### The solution
```
extern void some_callbacker(void (*cb)());

void my_callback(void *data) {
    *(int *)data++;
}

int main() {
    int g = 0;
    partial_context_t *c = partial_context_create(1);
    partial_t *p = partial_create(c, my_callback, &g);
    some_callbacker(partial_enclose(p));
    partial_destroy(p);
    partial_context_destroy(c);
}
```

## Supported architectures

Currently libpartial supports SysV ABI (x86_64 / amd64) and IA32. That covers
all x86 and x86_64 Unix like OSes including OSx, Linux and BSDs. Windows is not
supported.

## How it works

Depending on your architecture, a template which implements a basic function
is used to stamp out functions that call the appropriate ones with the enclosed
address inline. You can see this for yourself here.

```
#if defined(__x86_64__)
static const unsigned char kTemplate[] = {
    0x55,                                     /* push %rbp */
    0x48, 0x89, 0xE5,                         /* mov %rsp, %rbp */
    0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, /* movabs $0x0000000000000000,%rax */
    0x00, 0x00, 0x00,
    0x48, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, /* movabs $0x0000000000000000,%rdi */
    0x00, 0x00, 0x00,
    0xFF, 0xD0,                               /* callq *%rax */
    0x5D,                                     /* pop %rbp */
    0xC3                                      /* retq */
};
#else
static const unsigned char kTemplate[] = {
    0x55,                                     /* push %ebp */
    0x89, 0xE5,                               /* mov %esp, %ebp */
    0x83, 0xEC, 0x18,                         /* sub $0x18, %esp */
    0xC7, 0x04, 0x24, 0x00, 0x00, 0x00, 0x00, /* movl $0x00000000, (%esp) */
    0xB8, 0x00, 0x00, 0x00, 0x00,             /* mov $0x00000000, %eax */
    0xFF, 0xD0,                               /* call *%eax */
    0xC9,                                     /* leave */
    0xC3                                      /* ret */
};
#endif
```

You may stamp out as many of these functions as you want, provided you have
enough space left in the context to do so. A special hand-crafted template
allocator allocates from executable pages to store these functions.

## API

The API is documented in the `partial.h` header using standard Doxygen.
