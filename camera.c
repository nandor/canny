#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include "camera.h"

/**
 * Resumes ioctl on interrupts
 */
static int
devctl(struct camera *dev, int req, void *arg)
{
  int ret;

  do {
    ret = ioctl(dev->fd, req, arg);
  } while (ret == -1 && errno == EINTR);

  return ret;
}

/**
 * Retrieves a handle to a camera
 */
int
openCamera(struct camera *dev)
{
  struct stat st;
  struct v4l2_capability cap;
  struct v4l2_format fmt;
  struct v4l2_requestbuffers req;
  struct v4l2_buffer buf;
  uint32_t i;

  /* Check whether the camera can be read from */
  if (stat(dev->camera, &st) == -1 || !S_ISCHR(st.st_mode))
  {
    return 0;
  }

  /* Open the camera */
  if ((dev->fd = open(dev->camera, O_RDWR | O_NONBLOCK, 0)) < 0)
  {
    return 0;
  }

  /* Do not capture */
  dev->capture = 0;

  /* Check whether the camera can support memory mapped IO */
  memset(&cap, 0, sizeof(cap));
  if (devctl(dev, VIDIOC_QUERYCAP, &cap) < 0 ||
      !(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) ||
      !(cap.capabilities & V4L2_CAP_STREAMING))
  {
    return 0;
  }

  /* Retrieve the image format */
  memset(&fmt, 0, sizeof(fmt));
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (devctl(dev, VIDIOC_G_FMT, &fmt) < 0)
  {
    return 0;
  }

  dev->width = fmt.fmt.pix.width;
  dev->height = fmt.fmt.pix.height;
  dev->size = fmt.fmt.pix.sizeimage;
  dev->type = fmt.fmt.pix.colorspace;
  dev->format = fmt.fmt.pix.pixelformat;

  /* Request 4 buffers */
  memset(&req, 0, sizeof(req));
  req.count = 4;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;
  if (devctl(dev, VIDIOC_REQBUFS, &req) < 0 || req.count < 2)
  {
    return 0;
  }

  /* Initialise the buffers */
  dev->buffer_count = req.count;
  dev->buffers = (struct buffer*)malloc(sizeof(struct buffer) * req.count);
  memset(dev->buffers, 0, sizeof(struct buffer) * req.count);
  for (i = 0; i < dev->buffer_count; ++i)
  {
    dev->buffers[i].ptr = MAP_FAILED;
  }

  /* mmap all the buffers */
  for (i = 0; i < dev->buffer_count; ++i)
  {
    memset(&buf, 0, sizeof(buf));
    buf.index = i;
    buf.type = req.type;
    buf.memory = V4L2_MEMORY_MMAP;

    if (devctl(dev, VIDIOC_QUERYBUF, &buf) < 0)
    {
      return 0;
    }

    dev->buffers[i].length = buf.length;
    dev->buffers[i].ptr = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                               MAP_SHARED, dev->fd, buf.m.offset);
    if (dev->buffers[i].ptr == MAP_FAILED)
    {
      return 0;
    }
  }

  return 1;
}

/**
 * Destroys the camera
 * @param dev Device
 */
void
destroyCamera(struct camera *dev)
{
  uint32_t i;

  stopCamera(dev);
  if (dev->buffers)
  {
    for (i = 0; i < dev->buffer_count; ++i)
    {
      if (dev->buffers[i].ptr != MAP_FAILED)
      {
        munmap(dev->buffers[i].ptr, dev->buffers[i].length);
      }
    }

    free(dev->buffers);
    dev->buffers = NULL;
  }

  if (dev->fd >= 0)
  {
    close(dev->fd);
    dev->fd = -1;
  }
}

/**
 * Starts recording
 */
int
startCamera(struct camera *dev)
{
  enum v4l2_buf_type type;
  struct v4l2_buffer buf;
  uint32_t i;

  if (dev->capture)
  {
    return 1;
  }

  /* Queue all buffers */
  for (i = 0; i < dev->buffer_count; ++i)
  {
    memset(&buf, 0, sizeof(buf));
    buf.index = i;
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (devctl(dev, VIDIOC_QBUF, &buf) < 0)
    {
      return 0;
    }
  }

  /* Start streaming */
  type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (devctl(dev, VIDIOC_STREAMON, &type) < 0)
  {
    return 0;
  }

  dev->capture = 1;
  return 1;
}

/**
 * Clips a color to 0 - 255
 */
static inline uint8_t
clip(int color)
{
  return (color < 0) ? 0 : (color >= 0xFF ? 0xFF : color);
}

/**
 * Converts an YUYV source to RGB
 */
static void
yuyvToRGB(uint8_t *src, uint8_t *dest, uint32_t w, uint32_t h)
{
  uint32_t idx, sidx, i, j;
  int c0, c1, d, e;

  sidx = 0;
  idx = 0;
  for (i = 0; i < h; ++i)
  {
    for (j = 0; j < w >> 1; ++j)
    {
      c0 = src[sidx++] - 16;
      d  = src[sidx++] - 128;
      c1 = src[sidx++] - 16;
      e  = src[sidx++] - 128;

      dest[idx++] = clip((298 * c0           + 409 * e + 128) >> 8);
      dest[idx++] = clip((298 * c0 - 100 * d - 208 * e + 128) >> 8);
      dest[idx++] = clip((298 * c0 + 516 * d           + 128) >> 8);
      dest[idx++] = 0;

      dest[idx++] = clip((298 * c1           + 409 * e + 128) >> 8);
      dest[idx++] = clip((298 * c1 - 100 * d - 208 * e + 128) >> 8);
      dest[idx++] = clip((298 * c1 + 516 * d           + 128) >> 8);
      dest[idx++] = 0;
    }
  }
}

/**
 * Retrieves a frame from the camera
 */
int
getImage(struct camera *dev, uint8_t *dest)
{
  struct v4l2_buffer buf;
  struct timeval tv;
  fd_set fds;
  int r;

  if (!dev->capture || !dest)
  {
    return 0;
  }

  /* Repeat if VIDIOC_DQBUF fails with EAGAIN */
  do
  {
    /* Set up select */
    FD_ZERO(&fds);
    FD_SET(dev->fd, &fds);
    tv.tv_sec = 0;
    tv.tv_usec = 16000;
    if ((r = select(dev->fd + 1, &fds, NULL, NULL, &tv)) <= 0)
    {
      if (errno == EINTR)
      {
        continue;
      }
      return 0;
    }

    /* Deque the buffer & read from it */
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
  } while (devctl(dev, VIDIOC_DQBUF, &buf) < 0 && errno == EAGAIN);

  /* Invalid buffer */
  if (buf.index >= dev->buffer_count)
  {
    return 0;
  }

  /* Process the image */
  switch (dev->format)
  {
    case V4L2_PIX_FMT_YUYV:
    {
      yuyvToRGB(dev->buffers[buf.index].ptr, dest, dev->width, dev->height);
      break;
    }
    default:
    {
      return 0;
    }
  }

  /* Put the buffer back in the queue */
  if (devctl(dev, VIDIOC_QBUF, &buf) < 0)
  {
    return 0;
  }

  return 1;
}

/**
 * Stops the camera
 */
void
stopCamera(struct camera *dev)
{
  enum v4l2_buf_type type;

  if (!dev->capture)
  {
    return;
  }

  dev->capture = 0;
  type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  devctl(dev, VIDIOC_STREAMOFF, &type);
}
