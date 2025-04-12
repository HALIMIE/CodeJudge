#pragma once
#ifndef _DEFINESHIT_H
#define _DEFINESHIT_H
#ifndef NULL
#define NULL 0
#endif

#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif

#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif

#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif

#ifndef SO_REUSEPORT
#define SO_REUSEPORT 15
#endif

#ifndef sig_atomic_t
typedef int sig_atomic_t;
#endif

#ifndef size_t
typedef unsigned long size_t;
#endif

#ifndef errno
extern int errno;
#endif
#endif // _DEFINESHIT_H