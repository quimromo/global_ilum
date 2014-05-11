#pragma once

#include <optixu/optixpp_namespace.h>
//#include <sutil.h>
#include <string>
#include <iosfwd>


//-----------------------------------------------------------------------------
//
// Utility functions
//
//-----------------------------------------------------------------------------

// Creates a TextureSampler object for the given PPM file.  If filename is 
// empty or PPMLoader fails, a 1x1 texture is created with the provided default
// texture color.
optix::TextureSampler loadAnyTexture( optix::Context context,
                                             const std::string& ppm_filename,
                                             const optix::float3& default_color );


