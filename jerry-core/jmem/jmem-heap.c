/* Copyright JS Foundation and other contributors, http://js.foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * Heap implementation
 */

#include "ecma-gc.h"
#include "jcontext.h"
#include "jmem.h"
#include "jrt-bit-fields.h"
#include "jrt-libc-includes.h"

#define JMEM_ALLOCATOR_INTERNAL
#include "jmem-allocator-internal.h"

/** \addtogroup mem Memory allocation
 * @{
 *
 * \addtogroup heap Heap
 * @{
 */

#if !ENABLED (JERRY_SYSTEM_ALLOCATOR)
/**
 * End of list marker.
 */
#define JMEM_HEAP_END_OF_LIST ((uint32_t) 0xffffffff)

/**
 * @{
 */
#ifdef ECMA_VALUE_CAN_STORE_UINTPTR_VALUE_DIRECTLY
/* In this case we simply store the pointer, since it fits anyway. */
#define JMEM_HEAP_GET_OFFSET_FROM_ADDR(p) ((uint32_t) (p))
#define JMEM_HEAP_GET_ADDR_FROM_OFFSET(u) ((jmem_heap_free_t *) (u))
#else /* !ECMA_VALUE_CAN_STORE_UINTPTR_VALUE_DIRECTLY */
#define JMEM_HEAP_GET_OFFSET_FROM_ADDR(p) ((uint32_t) ((uint8_t *) (p) - JERRY_HEAP_CONTEXT (area)))
#define JMEM_HEAP_GET_ADDR_FROM_OFFSET(u) ((jmem_heap_free_t *) (JERRY_HEAP_CONTEXT (area) + (u)))
#endif /* ECMA_VALUE_CAN_STORE_UINTPTR_VALUE_DIRECTLY */
/**
 * @}
 */

/**
 * Get end of region
 *
 * @return pointer to the end of the region
 */
static inline jmem_heap_free_t *  JERRY_ATTR_ALWAYS_INLINE JERRY_ATTR_PURE
jmem_heap_get_region_end (jmem_heap_free_t *curr_p) /**< current region */
{
  return (jmem_heap_free_t *)((uint8_t *) curr_p + curr_p->size);
} /* jmem_heap_get_region_end */
#endif /* !ENABLED (JERRY_SYSTEM_ALLOCATOR) */

/**
 * Startup initialization of heap
 */
void
jmem_heap_init (void)
{
#if !ENABLED (JERRY_SYSTEM_ALLOCATOR)
#if !ENABLED (JERRY_CPOINTER_32_BIT)
  /* the maximum heap size for 16bit compressed pointers should be 512K */
  JERRY_ASSERT (((UINT16_MAX + 1) << JMEM_ALIGNMENT_LOG) >= JMEM_HEAP_SIZE);
#endif /* !ENABLED (JERRY_CPOINTER_32_BIT) */
  JERRY_ASSERT ((uintptr_t) JERRY_HEAP_CONTEXT (area) % JMEM_ALIGNMENT == 0);

  JERRY_CONTEXT (jmem_gc_limit) = CONFIG_GC_LIMIT;

  jmem_heap_free_t *const region_p = (jmem_heap_free_t *) JERRY_HEAP_CONTEXT (area);

  region_p->size = JMEM_HEAP_AREA_SIZE;
  region_p->next_offset = JMEM_HEAP_END_OF_LIST;

  JERRY_HEAP_CONTEXT (first).size = 0;
  JERRY_HEAP_CONTEXT (first).next_offset = JMEM_HEAP_GET_OFFSET_FROM_ADDR (region_p);

  JERRY_CONTEXT (jmem_heap_list_skip_p) = &JERRY_HEAP_CONTEXT (first);

  JMEM_VALGRIND_NOACCESS_SPACE (&JERRY_HEAP_CONTEXT (first), sizeof (jmem_heap_free_t));
  JMEM_VALGRIND_NOACCESS_SPACE (JERRY_HEAP_CONTEXT (area), JMEM_HEAP_AREA_SIZE);

#endif /* !ENABLED (JERRY_SYSTEM_ALLOCATOR) */
  JMEM_HEAP_STAT_INIT ();
} /* jmem_heap_init */

/**
 * Finalize heap
 */
void
jmem_heap_finalize (void)
{
  jmem_heap_reclaim_pools ();

  JERRY_ASSERT (JERRY_CONTEXT (jmem_heap_allocated_size) == 0);
#if !ENABLED (JERRY_SYSTEM_ALLOCATOR)
  JMEM_VALGRIND_NOACCESS_SPACE (&JERRY_HEAP_CONTEXT (first), JMEM_HEAP_SIZE);
#endif /* !ENABLED (JERRY_SYSTEM_ALLOCATOR) */
} /* jmem_heap_finalize */

/**
 * Allocation of memory region.
 *
 * See also:
 *          jmem_heap_alloc_block
 *
 * @return pointer to allocated memory block - if allocation is successful,
 *         NULL - if there is not enough memory.
 */
