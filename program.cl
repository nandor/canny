/* OpenCL kernels for Canny Edge Detector */

/* Texture sampler */
__constant sampler_t sampler = CLK_FILTER_NEAREST |
                               CLK_NORMALIZED_COORDS_FALSE | 
                               CLK_ADDRESS_CLAMP_TO_EDGE;

/* Low threshold for hysteresis supression */
__constant float TLOW = 0.01;
/* High threshold for hysteresis supression */
__constant float THIGH = 0.3;

/* 3x3 kernel border offsets */
__constant int2 OFF3X3[] =
{
  (int2)(-1, -1), (int2)( 0, -1), (int2)(1, -1), 
  (int2)(-1,  0),                 (int2)(1,  0), 
  (int2)(-1,  1), (int2)( 0,  1), (int2)(1,  1),
};

/* 5x5 kernel border offsets */
__constant int2 OFF5X5[] =
{
  (int2)(-2, -2), (int2)(-1, -2), (int2)(0, -2), (int2)(1, -2), (int2)(2, -2),
  (int2)(-2, -1),                                               (int2)(2, -1), 
  (int2)(-2,  0),                                               (int2)(2,  0),
  (int2)(-2,  1),                                               (int2)(2,  1),
  (int2)(-2,  2), (int2)(-1,  2), (int2)(0,  2), (int2)(1,  2), (int2)(2,  2)
};


/**
 * Gaussian blur with a 5x5 kernel
 * This pass also converts an image to grayscale
 */
__kernel void krnBlur(__read_only image2d_t input,
                      __write_only image2d_t blur)
{
  int2 uv;
  float4 acc;

  uv = (int2){ get_global_id(0), get_global_id(1) };

  acc = (float4)(0.0);
  acc +=  2.0 * read_imagef(input, sampler, uv + (int2)(-2, -2));
  acc +=  4.0 * read_imagef(input, sampler, uv + (int2)(-1, -2));
  acc +=  5.0 * read_imagef(input, sampler, uv + (int2)( 0, -2));
  acc +=  4.0 * read_imagef(input, sampler, uv + (int2)( 1, -2));
  acc +=  2.0 * read_imagef(input, sampler, uv + (int2)( 2, -2));
  acc +=  4.0 * read_imagef(input, sampler, uv + (int2)(-2, -1));
  acc +=  9.0 * read_imagef(input, sampler, uv + (int2)(-1, -1));
  acc += 12.0 * read_imagef(input, sampler, uv + (int2)( 0, -1));
  acc +=  9.0 * read_imagef(input, sampler, uv + (int2)( 1, -1));
  acc +=  4.0 * read_imagef(input, sampler, uv + (int2)( 2, -1));
  acc +=  5.0 * read_imagef(input, sampler, uv + (int2)(-2,  0));
  acc += 12.0 * read_imagef(input, sampler, uv + (int2)(-1,  0));
  acc += 15.0 * read_imagef(input, sampler, uv + (int2)( 0,  0));
  acc += 12.0 * read_imagef(input, sampler, uv + (int2)( 1,  0));
  acc +=  5.0 * read_imagef(input, sampler, uv + (int2)( 2,  0));
  acc +=  4.0 * read_imagef(input, sampler, uv + (int2)(-2,  1));
  acc +=  9.0 * read_imagef(input, sampler, uv + (int2)(-1,  1));
  acc += 12.0 * read_imagef(input, sampler, uv + (int2)( 0,  1));
  acc +=  9.0 * read_imagef(input, sampler, uv + (int2)( 1,  1));
  acc +=  4.0 * read_imagef(input, sampler, uv + (int2)( 2,  1));
  acc +=  2.0 * read_imagef(input, sampler, uv + (int2)(-2,  2));
  acc +=  4.0 * read_imagef(input, sampler, uv + (int2)(-1,  2));
  acc +=  5.0 * read_imagef(input, sampler, uv + (int2)( 0,  2));
  acc +=  4.0 * read_imagef(input, sampler, uv + (int2)( 1,  2));
  acc +=  2.0 * read_imagef(input, sampler, uv + (int2)( 2,  2));

  write_imagef(blur, uv, length(acc / 159.0));
}

/**
 * Horizontal Sobel pass
 */
