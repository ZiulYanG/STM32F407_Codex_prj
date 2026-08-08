/**
  ******************************************************************************
  * @file    sysmem.c
  * @brief   Memory management stub (_sbrk) for GCC ARM Embedded
  ******************************************************************************
  */
#include <errno.h>
#include <stdint.h>

/* Symbol defined by the linker script: end of the heap */
extern uint8_t end; /* Defined by the linker script */

static uint8_t *__sbrk_heap_end = NULL;

/**
  * @brief  _sbrk() allocates memory to the newlib heap
  * @param  incr: size of the memory block to allocate
  * @retval pointer to the allocated memory, or (void *)-1 on failure
  */
void *_sbrk(ptrdiff_t incr)
{
  extern uint8_t _estack;      /* Defined by the linker script */
  extern uint32_t _Min_Stack_Size; /* Defined by the linker script */

  const uint32_t stack_limit = (uint32_t)&_estack - (uint32_t)&_Min_Stack_Size;
  const uint8_t *max_heap = (uint8_t *)stack_limit;
  uint8_t *prev_heap_end;

  if (NULL == __sbrk_heap_end)
  {
    __sbrk_heap_end = &end;
  }

  if (__sbrk_heap_end + incr > max_heap)
  {
    errno = ENOMEM;
    return (void *)-1;
  }

  prev_heap_end = __sbrk_heap_end;
  __sbrk_heap_end += incr;

  return (void *)prev_heap_end;
}