static void * JERRY_ATTR_HOT
jmem_heap_alloc_internal (const size_t size) /**< size of requested block */
{
  const size_t aligned_size = JERRY_ALIGNUP (size, JMEM_ALIGNMENT);

#if !ENABLED (JERRY_SYSTEM_ALLOCATOR)
  JMEM_VALGRIND_DEFINED_SPACE (&JERRY_HEAP_CONTEXT (first), sizeof (jmem_heap_free_t));

  uint32_t current_offset = JERRY_HEAP_CONTEXT (first).next_offset;
  jmem_heap_free_t *prev_p = &JERRY_HEAP_CONTEXT (first);

  while (JERRY_LIKELY (current_offset != JMEM_HEAP_END_OF_LIST))
  {
    jmem_heap_free_t *const current_p = JMEM_HEAP_GET_ADDR_FROM_OFFSET (current_offset);
    JERRY_ASSERT (jmem_is_heap_pointer (current_p));
    JMEM_VALGRIND_DEFINED_SPACE (current_p, sizeof (jmem_heap_free_t));

    const uint32_t next_offset = current_p->next_offset;
    JERRY_ASSERT (next_offset == JMEM_HEAP_END_OF_LIST
                  || jmem_is_heap_pointer (JMEM_HEAP_GET_ADDR_FROM_OFFSET (next_offset)));

    if (current_p->size >= aligned_size)
    {
      /* Region was larger than necessary. */
      if (current_p->size > aligned_size)
      {
        /* Get address of remaining space. */
        jmem_heap_free_t *const remaining_p = (jmem_heap_free_t *) ((uint8_t *) current_p + aligned_size);

        /* Update metadata. */
        JMEM_VALGRIND_DEFINED_SPACE (remaining_p, sizeof (jmem_heap_free_t));
        remaining_p->size = current_p->size - (uint32_t) aligned_size;
        remaining_p->next_offset = next_offset;
        JMEM_VALGRIND_NOACCESS_SPACE (remaining_p, sizeof (jmem_heap_free_t));

        /* Update list. */
        JMEM_VALGRIND_DEFINED_SPACE (prev_p, sizeof (jmem_heap_free_t));
        prev_p->next_offset = JMEM_HEAP_GET_OFFSET_FROM_ADDR (remaining_p);
        JMEM_VALGRIND_NOACCESS_SPACE (prev_p, sizeof (jmem_heap_free_t));
      }
      /* Block is an exact fit. */
      else
      {
        /* Remove the region from the list. */
        JMEM_VALGRIND_DEFINED_SPACE (prev_p, sizeof (jmem_heap_free_t));
        prev_p->next_offset = next_offset;
        JMEM_VALGRIND_NOACCESS_SPACE (prev_p, sizeof (jmem_heap_free_t));
      }

      JERRY_CONTEXT (jmem_heap_list_skip_p) = prev_p;

      size_t gc_limit = JERRY_CONTEXT (jmem_gc_limit);
      while (gc_limit < aligned_size)
      {
        gc_limit += CONFIG_GC_LIMIT;
      }
      JERRY_CONTEXT (jmem_gc_limit) = gc_limit - aligned_size;
#ifndef JERRY_NDEBUG
      JERRY_CONTEXT (jmem_heap_allocated_size) += aligned_size;
#endif /* !JERRY_NDEBUG */

      /* Found enough space. */
      JERRY_ASSERT ((uintptr_t) current_p % JMEM_ALIGNMENT == 0);
      JMEM_VALGRIND_MALLOCLIKE_SPACE (current_p, size);
      JMEM_VALGRIND_NOACCESS_SPACE (&JERRY_HEAP_CONTEXT (first), sizeof (jmem_heap_free_t));

      return (void *) current_p;
    }

    JMEM_VALGRIND_NOACCESS_SPACE (current_p, sizeof (jmem_heap_free_t));
    /* Next in list. */
    prev_p = current_p;
    current_offset = next_offset;
  }

  JMEM_VALGRIND_NOACCESS_SPACE (&JERRY_HEAP_CONTEXT (first), sizeof (jmem_heap_free_t));
  return (void *) NULL;
#else /* ENABLED (JERRY_SYSTEM_ALLOCATOR) */
  size_t gc_limit = JERRY_CONTEXT (jmem_gc_limit);
  while (gc_limit < aligned_size)
  {
    gc_limit += CONFIG_GC_LIMIT;
  }
  JERRY_CONTEXT (jmem_gc_limit) = gc_limit - aligned_size;
#ifndef JERRY_NDEBUG
  JERRY_CONTEXT (jmem_heap_allocated_size) += aligned_size;
#endif /* !JERRY_NDEBUG */

  return malloc (aligned_size);
#endif /* !ENABLED (JERRY_SYSTEM_ALLOCATOR) */
} /* jmem_heap_alloc */

#if !ENABLED (JERRY_SYSTEM_ALLOCATOR)
/**
 * Finds the block in the free block list which preceeds the argument block
 *
 * @return pointer to the preceeding block
 */