__kernel void krnSobel(__read_only image2d_t blur,
                       __write_only image2d_t sobel)
{
  int2 uv;
  float4 vert, horz, pix;
  float4 orig;

  uv = (int2){ get_global_id(0), get_global_id(1) };

  float4 p_nw = read_imagef(blur, sampler, uv + (int2)(-1, -1));
  float4 p_n  = read_imagef(blur, sampler, uv + (int2)( 0, -1));
  float4 p_ne = read_imagef(blur, sampler, uv + (int2)( 1, -1));
  float4 p_e  = read_imagef(blur, sampler, uv + (int2)( 1,  0));
  float4 p_se = read_imagef(blur, sampler, uv + (int2)( 1,  1));
  float4 p_s  = read_imagef(blur, sampler, uv + (int2)( 0,  1));
  float4 p_sw = read_imagef(blur, sampler, uv + (int2)(-1,  1));
  float4 p_w  = read_imagef(blur, sampler, uv + (int2)(-1,  0));

  vert = p_nw + 2 * p_n + p_ne - p_sw - 2 * p_s - p_se;
  horz = p_nw + 2 * p_w + p_sw - p_ne - 2 * p_e - p_se;

  pix.x = hypot(vert, horz).x;          // G = sqrt(Gx^2+Gy^2)
  pix.y = atanpi(vert / horz).x + 0.5;  // theta = atan(vert/horz)
  pix.z = vert.x;
  pix.w = horz.x;

  write_imagef(sobel, uv, pix);
}

/**
 * Non maximum supression
 */
__kernel void krnNMS(__read_only image2d_t sobel,
                     __write_only image2d_t nms)
{
  float left, right;
  int2 uv, dir;
  float4 center, pix, out;

  uv = (int2){ get_global_id(0), get_global_id(1) };
  pix = read_imagef(sobel, sampler, uv);
  
  if (pix.y < 0.125) 
  {
    dir = (int2)(0, 1); // 90 degrees
  } 
  else if (pix.y < 0.375) 
  {   
    dir = (int2)(-1, 1); // 135 degrees
  } 
  else if (pix.y < 0.625) 
  {
    dir = (int2)(1, 0); // 0 degrees
  } 
  else if (pix.y < 0.875) 
  {
    dir = (int2)(1, 1); // 45 degrees
  } 
  else 
  { 
    dir = (int2)(0, 1); // 90 degrees
  }
  
  center = read_imagef(sobel, sampler, uv);
  left   = read_imagef(sobel, sampler, uv + dir).x;
  right  = read_imagef(sobel, sampler, uv - dir).x;

  out = (float4)(0.0);
  if (center.x > left && center.x > right) 
  {
    out = center;
  }

  write_imagef(nms, uv, out);
}

/**
 * Hysteresis thresholding
 */
__kernel void krnHysteresis(__read_only image2d_t nms,
                            __write_only image2d_t out)
{
  int2 uv;
  int i;
  bool cont;
  float4 pix, neigh;

  uv = (int2){ get_global_id(0), get_global_id(1) };
  pix = read_imagef(nms, sampler, uv);

  if (pix.x >= THIGH) 
  {
    write_imagef(out, uv, (float4)(1));
    return;
  } 
  else if (pix.x >= TLOW) 
  {
    cont = false;
    for (i = 0; i < 8; ++i) 
    {
      neigh = read_imagef(nms, sampler, uv + OFF3X3[i]);
      if (neigh.x >= THIGH) 
      {
        write_imagef(out, uv, (float4)(1));
        return;
      }
      else if (neigh.x >= TLOW) 
      {
        cont = true;
      }
    }

    if (cont) 
    {
      for (i = 0; i < 16; ++i) 
      {
        neigh = read_imagef(nms, sampler, uv + OFF5X5[i]);
        if (neigh.x >= THIGH) 
        {
          write_imagef(out, uv, (float4)(1));
          return;
        }
      }
    }
  }

  write_imagef(out, uv, (float)(0.0));
}


/**
 * Final composition
 */
__kernel void krnFinal(__read_only image2d_t edges,
                       __read_only image2d_t input,
                       __write_only image2d_t out)
{
  float4 edge, pix;
  int2 uv;

  uv = (int2){ get_global_id(0), get_global_id(1) };
  edge = read_imagef(edges, sampler, uv);
  pix = read_imagef(input, sampler, uv);

  write_imagef(out, uv, edge.x + pix);
}
