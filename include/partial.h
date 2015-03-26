/**
 * @file partial.h
 * @author Dale Weiler
 * @brief libpartial API
 *
 * libpartial allows you to partially apply arguments to standard C callback
 * functions that fail to provide a means to pass user data. Such examples of
 * this include: signal, atexit, glutDisplayFunc, etc.
 *
 * How libpartial works more or less involves creating functions at runtime
 * with the appropriate call to the function containing the pointer hard-coded
 * as a constant. This requires an allocator which can back executable pages,
 * which is most of the library.
 */
#ifndef LIBPARTIAL_HDR
#define LIBPARTIAL_HDR
#include <stddef.h>

/**
 * @brief Opaque handle to a partially applied function
 */
typedef struct partial_s partial_t;

/**
 * @brief Opaque handle to context
 */
typedef struct partial_context_s partial_context_t;

/**
 * @brief Construct a context for partial applications
 *
 * A context for partial applications to be created with.
 *
 * @param pages The amount of executable pages to allocate.
 * @return Opaque handle to a context on success, NULL on failure.
 */
partial_context_t *partial_context_create(size_t pages);

/**
 * @brief Destroy a context for partial applications.
 *
 * @param context The context to destroy.
 *
 * @warning Any partials that were created with the context passed to this will
 * be invalid and placing a call to them will invoke UB.
 */
void partial_context_destroy(partial_context_t *context);

/**
 * @brief Create a partially applied function.
 *
 * @param context The context to create the partial under.
 * @param function The function to partially apply.
 * @param data The data to partially apply the function with.
 *
 * @return Opaque handle to a partial on success, NULL on failure.
 */
partial_t *partial_create(partial_context_t *context, void (*function)(void *data), void *data);

/**
 * @brief Destroy a partially applied function.
 *
 * @param partial The partial to destroy.
 */
void partial_destroy(partial_t *partial);

/**
 * @brief Enclose a partial.
 *
 * Encloses a partial, giving a argument-less function that, when called will
 * actually call the enclosed function with the data specified for the given
 * partial during #partial_create
 */
void (*partial_enclose(partial_t *partial))(void);

#endif
