#ifndef __HOG_CAMERA_H__
#define __HOG_CAMERA_H__

#include <stdint.h>

struct buffer
{
  void *ptr;
  size_t length;
};

struct camera
{
  int fd;
  int capture;
  uint32_t width;
  uint32_t height;
  size_t size;
  uint32_t buffer_count;
  struct buffer *buffers;
  const char *camera;
};

int openCamera(struct camera *);
int startCamera(struct camera *);
int getFrame(struct camera *, uint8_t *);
void stopCamera(struct camera *);
void destroyCamera(struct camera *);

#endif /*__HOG_CAMERA_H__*/