static jmem_heap_free_t *
jmem_heap_find_prev (const jmem_heap_free_t * const block_p) /**< which memory block's predecessor we're looking for */
{
  const jmem_heap_free_t *prev_p;

  if (block_p > JERRY_CONTEXT (jmem_heap_list_skip_p))
  {
    prev_p = JERRY_CONTEXT (jmem_heap_list_skip_p);
  }
  else
  {
    prev_p = &JERRY_HEAP_CONTEXT (first);
  }

  JERRY_ASSERT (jmem_is_heap_pointer (block_p));
  const uint32_t block_offset = JMEM_HEAP_GET_OFFSET_FROM_ADDR (block_p);

  JMEM_VALGRIND_DEFINED_SPACE (prev_p, sizeof (jmem_heap_free_t));
  /* Find position of region in the list. */
  while (prev_p->next_offset < block_offset)
  {
    const jmem_heap_free_t * const next_p = JMEM_HEAP_GET_ADDR_FROM_OFFSET (prev_p->next_offset);
    JERRY_ASSERT (jmem_is_heap_pointer (next_p));

    JMEM_VALGRIND_DEFINED_SPACE (next_p, sizeof (jmem_heap_free_t));
    JMEM_VALGRIND_NOACCESS_SPACE (prev_p, sizeof (jmem_heap_free_t));
    prev_p = next_p;
  }

  JMEM_VALGRIND_NOACCESS_SPACE (prev_p, sizeof (jmem_heap_free_t));
  JERRY_CONTEXT (jmem_heap_list_skip_p) = prev_p;
  return (jmem_heap_free_t *) prev_p;
} /* jmem_heap_find_prev */

/**
 * Inserts the block into the free chain after a specified block.
 *
 * Note:
 *     'jmem_heap_find_prev' can and should be used to find the previous free block
 */
static void
jmem_heap_insert_block (jmem_heap_free_t *block_p, /**< block to insert */
                        jmem_heap_free_t *const prev_p, /**< the free block after which to insert 'block_p' */
                        const size_t size) /**< size of the inserted block */
{
  JERRY_ASSERT ((uintptr_t) block_p % JMEM_ALIGNMENT == 0);
  JERRY_ASSERT (size % JMEM_ALIGNMENT == 0);

  JMEM_VALGRIND_NOACCESS_SPACE (block_p, size);

  JMEM_VALGRIND_DEFINED_SPACE (prev_p, sizeof (jmem_heap_free_t));
  const uint32_t next_offset = prev_p->next_offset;
  jmem_heap_free_t *const next_p = JMEM_HEAP_GET_ADDR_FROM_OFFSET (next_offset);

  JMEM_VALGRIND_DEFINED_SPACE (next_p, sizeof (jmem_heap_free_t));
  JMEM_VALGRIND_DEFINED_SPACE (block_p, sizeof (jmem_heap_free_t));
  JMEM_VALGRIND_DEFINED_SPACE (next_p, sizeof (jmem_heap_free_t));

  const uint32_t block_offset = JMEM_HEAP_GET_OFFSET_FROM_ADDR (block_p);

  JERRY_ASSERT (JERRY_CONTEXT (jmem_heap_list_skip_p) != block_p);
  JERRY_ASSERT (block_p > prev_p);
  JERRY_ASSERT (block_p < next_p);

  /* Update prev. */
  if (jmem_heap_get_region_end (prev_p) == block_p)
  {
    /* Can be merged. */
    prev_p->size += (uint32_t) size;
    JMEM_VALGRIND_NOACCESS_SPACE (block_p, sizeof (jmem_heap_free_t));
    block_p = prev_p;
  }
  else
  {
    block_p->size = (uint32_t) size;
    prev_p->next_offset = block_offset;
  }

  /* Update next. */
  if (jmem_heap_get_region_end (block_p) == next_p)
  {
    JERRY_ASSERT (JERRY_CONTEXT (jmem_heap_list_skip_p) != next_p);

    /* Can be merged. */
    block_p->size += next_p->size;
    block_p->next_offset = next_p->next_offset;
  }
  else
  {
    block_p->next_offset = next_offset;
  }

  JMEM_VALGRIND_NOACCESS_SPACE (prev_p, sizeof (jmem_heap_free_t));
  JMEM_VALGRIND_NOACCESS_SPACE (block_p, sizeof (jmem_heap_free_t));
  JMEM_VALGRIND_NOACCESS_SPACE (next_p, sizeof (jmem_heap_free_t));

#ifndef JERRY_NDEBUG
  JERRY_ASSERT (JERRY_CONTEXT (jmem_heap_allocated_size) >= size);
  JERRY_CONTEXT (jmem_heap_allocated_size) -= size;
#endif /* !JERRY_NDEBUG */

  size_t gc_limit = JERRY_CONTEXT (jmem_gc_limit) + size;
  while (gc_limit > CONFIG_GC_LIMIT)
  {
    gc_limit -= CONFIG_GC_LIMIT;
  }
  JERRY_CONTEXT (jmem_gc_limit) = gc_limit;
} /* jmem_heap_insert_block */
#endif /* !ENABLED (JERRY_SYSTEM_ALLOCATOR) */

/**
 * Internal method for freeing a memory block.
 */
