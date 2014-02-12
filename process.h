#ifndef __HOG_PROCESS_H__
#define __HOG_PROCESS_H__

#include <CL/cl.h>
#include <CL/cl_gl.h>
#include <GL/glew.h>

struct rect
{
  uint32_t x, y, w, h;
  struct rect * next;
};

struct process
{
  /* Texture size */
  uint32_t width;
  uint32_t height;

  /* Output texture */
  GLuint output;

  /* OpenCL state */
  cl_context context;
  cl_command_queue queue;
  cl_program prog;

  /* OpenCL kernels */
  union {
    cl_kernel kernels[4];
    struct {
      cl_kernel krnBlur;
      cl_kernel krnSobel;
      cl_kernel krnNMS;
      cl_kernel krnHysteresis;
    };
  };

  /* OpenCL buffers */
  union {
    cl_mem images[5];
    struct {
      cl_mem out; 
      cl_mem input;
      cl_mem blur;
      cl_mem sobel;
      cl_mem nms;
    };
  };
};

int initProcess(struct process *);
void processImage(struct process *, uint8_t *);
void destroyProcess(struct process *);

#endif /*__HOG_PROCESS_H__*/
