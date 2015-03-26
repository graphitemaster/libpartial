#include "partial.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>

#include <sys/mman.h>
#include <unistd.h>

/* List of templates to patch to generate partials */
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
static const size_t fun_patch = 6;
static const size_t arg_patch = 16;
#elif defined(__i386__)
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
static const size_t fun_patch = 14;
static const size_t arg_patch = 9;
#endif

/* Patch a given template with an address */
static void template_patch(unsigned char *template, size_t offset, void *replace) {
    unsigned char *where = template + offset;
    union {
        void *ptr;
        unsigned char u8[sizeof(void*)];
    } patch = { replace };
    for (size_t i = 0; i < sizeof(void*); i++)
        where[i] = patch.u8[i];
}

struct partial_s {
    partial_context_t *context;
    unsigned char *function;
    size_t index;
};

struct partial_context_s {
    size_t templates_per_page;
    size_t page_size;
    size_t pages;
    unsigned char *memory;
    uint32_t *bitset_data;
    size_t bitset_words;
};

partial_context_t *partial_context_create(size_t pages) {
    partial_context_t *context;
    long page_size;

    page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1)
        return NULL;

    context = malloc(sizeof *context);
    if (!context)
        goto oom;

    context->page_size = page_size;
    context->pages = pages;
    context->templates_per_page = page_size / sizeof(kTemplate);

    /* Bitset to keep track of allocated and freed templates */
    context->bitset_words = (context->templates_per_page * pages)
        / (sizeof *context->bitset_data * CHAR_BIT) + 1;
    context->bitset_data = malloc(sizeof *context->bitset_data * context->bitset_words);
    if (!context->bitset_data)
        goto oom;
    memset(context->bitset_data, 0, sizeof *context->bitset_data * context->bitset_words);

    context->memory = mmap(0, page_size * pages, PROT_WRITE | PROT_READ | PROT_EXEC,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (context->memory == MAP_FAILED)
        goto oom;

    return context;

oom:
    free(context);
    return NULL;
}

void partial_context_destroy(partial_context_t *context) {
    munmap(context->memory, context->page_size * context->pages);
    free(context);
}

partial_t *partial_create(partial_context_t *context, void (*function)(void *), void *data) {
    partial_t *partial;
    size_t index;
    /* Find next available template memory */
    for (size_t i = 0; i < context->bitset_words; i++) {
        for (size_t j = 0; j < sizeof *context->bitset_data * CHAR_BIT; j++)  {
            if (context->bitset_data[i] & (1 << j))
                continue;
            index = i * (sizeof *context->bitset_data * CHAR_BIT) + j;
            context->bitset_data[i] |= (1 << j);
            goto build;
        }
    }
    return NULL;

build:
    partial = malloc(sizeof *partial);
    if (!partial)
        return NULL;

    partial->context = context;
    partial->index = index;

    /* Copy and patch template */
    unsigned char *page = context->memory + context->page_size * (index / context->templates_per_page) + 1;
    unsigned char *template = page + sizeof(kTemplate) * (index % context->templates_per_page);
    if (template > context->memory + context->page_size * context->pages)
        goto oom;
    memcpy(template, kTemplate, sizeof(kTemplate));
    template_patch(template, fun_patch, function);
    template_patch(template, arg_patch, data);
    /* Point to it */
    partial->function = template;
    return partial;

oom:
    free(partial);
    return NULL;
}

void partial_destroy(partial_t *partial) {
    if (partial) {
        const size_t word_size = sizeof *partial->context->bitset_data * CHAR_BIT;
        const size_t index = partial->index / word_size;
        const size_t offset = partial->index % word_size;
        partial->context->bitset_data[index] &= ~(1 << offset);
    }
    free(partial);
}

void (*partial_enclose(partial_t *partial))(void) {
    /* Enclose the function */
    return (void (*)(void))partial->function;
}
