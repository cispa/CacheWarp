/* See LICENSE file for license and copyright information */

#ifndef LEAKY_MODULE_H
#define LEAKY_MODULE_H

#include "stddef.h"

#define LEAKY_DEVICE_NAME "cachectl"
#define LEAKY_DEVICE_PATH "/dev/" LEAKY_DEVICE_NAME


#define LEAKY_IOCTL_MAGIC_NUMBER (long)0xc21

#define LEAKY_IOCTL_CMD_INVD \
  _IOR(LEAKY_IOCTL_MAGIC_NUMBER, 1, size_t)

#define LEAKY_IOCTL_CMD_WBNOINVD \
  _IOR(LEAKY_IOCTL_MAGIC_NUMBER, 2, size_t)

#define LEAKY_IOCTL_CMD_WBINVD \
  _IOR(LEAKY_IOCTL_MAGIC_NUMBER, 3, size_t)

#define LEAKY_IOCTL_CMD_CACHE_TEST \
  _IOR(LEAKY_IOCTL_MAGIC_NUMBER, 4, size_t)

#define LEAKY_IOCTL_CMD_INVD_TIMING \
  _IOR(LEAKY_IOCTL_MAGIC_NUMBER, 5, size_t)

  #endif // LEAKY_MODULE_H
