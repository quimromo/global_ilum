

#define PI		3.14159265f;
#include <optix.h>
#include <optix_math.h>


struct PerRayData_radiance
{
  float3 result;
  float  importance;
  int depth;
  unsigned int seed;
  unsigned int dist;
};

struct PerRayData_shadow
{
  float3 attenuation;
};

/*
 * Sample a uniform hemisphere
 * radius, revs must be in the range [0,1]
 * norm must be normalized
 * returns a normalized vector in the hemisphere
 */
static __device__ __inline__ float3 sampleHemisphere( const float3& norm, float radius, float revs)
{
	float3 axis;
	if(norm.x < norm.y && norm.y < norm.z) axis = make_float3(1.0f, 0.0f, 0.0f);
	else if (norm.y < norm.z) axis = make_float3(0.0f, 1.0f, 0.0f);
	else axis = make_float3(0.0f, 0.0f, 1.0f);

	float3 x = normalize(cross(norm, axis));
	float3 z = normalize(cross(norm, x));

	float j = sqrtf(1.0 - radius*radius);
	float alpha = 2.0 * revs * PI; 

	float i = radius*cos(alpha);
	float k = radius*sin(alpha);

	return i*x + j*norm + k*z;
}

// Create ONB from normalalized vector
static
__device__ __inline__ void createONB( const optix::float3& n,
                                      optix::float3& U,
                                      optix::float3& V)
{
  using namespace optix;

  U = cross( n, make_float3( 0.0f, 1.0f, 0.0f ) );
  if ( dot(U, U) < 1.e-3f )
    U = cross( n, make_float3( 1.0f, 0.0f, 0.0f ) );
  U = normalize( U );
  V = cross( n, U );
}

static __device__ __inline__ float3 sampleHemisphere(float radius, float revs)
{
	float alpha = 2.0 * revs *PI;
	float x = radius * cos(alpha);
	float y = radius * sin(alpha);
	float z = sqrtf(1.0 - x*x);
	return make_float3(x, y, z);
}

static __device__ __inline__ float3 sample_specular(float phong_exp, float rnd1, float rnd2){
	
	float cosPhi = powf(rnd1, 1.0f/(phong_exp + 1.0f));
	float theta = 2.0 * rnd2 * PI;
	float x = cos(theta)*cosPhi;
	float y = sin(theta)*cosPhi;
	float z = sqrtf(1 - cosPhi*cosPhi);

	return make_float3(x,y,z);


}