inline static void JERRY_ATTR_HOT JERRY_ATTR_ALWAYS_INLINE
jmem_heap_free_internal (void *ptr, /**< pointer to beginning of data space of the block */
                         const size_t size) /**< size of allocated region */
{
  JERRY_ASSERT (size > 0);
  JERRY_ASSERT (jmem_is_heap_pointer (ptr));
  JERRY_ASSERT ((uintptr_t) ptr % JMEM_ALIGNMENT == 0);

#if !ENABLED (JERRY_SYSTEM_ALLOCATOR)
  const size_t aligned_size = JERRY_ALIGNUP (size, JMEM_ALIGNMENT);

  jmem_heap_free_t *const block_p = (jmem_heap_free_t *) ptr;
  jmem_heap_free_t *const prev_p = jmem_heap_find_prev (block_p);
  jmem_heap_insert_block (block_p, prev_p, aligned_size);

  JMEM_VALGRIND_FREELIKE_SPACE (ptr);
#else /* ENABLED (JERRY_SYSTEM_ALLOCATOR) */
  free (ptr);
#endif /* !ENABLED (JERRY_SYSTEM_ALLOCATOR) */
} /* jmem_heap_free_internal */

/**
 * TODO
 */
inline static void * JERRY_ATTR_HOT JERRY_ATTR_ALWAYS_INLINE
jmem_heap_pool_alloc (const size_t size)
{
  JERRY_ASSERT (size > 0);
  if (size <= JMEM_ALIGNMENT * JMEM_POOLS_COUNT)
  {
    jmem_pools_chunk_t **const chunk_lists_p = JERRY_CONTEXT (jmem_free_chunk_lists);
    const size_t offset = (size - 1) >> JMEM_ALIGNMENT_LOG;
    JERRY_ASSERT (offset < JMEM_POOLS_COUNT);

    const jmem_pools_chunk_t *const chunk_p = chunk_lists_p[offset];
    if (chunk_p != NULL)
    {
      JMEM_VALGRIND_DEFINED_SPACE (chunk_p, sizeof (jmem_pools_chunk_t));
      chunk_lists_p[offset] = chunk_p->next_p;
      JMEM_VALGRIND_UNDEFINED_SPACE (chunk_p, size);
      JMEM_HEAP_STAT_ALLOC (size);

      return (void *) chunk_p;
    }
  }

  return NULL;
} /* jmem_heap_pool_alloc */

/**
 * TODO
 */
inline static void JERRY_ATTR_HOT JERRY_ATTR_ALWAYS_INLINE
jmem_heap_pool_free (void *ptr,
                     const size_t size)
{
  JERRY_ASSERT (size > 0);
  JERRY_ASSERT (size <= JMEM_ALIGNMENT * JMEM_POOLS_COUNT);

  jmem_pools_chunk_t **const chunk_lists_p = JERRY_CONTEXT (jmem_free_chunk_lists);
  const size_t offset = (size - 1) >> JMEM_ALIGNMENT_LOG;
  JERRY_ASSERT (offset < JMEM_POOLS_COUNT);

  jmem_pools_chunk_t *const chunk_to_free_p = (jmem_pools_chunk_t *) ptr;

  JMEM_VALGRIND_DEFINED_SPACE (chunk_to_free_p, size);
  chunk_to_free_p->next_p = chunk_lists_p[offset];
  chunk_lists_p[offset] = chunk_to_free_p;
  JMEM_VALGRIND_NOACCESS_SPACE (chunk_to_free_p, size);
} /* jmem_heap_pool_free */

/**
 * TODO
 */
inline static void * JERRY_ATTR_HOT JERRY_ATTR_ALWAYS_INLINE
jmem_heap_alloc_loop (const size_t size,
                      const jmem_pressure_t max_pressure)
{
  jmem_pressure_t pressure = JMEM_PRESSURE_NONE;

#if !ENABLED (JERRY_MEM_GC_BEFORE_EACH_ALLOC)
  if (JERRY_UNLIKELY (size >= JERRY_CONTEXT (jmem_gc_limit)))
  {
    ecma_free_unused_memory (++pressure);
  }
#else /* ENABLED (JERRY_MEM_GC_BEFORE_EACH_ALLOC) */
  pressure = JMEM_PRESSURE_HIGH;
#endif /* ENABLED (JERRY_MEM_GC_BEFORE_EACH_ALLOC) */


  while (true)
  {
    void *const block_p = jmem_heap_alloc_internal (size);
    if (JERRY_LIKELY (block_p != NULL))
    {
      JMEM_HEAP_STAT_ALLOC (size);
      return block_p;
    }

    if (JERRY_UNLIKELY (pressure >= max_pressure))
    {
      break;
    }

    ecma_free_unused_memory (++pressure);
  }

  return NULL;
} /* jmem_heap_alloc_loop */

/**
 * TODO
 */
void * JERRY_ATTR_HOT
jmem_heap_alloc_loop_wrapped (const size_t size,
                              const jmem_pressure_t max_pressure)
{
  return jmem_heap_alloc_loop (size, max_pressure);
} /* jmem_heap_alloc_loop_wrapped */

/**
 * Allocation of memory block, reclaiming unused memory if there is not enough.
 *
 * Note:
 *      If a sufficiently sized block can't be found, the engine will be terminated with ERR_OUT_OF_MEMORY.
 *
 * @return NULL, if the required memory is 0
 *         pointer to allocated memory block, otherwise
 */
