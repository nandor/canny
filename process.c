#include <stdio.h>
#include <string.h>
#include <GL/glxew.h>
#include "process.h"
#include "program.h"

int
initProcess(struct process * proc)
{    
	cl_int err;
  cl_uint count;
  cl_device_id dev;
	cl_platform_id platform;
  size_t log, i;
  char * tmp;

  /* Retrieve device information */
  clGetPlatformIDs(1, &platform, &count);
  if (count <= 0) 
  {
    fprintf(stderr, "OpenCL: platforms found!\n");
    return 0;
  }

  clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &dev, &count);
  if (count <= 0)
  {
    fprintf(stderr, "OpenCL: devices found!\n");
    return 0;
  }
	
  clGetDeviceInfo(dev, CL_DEVICE_EXTENSIONS, 0, NULL, &i);
  if (!(tmp = malloc(sizeof(char) * (i + 1))) || 
      clGetDeviceInfo(dev, CL_DEVICE_EXTENSIONS, i, tmp, NULL) != CL_SUCCESS)
  {
    if (tmp)
    {
      free(tmp);
    }

    fprintf(stderr, "OpenCL: Cannot retrieve extensions\n");
    return 0;
  }

  tmp[i] = '\0';
  if (!strstr(tmp, "cl_khr_gl_sharing") && !strstr(tmp, "cl_APPLE_gl_sharing"))
  {
    free(tmp);
    fprintf(stderr, "OpenCL: cl_khr_gl_sharing unavailable\n");
    return 0;
  }
  
  /* Create the OpenCL context */
  const cl_context_properties prop[] =
  {
    CL_CONTEXT_PLATFORM, (cl_context_properties)platform,
    CL_GL_CONTEXT_KHR,   (cl_context_properties)glXGetCurrentContext(),
    CL_GLX_DISPLAY_KHR,  (cl_context_properties)glXGetCurrentDisplay(),
    0
  };

  free(tmp);
	if (!(proc->context = clCreateContext(prop, 1, &dev, NULL, NULL, &err)))
	{
		fprintf(stderr, "OpenCL: Cannot create context (%d)\n", err);
		return 0;
	}	

  if (!(proc->queue = clCreateCommandQueue(proc->context, dev, 0, &err)))
  {
    fprintf(stderr, "OpenCL: Cannot create command queue\n");
    return 0;
  }

  /* Build the program */
  tmp = (char*)program_cl;
  if (!(proc->prog = clCreateProgramWithSource(proc->context, 1, 
                                               (const char**)&tmp,
                                               (const size_t*)&program_cl_len, 
                                               &err)))
  {
    fprintf(stderr, "OpenCL: Cannot create program\n");
    return 0;
  }

  tmp = "-Werror";
  if (clBuildProgram(proc->prog, 1, &dev, tmp, NULL, NULL) != CL_SUCCESS)
  {
    clGetProgramBuildInfo(proc->prog, dev, CL_PROGRAM_BUILD_LOG, 0, NULL, &log);
    if ((tmp = (char*)malloc(sizeof(char) * (log + 1))) == NULL)
    {
      fprintf(stderr, "OpenCL: Cannot build program\n");
      return 0;
    }

    clGetProgramBuildInfo(proc->prog, dev, CL_PROGRAM_BUILD_LOG, log, tmp, 0);
    fprintf(stderr, "OpenCL: \n%s\n", tmp);
    free(tmp);
    return 0;
  }
  
  /* Retrieve the kernels */
  const char * kernels[] = { 
      "krnBlur", "krnSobel", "krnNMS", 
      "krnHysteresis", "krnFinal"
  };

  for (i = 0; i < sizeof(kernels) / sizeof(kernels[0]); ++i) 
  {
    if (!(proc->kernels[i] = clCreateKernel(proc->prog, kernels[i], &err)))
    {
      fprintf(stderr, "OpenCL: Cannot create kernel '%s'\n", kernels[i]);
      return 0;
    }
  }

  /* Initialise the output image */
  glGenTextures(1, &proc->output);
  glBindTexture(GL_TEXTURE_2D, proc->output);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, proc->width, proc->height, 0,
               GL_RGB, GL_UNSIGNED_BYTE, NULL);
  glBindTexture(GL_TEXTURE_2D, 0);
  glFinish();

  if (!(proc->out = clCreateFromGLTexture2D(proc->context, CL_MEM_READ_WRITE,
                                            GL_TEXTURE_2D, 0, proc->output,
                                            &err)))
  {
    fprintf(stderr, "OpenCL: Cannot create buffer (%d)\n", err);
    return 0;
  }

  /* Create the buffers */
  cl_image_format fmt = { CL_RGBA, CL_UNORM_INT8 };
  for (i = 1; i < sizeof(proc->images) / sizeof(proc->images[0]); ++i)
  {
    if (!(proc->images[i] = clCreateImage2D(proc->context, CL_MEM_READ_WRITE,
                                            &fmt, proc->width, proc->height,
                                            0, NULL, &err)))
    {
      fprintf(stderr, "OpenCL: Cannot create buffer (%d)\n", err);
      return 0;
    }
  }

  return 1;
}

