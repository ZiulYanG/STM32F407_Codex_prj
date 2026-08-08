/**
  ******************************************************************************
  * @file    syscalls.c
  * @brief   Minimal newlib system call stubs for GCC ARM Embedded
  ******************************************************************************
  */
#include <sys/stat.h>
#include <errno.h>

extern int __io_putchar(int ch) __attribute__((weak));
extern int __io_getchar(void) __attribute__((weak));

int _write(int fd, char *ptr, int len)
{
  (void)fd;
  int i;

  for (i = 0; i < len; i++)
  {
    __io_putchar(*ptr++);
  }
  return len;
}

int _read(int fd, char *ptr, int len)
{
  (void)fd;
  int i;

  for (i = 0; i < len; i++)
  {
    *ptr++ = (char)__io_getchar();
  }
  return len;
}

int _close(int fd)
{
  (void)fd;
  return -1;
}

int _lseek(int fd, int ptr, int dir)
{
  (void)fd;
  (void)ptr;
  (void)dir;
  return 0;
}

int _fstat(int fd, struct stat *st)
{
  (void)fd;
  st->st_mode = S_IFCHR;
  return 0;
}

int _isatty(int fd)
{
  (void)fd;
  return 1;
}

void _exit(int status)
{
  (void)status;
  while (1)
  {
  }
}

int _kill(int pid, int sig)
{
  (void)pid;
  (void)sig;
  errno = EINVAL;
  return -1;
}

int _getpid(void)
{
  return 1;
}
