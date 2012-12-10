
/*-
 *
 * New BSD License 2006
 *
 * Copyright (c) 2006, Jorgen Lundman
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1 Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 2 Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 * 3 Neither the name of the stuff nor the names of its contributors
 *   may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef LION_TYPES_INCLUDED
#define LION_TYPES_INCLUDED


#ifdef WIN32
#define THREAD_SAFE __declspec(thread)
#else
#define THREAD_SAFE
#endif

#if HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_INTTYPES_H
#include <inttypes.h>
#endif
#if HAVE_STDINT_H
#include <stdint.h>
#endif


#if defined int64_t
typedef int64_t lion64_t;
#elif defined (WIN32)
typedef __int64 lion64_t;
#else // Last desperate ditch
typedef long long int lion64_t;
#endif

#if defined uint64_t
typedef uint64_t lion64u_t;
#elif defined (WIN32)
// Hah! error C2520: conversion from unsigned __int64 to double not implemented, use signed __int64
// So we make both be signed... but thats a bit crap.
// That's devstudio6, devstudio 8 has unsigned. FIXME.
typedef __int64 lion64u_t;
#else // Last desperate ditch
typedef unsigned long long int lion64u_t;
#endif


typedef lion64u_t bytes_t;


// Define the printable type. Unix std is from inttypes.h
// Should already be defined by the time we get here, but incase
// it is not, let them at least compile the code.

#ifdef WIN32
#ifndef PRIu64
#define PRIu64 "I64d"
#endif
#endif

#ifdef __FreeBSD__
#ifndef PRIu64
#define PRIu64 "qu"
#endif
#endif



#endif