extern void * JERRY_ATTR_HOT
jmem_heap_alloc (const size_t size) /**< required memory size */
{
#if ENABLED (JERRY_MEM_GC_BEFORE_EACH_ALLOC)
  ecma_free_unused_memory (JMEM_PRESSURE_HIGH);
#endif /* !ENABLED (JERRY_MEM_GC_BEFORE_EACH_ALLOC) */

  void *const block_p = jmem_heap_pool_alloc (size);

  if (block_p != NULL)
  {
    return block_p;
  }

  return jmem_heap_alloc_loop (size, JMEM_PRESSURE_FULL);
} /* jmem_heap_alloc */

/**
 * Allocation of memory block, reclaiming unused memory if there is not enough.
 *
 * Note:
 *      If a sufficiently sized block can't be found, NULL will be returned.
 *
 * @return NULL, if the required memory size is 0
 *         also NULL, if the allocation has failed
 *         pointer to the allocated memory block, otherwise
 */
void * JERRY_ATTR_HOT
jmem_heap_alloc_maybe_null (const size_t size) /**< required memory size */
{
#if ENABLED (JERRY_MEM_GC_BEFORE_EACH_ALLOC)
  ecma_free_unused_memory (JMEM_PRESSURE_HIGH);
#endif /* !ENABLED (JERRY_MEM_GC_BEFORE_EACH_ALLOC) */
  void *const block_p = jmem_heap_pool_alloc (size);

  if (block_p != NULL)
  {
    return block_p;
  }

  return jmem_heap_alloc_loop (size, JMEM_PRESSURE_HIGH);
} /* jmem_heap_alloc_maybe_null */

/**
 * TODO
 */
inline void * JERRY_ATTR_HOT JERRY_ATTR_ALWAYS_INLINE
jmem_heap_alloc_const (const size_t size)
{
#if ENABLED (JERRY_MEM_GC_BEFORE_EACH_ALLOC)
  ecma_free_unused_memory (JMEM_PRESSURE_HIGH);
#endif /* !ENABLED (JERRY_MEM_GC_BEFORE_EACH_ALLOC) */

  void *const block_p = jmem_heap_pool_alloc (size);

  if (block_p != NULL)
  {
    return block_p;
  }

  return jmem_heap_alloc_loop_wrapped (size, JMEM_PRESSURE_HIGH);
} /* jmem_heap_alloc_const */

/**
 * Reallocates the memory region pointed to by 'ptr', changing the size of the allocated region.
 *
 * @return pointer to the reallocated region
 */
