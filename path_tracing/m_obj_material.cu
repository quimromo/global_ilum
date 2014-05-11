
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

#include "types.h"
#include "commonStructs.h"
#include "random.h"
#include <optix.h>
#include <optixu/optixu_math_namespace.h>


using namespace optix;

rtTextureSampler<float4, 2>   ambient_map;         // 
rtTextureSampler<float4, 2>   diffuse_map;         // Correspond to OBJ mtl params
rtTextureSampler<float4, 2>   specular_map;        //
rtTextureSampler<float4, 2> bump_map;
rtDeclareVariable(uint, usebump, , );
rtDeclareVariable(float,      phong_exp, , );          //
rtDeclareVariable(int,        illum, , );              //

rtDeclareVariable(float3, texcoord, attribute texcoord, ); 
rtDeclareVariable(float3, geometric_normal, attribute geometric_normal, ); 
rtDeclareVariable(float3, shading_normal, attribute shading_normal, );

rtDeclareVariable(float3,    tangent, attribute    tangent, );
rtDeclareVariable(float3, bi_tangent, attribute bi_tangent, );

rtDeclareVariable(PerRayData_radiance, prd_radiance, rtPayload, );
rtDeclareVariable(PerRayData_shadow,   prd_shadow,   rtPayload, );

rtDeclareVariable(optix::Ray, ray,          rtCurrentRay, );
rtDeclareVariable(float,      t_hit,        rtIntersectionDistance, );

rtDeclareVariable(unsigned int, radiance_ray_type, , );
rtDeclareVariable(unsigned int, shadow_ray_type , , );
rtDeclareVariable(float,        scene_epsilon, , );
rtDeclareVariable(rtObject,     top_object, , );

rtDeclareVariable(int, max_depth, , );


RT_PROGRAM void any_hit_shadow()
{
  //phongShadowed();
}

RT_PROGRAM void diffuse()
{
  float3 direction              = ray.direction;
  float3 world_shading_normal   = normalize( rtTransformNormal( RT_OBJECT_TO_WORLD, shading_normal ) );
  float3 world_geometric_normal = normalize( rtTransformNormal( RT_OBJECT_TO_WORLD, geometric_normal ) );
  float3 uv                     = texcoord;
  //rtPrintf("objmaterial");
  
  if(usebump){
	  float z = 1.0;
	  float x = make_float3(tex2D(bump_map, uv.x + 0.001, uv.y)).x - make_float3(tex2D(bump_map, uv.x - 0.001, uv.y)).x;
	  float y = make_float3(tex2D(bump_map, uv.x, uv.y + 0.001)).x - make_float3(tex2D(bump_map, uv.x, uv.y - 0.001)).x;
	  float3 bump_normal = normalize(make_float3(x,y,z));
	  //float3 obj_norm = bump_normal.x * tangent + bump_normal.y * bi_tangent + bump_normal.z * shading_normal;
	  float3 obj_norm = shading_normal + tangent * x + bi_tangent * y;
	  world_shading_normal   = normalize( rtTransformNormal( RT_OBJECT_TO_WORLD, obj_norm ) );
	  prd_radiance.result = world_shading_normal;

  }
  
  else{
  
  float3 ffnormal               = normalize(faceforward( world_shading_normal, -direction, world_geometric_normal ));
  

  float3 black = make_float3(0.0f, 0.0f, 0.0f);

  // grab values from textures
  // support only MTL illumination modes 0-3 (Ks is for now used as reflectivity)
  // ( see http://local.wasp.uwa.edu.au/~pbourke/dataformats/mtl/ )
  float3 Kd =                       make_float3( tex2D( diffuse_map,  uv.x, uv.y ) );
  //float3 Ka = (illum < 1) ? black : make_float3( tex2D( ambient_map,  uv.x, uv.y ) );
  //float3 Ks = (illum < 2) ? black : make_float3( tex2D( specular_map, uv.x, uv.y ) );
  //float3 Kr = (illum < 3) ? black : Ks;
  
  	// diffuse reflection
	float3 color = make_float3(0.0);
	float3 brdfLight = make_float3(0.0);
	float3 hit = ray.origin + ray.direction * t_hit;
	
	if(prd_radiance.depth < max_depth){
		unsigned int seed = prd_radiance.seed;
		float r1 = rnd(seed);
		float r2 = rnd(seed);
		float3 p;
		cosine_sample_hemisphere(r1, r2, p);
		//p = sampleHemisphere(r1, r2);
		
		float3 v1, v2;
		createONB(ffnormal, v1, v2);
		float3 rd = normalize(v1 * p.x + v2 * p.y + ffnormal * p.z);
		
		//float3 rd = sampleHemisphere(norm, r1, r2);
		PerRayData_radiance diffuse_refl_prd;
		diffuse_refl_prd.seed = seed;
		diffuse_refl_prd.depth = prd_radiance.depth + 1;
		optix::Ray diffuse_refl_ray( hit, rd, radiance_ray_type, scene_epsilon );
		rtTrace(top_object, diffuse_refl_ray, diffuse_refl_prd);
		brdfLight = diffuse_refl_prd.result;// * dot(norm, rd);
		color = brdfLight * Kd * dot(ffnormal, rd);// / (diffuse_refl_prd.dist*diffuse_refl_prd.dist);
	}
	prd_radiance.result = color;
	prd_radiance.dist = t_hit;
  }
	
  //prd_radiance.result = Kd;
}

rtDeclareVariable(float3, emission, , );

RT_PROGRAM void emitter(){
	
	float3 world_shading_normal   = normalize( rtTransformNormal( RT_OBJECT_TO_WORLD, shading_normal ) );
	float3 world_geometric_normal = normalize( rtTransformNormal( RT_OBJECT_TO_WORLD, geometric_normal ) );
	float3 ffnormal               = normalize(faceforward( world_shading_normal, -ray.direction, world_geometric_normal ));
	// la normal esta al revés!
	float LnDL = dot(-ray.direction, ffnormal);
	//if( LnDL < 0){

		prd_radiance.result = emission * LnDL;
		prd_radiance.dist = t_hit;
	//}
	//else prd_radiance.result = make_float3(0.0f);
	
	//prd_radiance.result = Kd;
}
