#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "camera.h"
#include "window.h"

/**
 * Entry point of the application
 */
int
main(int argc, char **argv)
{
  struct camera dev;
  struct window wnd;
  uint8_t *buf;

  memset(&dev, 0, sizeof(dev));
  dev.camera = (argc >= 2) ? argv[1] : "/dev/video0";
  if (!openCamera(&dev))
  {
    destroyCamera(&dev);
    fprintf(stderr, "Cannot open camera '%s'\n", dev.camera);
    return EXIT_FAILURE;
  }

  if (!(buf = (uint8_t*)malloc(dev.width * dev.height * 3)))
  {
    destroyCamera(&dev);
    fprintf(stderr, "Cannot allocate buffer");
    return EXIT_FAILURE;
  }

  memset(&wnd, 0, sizeof(wnd));
  wnd.width = dev.width;
  wnd.height = dev.height;
  if (!initWindow(&wnd))
  {
    destroyCamera(&dev);
    destroyWindow(&wnd);
    fprintf(stderr, "Cannot create window");
    free(buf);
    return EXIT_FAILURE;
  }

  startCamera(&dev);

  while (updateWindow(&wnd))
  {
    getFrame(&dev, buf);
    displayFrame(&wnd, buf);
  }

  stopCamera(&dev);

  free(buf);
  destroyWindow(&wnd);
  destroyCamera(&dev);
  return EXIT_SUCCESS;
}