void * JERRY_ATTR_HOT
jmem_heap_realloc (void *ptr, /**< memory region to reallocate */
                   const size_t old_size, /**< current size of the region */
                   const size_t new_size) /**< desired new size */
{
  const size_t aligned_new_size = JERRY_ALIGNUP (new_size, JMEM_ALIGNMENT);
  const size_t aligned_old_size = JERRY_ALIGNUP (old_size, JMEM_ALIGNMENT);

#if !ENABLED (JERRY_SYSTEM_ALLOCATOR)
  JERRY_ASSERT (jmem_is_heap_pointer (ptr));
  JERRY_ASSERT ((uintptr_t) ptr % JMEM_ALIGNMENT == 0);
  JERRY_ASSERT (old_size != 0);
  JERRY_ASSERT (new_size != 0);

  jmem_heap_free_t * const block_p = (jmem_heap_free_t *) ptr;

  if (aligned_old_size == aligned_new_size)
  {
    JMEM_VALGRIND_RESIZE_SPACE (block_p, old_size, new_size);
    JMEM_HEAP_STAT_FREE (old_size);
    JMEM_HEAP_STAT_ALLOC (new_size);
    return block_p;
  }

  if (aligned_new_size < aligned_old_size)
  {
    JMEM_VALGRIND_RESIZE_SPACE (block_p, old_size, new_size);
    JMEM_HEAP_STAT_FREE (old_size);
    JMEM_HEAP_STAT_ALLOC (new_size);
    const size_t remaining_size = aligned_old_size - aligned_new_size;
    jmem_heap_insert_block ((jmem_heap_free_t *)((uint8_t *) block_p + aligned_new_size),
                            jmem_heap_find_prev (block_p),
                            remaining_size);

    return block_p;
  }

  void *ret_block_p = NULL;
  const size_t required_size = aligned_new_size - aligned_old_size;

#if !ENABLED (JERRY_MEM_GC_BEFORE_EACH_ALLOC)
  if (required_size >= JERRY_CONTEXT (jmem_gc_limit))
  {
    ecma_free_unused_memory (JMEM_PRESSURE_LOW);
  }
#else /* ENABLED (JERRY_MEM_GC_BEFORE_EACH_ALLOC( */
  ecma_free_unused_memory (JMEM_PRESSURE_HIGH)
#endif /* ENABLED (JERRY_MEM_GC_BEFORE_EACH_ALLOC) */

  jmem_heap_free_t *prev_p = jmem_heap_find_prev (block_p);
  JMEM_VALGRIND_DEFINED_SPACE (prev_p, sizeof (jmem_heap_free_t));
  jmem_heap_free_t * const next_p = JMEM_HEAP_GET_ADDR_FROM_OFFSET (prev_p->next_offset);

  /* Check if block can be extended at the end */
  if (((jmem_heap_free_t *) ((uint8_t *) block_p + aligned_old_size)) == next_p)
  {
    JMEM_VALGRIND_DEFINED_SPACE (next_p, sizeof (jmem_heap_free_t));

    if (required_size <= next_p->size)
    {
      /* Block can be extended, update the list. */
      if (required_size == next_p->size)
      {
        prev_p->next_offset = next_p->next_offset;
      }
      else
      {
        jmem_heap_free_t *const new_next_p = (jmem_heap_free_t *) ((uint8_t *) next_p + required_size);
        JMEM_VALGRIND_DEFINED_SPACE (new_next_p, sizeof (jmem_heap_free_t));
        new_next_p->next_offset = next_p->next_offset;
        new_next_p->size = (uint32_t) (next_p->size - required_size);
        JMEM_VALGRIND_NOACCESS_SPACE (new_next_p, sizeof (jmem_heap_free_t));
        prev_p->next_offset = JMEM_HEAP_GET_OFFSET_FROM_ADDR (new_next_p);
      }

      /* next_p will be marked as undefined space. */
      JMEM_VALGRIND_RESIZE_SPACE (block_p, old_size, new_size);
      ret_block_p = block_p;
    }
    else
    {
      JMEM_VALGRIND_NOACCESS_SPACE (next_p, sizeof (jmem_heap_free_t));
    }

    JMEM_VALGRIND_NOACCESS_SPACE (prev_p, sizeof (jmem_heap_free_t));
  }
  /*
   * Check if block can be extended at the front.
   * This is less optimal because we need to copy the data, but still better than allocting a new block.
   */
  else if (jmem_heap_get_region_end (prev_p) == block_p)
  {
    if (required_size <= prev_p->size)
    {
      if (required_size == prev_p->size)
      {
        JMEM_VALGRIND_NOACCESS_SPACE (prev_p, sizeof (jmem_heap_free_t));
        prev_p = jmem_heap_find_prev (prev_p);
        JMEM_VALGRIND_DEFINED_SPACE (prev_p, sizeof (jmem_heap_free_t));
        prev_p->next_offset = JMEM_HEAP_GET_OFFSET_FROM_ADDR (next_p);
      }
      else
      {
        prev_p->size = (uint32_t) (prev_p->size - required_size);
      }

      JMEM_VALGRIND_NOACCESS_SPACE (prev_p, sizeof (jmem_heap_free_t));

      ret_block_p = (uint8_t *) block_p - required_size;

      /* Mark the the new block as undefined so that we are able to write to it. */
      JMEM_VALGRIND_UNDEFINED_SPACE (ret_block_p, old_size);
      /* The blocks are likely to overlap, so mark the old block as defined memory again. */
      JMEM_VALGRIND_DEFINED_SPACE (block_p, old_size);
      memmove (ret_block_p, block_p, old_size);

      JMEM_VALGRIND_FREELIKE_SPACE (block_p);
      JMEM_VALGRIND_MALLOCLIKE_SPACE (ret_block_p, new_size);
      JMEM_VALGRIND_DEFINED_SPACE (ret_block_p, old_size);
    }
    else
    {
      JMEM_VALGRIND_NOACCESS_SPACE (prev_p, sizeof (jmem_heap_free_t));
    }
  }

  if (ret_block_p != NULL)
  {
    /* Managed to extend the block, update memory usage. */
    size_t gc_limit = JERRY_CONTEXT (jmem_gc_limit);
    while (gc_limit < required_size)
    {
      gc_limit += CONFIG_GC_LIMIT;
    }
    JERRY_CONTEXT (jmem_gc_limit) = gc_limit - required_size;
#ifndef JERRY_NDEBUG
    JERRY_CONTEXT (jmem_heap_allocated_size) += required_size;
#endif /* !JERRY_NDEBUG */

    JMEM_HEAP_STAT_FREE (old_size);
    JMEM_HEAP_STAT_ALLOC (new_size);
    return ret_block_p;
  }

  /* Could not extend block. Allocate new region and copy the data. */
  ret_block_p = jmem_heap_alloc (new_size);
  memcpy (ret_block_p, block_p, old_size);
  jmem_heap_free (block_p, aligned_old_size);

  return ret_block_p;
#else /* ENABLED (JERRY_SYSTEM_ALLOCATOR) */
  const size_t required_size = aligned_new_size - aligned_old_size;

#if !ENABLED (JERRY_MEM_GC_BEFORE_EACH_ALLOC)
  if (required_size >= JERRY_CONTEXT (jmem_gc_limit))
  {
    ecma_free_unused_memory (JMEM_PRESSURE_LOW);
  }
#else /* ENABLED (JERRY_MEM_GC_BEFORE_EACH_ALLOC( */
  ecma_free_unused_memory (JMEM_PRESSURE_HIGH)
#endif /* ENABLED (JERRY_MEM_GC_BEFORE_EACH_ALLOC) */

  size_t gc_limit = JERRY_CONTEXT (jmem_gc_limit);
  while (gc_limit < required_size)
  {
    gc_limit += CONFIG_GC_LIMIT;
  }
  JERRY_CONTEXT (jmem_gc_limit) = gc_limit - required_size;
#ifndef JERRY_NDEBUG
  JERRY_CONTEXT (jmem_heap_allocated_size) += required_size;
#endif /* !JERRY_NDEBUG */

  JMEM_HEAP_STAT_FREE (aligned_old_size);
  JMEM_HEAP_STAT_ALLOC (aligned_new_size);
  return realloc (ptr, aligned_new_size);
#endif /* !ENABLED (JERRY_SYSTEM_ALLOCATOR) */
} /* jmem_heap_realloc */

