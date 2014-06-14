
/*
 * Copyright (c) 2008 - 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and proprietary
 * rights in and to this software, related documentation and any modifications thereto.
 * Any use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation is strictly
 * prohibited.
 *
 * TO THE MAXIMUM EXTENT PERMITTED BY APPLICABLE LAW, THIS SOFTWARE IS PROVIDED *AS IS*
 * AND NVIDIA AND ITS SUPPLIERS DISCLAIM ALL WARRANTIES, EITHER EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE.  IN NO EVENT SHALL NVIDIA OR ITS SUPPLIERS BE LIABLE FOR ANY
 * SPECIAL, INCIDENTAL, INDIRECT, OR CONSEQUENTIAL DAMAGES WHATSOEVER (INCLUDING, WITHOUT
 * LIMITATION, DAMAGES FOR LOSS OF BUSINESS PROFITS, BUSINESS INTERRUPTION, LOSS OF
 * BUSINESS INFORMATION, OR ANY OTHER PECUNIARY LOSS) ARISING OUT OF THE USE OF OR
 * INABILITY TO USE THIS SOFTWARE, EVEN IF NVIDIA HAS BEEN ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES
 */

#include <optix_world.h>
#include "helpers.h"
#include "random.h"
#include "device_funcs.h"

using namespace optix;

/*
struct PerRayData_radiance
{
  float3 result;
  float  importance;
  int    depth;
};
*/
rtDeclareVariable(uint,			currentSample, , );
rtDeclareVariable(uint,			sqrtspp, , );
rtDeclareVariable(uint,			offset_x, , );
rtDeclareVariable(uint,			offset_y, , );
rtDeclareVariable(float,			aspect_ratio, , );
rtDeclareVariable(uint2,			screen_dim, , );
rtDeclareVariable(float3,        eye, , );
rtDeclareVariable(float3,        U, , );
rtDeclareVariable(float3,        V, , );
rtDeclareVariable(float3,        W, , );
rtDeclareVariable(float,		 viewd, , );
rtDeclareVariable(float3,        bad_color, , );
rtDeclareVariable(float,         scene_epsilon, , );
rtBuffer<float3, 1>              output_buffer;
rtBuffer<float3, 1>              cumulated_buffer;
rtDeclareVariable(rtObject,      top_object, , );
rtDeclareVariable(unsigned int,  radiance_ray_type, , );

rtDeclareVariable(uint2, launch_index, rtLaunchIndex, );
rtDeclareVariable(uint2, launch_dim,   rtLaunchDim, );
rtDeclareVariable(float, time_view_scale, , ) = 1e-6f;
rtDeclareVariable(uint2, output_buffer_size, , );

//#define TIME_VIEW


RT_PROGRAM void m_pinhole_camera()
{
#ifdef TIME_VIEW
  clock_t t0 = clock(); 
#endif
  float2 offset = make_float2(offset_x, offset_y);
  float2 startSample = make_float2(launch_index) + offset 
	  + make_float2(
		(float)(currentSample%sqrtspp) * (1.0f/(float)sqrtspp),
		(float)(currentSample/sqrtspp) * (1.0f/(float)sqrtspp)
		);

  float2 centerSample = make_float2(1.0f/float(2 * sqrtspp));
  float2 d = ((startSample + centerSample) / make_float2(screen_dim.x, screen_dim.y)) * 2.f - 1.f;
  //float aspect_ratio = (float)launch_dim.x / launch_dim.y;
  d.x *= aspect_ratio;
  float3 ray_origin = eye;
  float3 ray_direction = normalize(d.x*U + d.y*V + W*viewd);
  
  optix::Ray ray = optix::make_Ray(ray_origin, ray_direction, radiance_ray_type, scene_epsilon, RT_DEFAULT_MAX);

  PerRayData_radiance prd;
  prd.contribution = make_float3(1.0f);
  prd.importance = 1.f;
  prd.depth = 0;
  prd.seed = tea<32>((launch_index.x + offset_x) * (launch_index.y + offset_y) + (int)clock(), currentSample + (int) clock());  
  rtTrace(top_object, ray, prd);

#ifdef TIME_VIEW
  clock_t t1 = clock(); 
 
  float expected_fps   = 1.0f;
  float pixel_time     = ( t1 - t0 ) * time_view_scale * expected_fps;
  output_buffer[launch_index] = make_color( make_float3(  pixel_time ) ); 
#else
  //output_buffer[launch_index] = make_color( prd.result );
  
  /*
  output_buffer[launch_index.y* output_buffer_size.x + launch_index.x] = make_float3 (	prd.result.z,
												prd.result.y,
												prd.result.x
											);
*/
  unsigned int buff_idx = (launch_index.y + offset_y)* screen_dim.x + launch_index.x + offset_x;
  if(currentSample == 0){
	  cumulated_buffer[buff_idx] = make_float3(0.0f);
  }
  cumulated_buffer[buff_idx] += make_float3( prd.result.z, prd.result.y, prd.result.x );
	
  float3 render_color = cumulated_buffer[buff_idx] / (currentSample + 1u);
  output_buffer[buff_idx] = make_float3 (	__saturatef( powf( render_color.z, 1.0f/2.0f) ),
											__saturatef( powf( render_color.y, 1.0f/2.0f) ),
											__saturatef( powf( render_color.x, 1.0f/2.0f) )
										);
  /*
  output_buffer[launch_index] = make_float3 (	__saturatef(prd.result.z),
												__saturatef(prd.result.y ),
												__saturatef(prd.result.x )
											);
	*/										
  //output_buffer[launch_index] = prd.result;
#endif
}

RT_PROGRAM void exception()
{
  const unsigned int code = rtGetExceptionCode();
  rtPrintf( "Caught exception 0x%X at launch index (%d,%d)\n", code, launch_index.x, launch_index.y );
  rtPrintExceptionDetails();
  output_buffer[launch_index.y* launch_dim.x + launch_index.x] = bad_color;
}
