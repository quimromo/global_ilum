
#include "DevilLoader.h"
#include <IL/il.h>
#include <optixu/optixu_math_namespace.h>
#include <iostream>

//-----------------------------------------------------------------------------
//  
//  Utility functions 
//
//-----------------------------------------------------------------------------

optix::TextureSampler loadAnyTexture( optix::Context context,
                                      const std::string& filename,
                                      const optix::float3& default_color )
{

	// PPMLoader ppm( filename );
	// return ppm.loadTexture(context, default_color );
	ILuint width, height, bpp;
	ILubyte* data;
	RTformat fmt;
	bool error = false;
	ilInit();
	ILuint ImageName;
	ilGenImages(1, &ImageName);
	ilBindImage(ImageName);
	if(ilLoadImage(filename.c_str())){
		
		width = ilGetInteger(IL_IMAGE_WIDTH);
		height = ilGetInteger(IL_IMAGE_HEIGHT);
		bpp = ilGetInteger(IL_IMAGE_BPP);
		
		data = ilGetData();
		
		std::cout << "BPP: " << bpp << std::endl;
		switch(bpp){
			case 3:
				fmt = RT_FORMAT_UNSIGNED_BYTE3;
				break;
			case 4:
				fmt = RT_FORMAT_UNSIGNED_BYTE4;
				break;
			default:
				error = true;
		}
	}
	else error = true;

	optix::TextureSampler sampler = context->createTextureSampler();
	sampler->setWrapMode( 0, RT_WRAP_REPEAT );
	sampler->setWrapMode( 1, RT_WRAP_REPEAT );
	sampler->setWrapMode( 2, RT_WRAP_REPEAT );
	sampler->setIndexingMode( RT_TEXTURE_INDEX_NORMALIZED_COORDINATES );
	sampler->setReadMode( RT_TEXTURE_READ_NORMALIZED_FLOAT );
	sampler->setMaxAnisotropy( 1.0f );
	sampler->setMipLevelCount( 1u );
	sampler->setArraySize( 1u );

	if(error){
		// Create buffer with single texel set to default_color
		optix::Buffer buffer = context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_UNSIGNED_BYTE4, 1u, 1u );
		unsigned char* buffer_data = static_cast<unsigned char*>( buffer->map() );
		buffer_data[0] = (unsigned char)optix::clamp((int)(default_color.x * 255.0f), 0, 255);
		buffer_data[1] = (unsigned char)optix::clamp((int)(default_color.y * 255.0f), 0, 255);
		buffer_data[2] = (unsigned char)optix::clamp((int)(default_color.z * 255.0f), 0, 255);
		buffer_data[3] = 255;
		buffer->unmap();

		sampler->setBuffer( 0u, 0u, buffer );
		// Although it would be possible to use nearest filtering here, we chose linear
		// to be consistent with the textures that have been loaded from a file. This
		// allows OptiX to perform some optimizations.
		sampler->setFilteringModes( RT_FILTER_LINEAR, RT_FILTER_LINEAR, RT_FILTER_NONE );

		return sampler;
	}

	optix::Buffer buffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_UNSIGNED_BYTE4, width, height);
	unsigned char* buffer_data = static_cast<unsigned char*>(buffer->map());
	/*
	for( unsigned i = 0; i < width; ++i){
		for( unsigned j = 0; j < height; ++j){
			unsigned index = ((fmt==RT_FORMAT_UNSIGNED_BYTE4)?4:3) * (i*height + j); 
			buffer_data[index] = data[index];
			buffer_data[index +  1] = data[index + 1];
			buffer_data[index +  2] = data[index + 2];
			
			if(fmt == RT_FORMAT_UNSIGNED_BYTE4) buffer_data[index +  3] = data[index + 3];
			else buffer_data[index +  3] = 255;
		}
	}
	*/
	if(ilCopyPixels( 0, 0, 0, width, height, 1, IL_RGBA, IL_UNSIGNED_BYTE, buffer_data) == IL_FALSE){
		std::cout << "error copying texture pixels!" << std::endl << "exit" << std::endl;
		exit(1);
	}; 

	buffer->unmap();

	sampler->setBuffer( 0u, 0u, buffer );
	sampler->setFilteringModes( RT_FILTER_LINEAR, RT_FILTER_LINEAR, RT_FILTER_NONE );

	return sampler;
}