/**
 * Free memory block
 */
void JERRY_ATTR_HOT
jmem_heap_free (void *ptr, /**< pointer to beginning of data space of the block */
                const size_t size) /**< size of allocated region */
{
  JMEM_HEAP_STAT_FREE (size);

  if (size <= JMEM_ALIGNMENT * JMEM_POOLS_COUNT)
  {
    jmem_heap_pool_free (ptr, size);
    return;
  }

  jmem_heap_free_internal (ptr, size);
} /* jmem_heap_free */

/**
 * Free memory block
 */
inline void JERRY_ATTR_HOT JERRY_ATTR_ALWAYS_INLINE
jmem_heap_free_const (void *ptr, /**< pointer to beginning of data space of the block */
                      const size_t size) /**< size of allocated region */
{
  JMEM_HEAP_STAT_FREE (size);

  if (size <= JMEM_ALIGNMENT * JMEM_POOLS_COUNT)
  {
    jmem_heap_pool_free (ptr, size);
    return;
  }

  jmem_heap_free_internal (ptr, size);
} /* jmem_heap_free_const */

/**
 * Reclaim unused pool chunks
 */
void
jmem_heap_reclaim_pools (void)
{
  jmem_pools_chunk_t **const chunk_lists_p = JERRY_CONTEXT (jmem_free_chunk_lists);

  for (uint32_t i = 0; i < JMEM_POOLS_COUNT; i++)
  {
    jmem_pools_chunk_t *chunk_p = chunk_lists_p[i];
    const size_t size = (i + 1) * JMEM_ALIGNMENT;

    while (chunk_p != NULL)
    {
      JMEM_VALGRIND_DEFINED_SPACE (chunk_p, sizeof (jmem_pools_chunk_t));
      jmem_pools_chunk_t *const next_p = chunk_p->next_p;
      JMEM_VALGRIND_NOACCESS_SPACE (chunk_p, sizeof (jmem_pools_chunk_t));

      jmem_heap_free_internal (chunk_p, size);
      chunk_p = next_p;
    }

    chunk_lists_p[i] = NULL;
  }
} /* jmem_heap_recalim_pools */

#if !ENABLED (JERRY_SYSTEM_ALLOCATOR)
/**
 * TODO
 */
void
jmem_heap_defragment (void)
{
  JMEM_VALGRIND_DEFINED_SPACE (&JERRY_HEAP_CONTEXT (first), sizeof (jmem_heap_free_t));

  uint32_t current_offset = JERRY_HEAP_CONTEXT (first).next_offset;
  jmem_heap_free_t *prev_p = &JERRY_HEAP_CONTEXT (first);

  while (JERRY_LIKELY (current_offset != JMEM_HEAP_END_OF_LIST))
  {
    jmem_heap_free_t *const current_p = JMEM_HEAP_GET_ADDR_FROM_OFFSET (current_offset);
    JERRY_ASSERT (jmem_is_heap_pointer (current_p));
    JMEM_VALGRIND_DEFINED_SPACE (current_p, sizeof (jmem_heap_free_t));

    const uint32_t next_offset = current_p->next_offset;
    JERRY_ASSERT (next_offset == JMEM_HEAP_END_OF_LIST
                  || jmem_is_heap_pointer (JMEM_HEAP_GET_ADDR_FROM_OFFSET (next_offset)));

    current_offset = next_offset;
    const size_t current_size = current_p->size;

    if (current_size <= JMEM_ALIGNMENT * JMEM_POOLS_COUNT)
    {
      prev_p->next_offset = next_offset;
      jmem_heap_pool_free (current_p, current_size);
#ifndef JERRY_NDEBUG
      JERRY_CONTEXT (jmem_heap_allocated_size) += current_size;
#endif /* !JERRY_NDEBUG */

      continue;
    }

    JMEM_VALGRIND_NOACCESS_SPACE (current_p, sizeof (jmem_heap_free_t));
    /* Next in list. */
    prev_p = current_p;
  }

  JERRY_CONTEXT (jmem_heap_list_skip_p) = &JERRY_HEAP_CONTEXT (first);
  JMEM_VALGRIND_NOACCESS_SPACE (&JERRY_HEAP_CONTEXT (first), sizeof (jmem_heap_free_t));
} /* jmem_heap_defragment */
#endif /* !ENABLED (JERRY_SYSTEM_ALLOCATOR) */