void 
processImage(struct process *proc, uint8_t *data)
{
  size_t workSize[] = { proc->width, proc->height, 1 };
  size_t orig[] = { 0, 0, 0 };

  clEnqueueAcquireGLObjects(proc->queue, 1, &proc->out, 0, NULL, NULL);

  /* Upload the source image */
  clEnqueueWriteImage(proc->queue, proc->input, CL_FALSE, orig, workSize, 
                      0, 0, data, 0, NULL, NULL);

  /* Blur it */
  clSetKernelArg(proc->krnBlur, 0, sizeof(cl_mem), &proc->input);
  clSetKernelArg(proc->krnBlur, 1, sizeof(cl_mem), &proc->blur);
  clEnqueueNDRangeKernel(proc->queue, proc->krnBlur, 2, NULL, 
                         workSize, NULL, 0, NULL, NULL);  
  
  clSetKernelArg(proc->krnSobel, 0, sizeof(cl_mem), &proc->blur);
  clSetKernelArg(proc->krnSobel, 1, sizeof(cl_mem), &proc->sobel);
  clEnqueueNDRangeKernel(proc->queue, proc->krnSobel, 2, NULL, 
                         workSize, NULL, 0, NULL, NULL);  
  
  clSetKernelArg(proc->krnNMS, 0, sizeof(cl_mem), &proc->sobel);
  clSetKernelArg(proc->krnNMS, 1, sizeof(cl_mem), &proc->nms);
  clEnqueueNDRangeKernel(proc->queue, proc->krnNMS, 2, NULL, 
                         workSize, NULL, 0, NULL, NULL);  
  
  clSetKernelArg(proc->krnHysteresis, 0, sizeof(cl_mem), &proc->nms);
  clSetKernelArg(proc->krnHysteresis, 1, sizeof(cl_mem), &proc->edges);
  clEnqueueNDRangeKernel(proc->queue, proc->krnHysteresis, 2, NULL, 
                         workSize, NULL, 0, NULL, NULL);  
  
  clSetKernelArg(proc->krnFinal, 0, sizeof(cl_mem), &proc->edges);
  clSetKernelArg(proc->krnFinal, 1, sizeof(cl_mem), &proc->input);
  clSetKernelArg(proc->krnFinal, 2, sizeof(cl_mem), &proc->out);
  clEnqueueNDRangeKernel(proc->queue, proc->krnFinal, 2, NULL, 
                         workSize, NULL, 0, NULL, NULL); 
  
  clEnqueueReleaseGLObjects(proc->queue, 1, &proc->out, 0, NULL, NULL);
  clFinish(proc->queue);
}

void
destroyProcess(struct process *proc)
{
  size_t i;

  for (i = 0; i < sizeof(proc->images) / sizeof(proc->images[0]); ++i)
  {
    if (proc->images[i]) 
    {
      clReleaseMemObject(proc->images[i]);
      proc->images[i] = 0;
    }
  }

  for (i = 0; i < sizeof(proc->kernels) / sizeof(proc->kernels[0]); ++i)
  {
    if (proc->kernels[i]) 
    {
      clReleaseKernel(proc->kernels[i]);
      proc->kernels[i] = 0;
    }
  }

  if (proc->queue)
  {
    clReleaseCommandQueue(proc->queue);
    proc->queue = 0;
  }

  if (proc->prog)
  {
    clReleaseProgram(proc->prog);
    proc->prog = 0;
  }

	if (proc->context)
	{
		clReleaseContext(proc->context);
		proc->context = 0;
	}

  if (proc->output)
  {
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &proc->output);
    proc->output = 0;
  }
}