#ifndef JERRY_NDEBUG
/**
 * Check whether the pointer points to the heap
 *
 * Note:
 *      the routine should be used only for assertion checks
 *
 * @return true - if pointer points to the heap,
 *         false - otherwise
 */
bool
jmem_is_heap_pointer (const void *pointer) /**< pointer */
{
#if !ENABLED (JERRY_SYSTEM_ALLOCATOR)
  return ((uint8_t *) pointer >= JERRY_HEAP_CONTEXT (area)
          && (uint8_t *) pointer <= (JERRY_HEAP_CONTEXT (area) + JMEM_HEAP_AREA_SIZE));
#else /* ENABLED (JERRY_SYSTEM_ALLOCATOR) */
  JERRY_UNUSED (pointer);
  return true;
#endif /* !ENABLED (JERRY_SYSTEM_ALLOCATOR) */
} /* jmem_is_heap_pointer */
#endif /* !JERRY_NDEBUG */

#if ENABLED (JERRY_MEM_STATS)
/**
 * Get heap memory usage statistics
 */
void
jmem_heap_get_stats (jmem_heap_stats_t *out_heap_stats_p) /**< [out] heap stats */
{
  JERRY_ASSERT (out_heap_stats_p != NULL);

  *out_heap_stats_p = JERRY_CONTEXT (jmem_heap_stats);
} /* jmem_heap_get_stats */

/**
 * Print heap memory usage statistics
 */
void
jmem_heap_stats_print (void)
{
  jmem_heap_stats_t *heap_stats = &JERRY_CONTEXT (jmem_heap_stats);

  JERRY_DEBUG_MSG ("Heap stats:\n");
#if !ENABLED (JERRY_SYSTEM_ALLOCATOR)
  JERRY_DEBUG_MSG ("  Heap size = %zu bytes\n",
                   heap_stats->size);
#endif /* !ENABLED (JERRY_SYSTEM_ALLOCATOR) */
  JERRY_DEBUG_MSG ("  Allocated = %zu bytes\n"
                   "  Peak allocated = %zu bytes\n"
                   "  Waste = %zu bytes\n"
                   "  Peak waste = %zu bytes\n"
                   "  Allocated byte code data = %zu bytes\n"
                   "  Peak allocated byte code data = %zu bytes\n"
                   "  Allocated string data = %zu bytes\n"
                   "  Peak allocated string data = %zu bytes\n"
                   "  Allocated object data = %zu bytes\n"
                   "  Peak allocated object data = %zu bytes\n"
                   "  Allocated property data = %zu bytes\n"
                   "  Peak allocated property data = %zu bytes\n",
                   heap_stats->allocated_bytes,
                   heap_stats->peak_allocated_bytes,
                   heap_stats->waste_bytes,
                   heap_stats->peak_waste_bytes,
                   heap_stats->byte_code_bytes,
                   heap_stats->peak_byte_code_bytes,
                   heap_stats->string_bytes,
                   heap_stats->peak_string_bytes,
                   heap_stats->object_bytes,
                   heap_stats->peak_object_bytes,
                   heap_stats->property_bytes,
                   heap_stats->peak_property_bytes);
} /* jmem_heap_stats_print */

/**
 * Initalize heap memory usage statistics account structure
 */
void
jmem_heap_stat_init (void)
{
#if !ENABLED (JERRY_SYSTEM_ALLOCATOR)
  JERRY_CONTEXT (jmem_heap_stats).size = JMEM_HEAP_AREA_SIZE;
#endif /* !ENABLED (JERRY_SYSTEM_ALLOCATOR) */
} /* jmem_heap_stat_init */

/**
 * Account allocation
 */
void
jmem_heap_stat_alloc (size_t size) /**< Size of allocated block */
{
  const size_t aligned_size = (size + JMEM_ALIGNMENT - 1) / JMEM_ALIGNMENT * JMEM_ALIGNMENT;
  const size_t waste_bytes = aligned_size - size;

  jmem_heap_stats_t *heap_stats = &JERRY_CONTEXT (jmem_heap_stats);

  heap_stats->allocated_bytes += aligned_size;
  heap_stats->waste_bytes += waste_bytes;

  if (heap_stats->allocated_bytes > heap_stats->peak_allocated_bytes)
  {
    heap_stats->peak_allocated_bytes = heap_stats->allocated_bytes;
  }

  if (heap_stats->waste_bytes > heap_stats->peak_waste_bytes)
  {
    heap_stats->peak_waste_bytes = heap_stats->waste_bytes;
  }
} /* jmem_heap_stat_alloc */

/**
 * Account freeing
 */
void
jmem_heap_stat_free (size_t size) /**< Size of freed block */
{
  const size_t aligned_size = (size + JMEM_ALIGNMENT - 1) / JMEM_ALIGNMENT * JMEM_ALIGNMENT;
  const size_t waste_bytes = aligned_size - size;

  jmem_heap_stats_t *heap_stats = &JERRY_CONTEXT (jmem_heap_stats);

  heap_stats->allocated_bytes -= aligned_size;
  heap_stats->waste_bytes -= waste_bytes;
} /* jmem_heap_stat_free */

#endif /* ENABLED (JERRY_MEM_STATS) */

/**
 * @}
 * @}
 */
