// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "maya_render_save_interface.hpp"
#include "stdafx.h"

#include <boost/cstdint.hpp>

#include <maya/MImage.h>

#include <frantic/files/paths.hpp>
#include <frantic/logging/logging_level.hpp>
#include <frantic/maya/attributes.hpp>
#include <frantic/maya/maya_util.hpp>

using namespace frantic::files;
using namespace frantic::maya;

maya_render_save_interface::maya_render_save_interface( const MFnDependencyNode& krakatoaSettingsNode,
                                                        const MDGContext& currentContext,
                                                        const frantic::tstring& cameraName ) {

    m_krakatoaSettings.setObject( krakatoaSettingsNode.object() );
    m_currentContext = currentContext;
    m_cameraName = cameraName;
}

maya_render_save_interface::~maya_render_save_interface() {}

void maya_render_save_interface::save_render_data( int width, int height, int imageCount,
                                                   const krakatoasr::output_type_t* listOfTypes,
                                                   const krakatoasr::frame_buffer_pixel_data* const* listOfImages ) {
    frantic::tstring extension = get_output_image_extension();

    // special case handling for exr files, since we have a better system than maya
    if( extension == _T("exr") ) {
        krakatoasr::multi_channel_exr_file_saver exrSaver(
            frantic::strings::to_string( get_output_image_name() ).c_str() );

        // get exr compression type
        krakatoasr::exr_compression_t exr_compression_type = get_exr_compression_type();
        exrSaver.set_exr_compression_type( exr_compression_type );

        // get rgba bit depth setting
        krakatoasr::exr_bit_depth_t rgbaD = get_exr_bit_depth( "exrRgbaBitDepth" );
        exrSaver.set_channel_name_rgba( "R", "G", "B", "A", rgbaD );

        // get normal bit depth setting
        krakatoasr::exr_bit_depth_t normalD = get_exr_bit_depth( "exrNormalBitDepth" );
        exrSaver.set_channel_name_normal( "normal.X", "normal.Y", "normal.Z", normalD );

        // get velocity bit depth setting
        krakatoasr::exr_bit_depth_t velocityD = get_exr_bit_depth( "exrVelocityBitDepth" );
        exrSaver.set_channel_name_velocity( "velocity.X", "velocity.Y", "velocity.Z", velocityD );

        // get z bit depth setting
        krakatoasr::exr_bit_depth_t zD = get_exr_bit_depth( "exrZBitDepth" );
        exrSaver.set_channel_name_z( "Z", zD );

        // get occluded bit depth setting
        krakatoasr::exr_bit_depth_t occludedD = get_exr_bit_depth( "exrOccludedBitDepth" );
        exrSaver.set_channel_name_rgba_occluded( "occluded.R", "occluded.G", "occluded.B", "occluded.A", occludedD );

        // save the data
        exrSaver.save_render_data( width, height, imageCount, listOfTypes, listOfImages );
    } else {
        for( int i = 0; i < imageCount; ++i ) {
            switch( listOfTypes[i] ) {
            case krakatoasr::OUTPUT_RGBA:
                save_rgba_image_data( width, height, listOfImages[i] );
                break;
            case krakatoasr::OUTPUT_Z:
                save_depth_image_data( width, height, listOfImages[i] );
                break;
            case krakatoasr::OUTPUT_NORMAL:
                save_normal_image_data( width, height, listOfImages[i] );
                break;
            case krakatoasr::OUTPUT_RGBA_OCCLUDED:
                save_occluded_image_data( width, height, listOfImages[i] );
                break;
            case krakatoasr::OUTPUT_VELOCITY:
                save_velocity_image_data( width, height, listOfImages[i] );
                break;
            default:
                FF_LOG( debug ) << "Krakatoa MY: Unknown image output type " << listOfTypes[i] << ", omitting."
                                << std::endl;
            }
        }
    }
}

boost::uint8_t to_uint8( float f ) {
    return static_cast<boost::uint8_t>( frantic::math::clamp<float>( f * 255, 0, 255 ) );
}

void maya_render_save_interface::save_rgba_image_data( int width, int height,
                                                       const krakatoasr::frame_buffer_pixel_data* imageData ) {
    MImage mayaImage;
    mayaImage.create( width, height, 4, MImage::kByte );

    const size_t imageSize = width * height;

    for( size_t i = 0; i < imageSize; ++i ) {
        mayaImage.pixels()[i * 4 + 0] = to_uint8( imageData[i].r );
        mayaImage.pixels()[i * 4 + 1] = to_uint8( imageData[i].g );
        mayaImage.pixels()[i * 4 + 2] = to_uint8( imageData[i].b );
        mayaImage.pixels()[i * 4 + 3] =
            to_uint8( ( imageData[i].r_alpha + imageData[i].g_alpha + imageData[i].b_alpha ) / 3.0f );
    }

    mayaImage.writeToFile( MString( get_output_image_name().c_str() ),
                           MString( get_output_image_extension().c_str() ) );
}

void maya_render_save_interface::save_depth_image_data( int width, int height,
                                                        const krakatoasr::frame_buffer_pixel_data* imageData ) {
    MImage mayaImage;
    mayaImage.create( width, height, 4, MImage::kByte );

    const size_t imageSize = width * height;

    for( size_t i = 0; i < imageSize; ++i ) {
        mayaImage.pixels()[i * 4 + 0] = to_uint8( imageData[i].r );
        mayaImage.pixels()[i * 4 + 1] = to_uint8( imageData[i].r );
        mayaImage.pixels()[i * 4 + 2] = to_uint8( imageData[i].r );
        mayaImage.pixels()[i * 4 + 3] = to_uint8( imageData[i].r_alpha );
    }

    mayaImage.writeToFile( MString( get_output_image_name( _T( "ZDepth" ) ).c_str() ),
                           MString( get_output_image_extension().c_str() ) );
}

void maya_render_save_interface::save_normal_image_data( int width, int height,
                                                         const krakatoasr::frame_buffer_pixel_data* imageData ) {
    MImage mayaImage;
    mayaImage.create( width, height, 4, MImage::kByte );

    const size_t imageSize = width * height;

    for( size_t i = 0; i < imageSize; ++i ) {
        mayaImage.pixels()[i * 4 + 0] = to_uint8( imageData[i].r );
        mayaImage.pixels()[i * 4 + 1] = to_uint8( imageData[i].g );
        mayaImage.pixels()[i * 4 + 2] = to_uint8( imageData[i].b );
        mayaImage.pixels()[i * 4 + 3] =
            to_uint8( ( imageData[i].r_alpha + imageData[i].g_alpha + imageData[i].b_alpha ) / 3.0f );
    }

    mayaImage.writeToFile( MString( get_output_image_name( _T( "Normal" ) ).c_str() ),
                           MString( get_output_image_extension().c_str() ) );
}

void maya_render_save_interface::save_velocity_image_data( int width, int height,
                                                           const krakatoasr::frame_buffer_pixel_data* imageData ) {
    MImage mayaImage;
    mayaImage.create( width, height, 4, MImage::kByte );

    const size_t imageSize = width * height;

    for( size_t i = 0; i < imageSize; ++i ) {
        mayaImage.pixels()[i * 4 + 0] = to_uint8( imageData[i].r );
        mayaImage.pixels()[i * 4 + 1] = to_uint8( imageData[i].g );
        mayaImage.pixels()[i * 4 + 2] = to_uint8( imageData[i].b );
        mayaImage.pixels()[i * 4 + 3] =
            to_uint8( ( imageData[i].r_alpha + imageData[i].g_alpha + imageData[i].b_alpha ) / 3.0f );
    }

    mayaImage.writeToFile( MString( get_output_image_name( _T( "Velocity" ) ).c_str() ),
                           MString( get_output_image_extension().c_str() ) );
}

void maya_render_save_interface::save_occluded_image_data( int width, int height,
                                                           const krakatoasr::frame_buffer_pixel_data* imageData ) {
    MImage mayaImage;
    mayaImage.create( width, height, 4, MImage::kByte );

    const size_t imageSize = width * height;

    for( size_t i = 0; i < imageSize; ++i ) {
        mayaImage.pixels()[i * 4 + 0] = to_uint8( imageData[i].r );
        mayaImage.pixels()[i * 4 + 1] = to_uint8( imageData[i].g );
        mayaImage.pixels()[i * 4 + 2] = to_uint8( imageData[i].b );
        mayaImage.pixels()[i * 4 + 3] =
            to_uint8( ( imageData[i].r_alpha + imageData[i].g_alpha + imageData[i].b_alpha ) / 3.0f );
    }

    mayaImage.writeToFile( MString( get_output_image_name( _T( "Occluded" ) ).c_str() ),
                           MString( get_output_image_extension().c_str() ) );
}

void maya_render_save_interface::set_current_context( const MDGContext& currentContext ) {
    m_currentContext = currentContext;
}

void maya_render_save_interface::set_camera_name( const frantic::tstring& cameraName ) { m_cameraName = cameraName; }

frantic::tstring maya_render_save_interface::get_output_image_name( const frantic::tstring& appendName ) {
    frantic::tstring result =
        maya_util::get_render_filename( m_currentContext, m_cameraName, appendName, get_output_image_extension() );

    // if (get_output_image_extension() == _T("exr"))
    //{
    //	result = replace_extension(result, _T(".exr"));
    // }

    return result;
}

frantic::tstring maya_render_save_interface::get_output_image_extension() {
    frantic::tstring output;
    int renderImageFormat = maya_util::get_current_render_image_format();
    if( frantic::maya::get_boolean_attribute( m_krakatoaSettings, _T( "forceEXROutput" ) ) ) {
        return _T("exr");
    } else if( maya_util::get_image_format_extension( renderImageFormat, output ) ) {
        return output;
    } else {
        FF_LOG( stats ) << "Image format " << renderImageFormat << " not recognized, using default (iff)" << std::endl;
        return _T("iff");
    }
}

krakatoasr::exr_compression_t maya_render_save_interface::get_exr_compression_type() const {
    krakatoasr::exr_compression_t retval = krakatoasr::COMPRESSION_NONE;
    MString exrCompressionType = frantic::maya::get_enum_attribute( m_krakatoaSettings, "exrCompressionType" );

    if( exrCompressionType == "No Compression" )
        retval = krakatoasr::COMPRESSION_NONE;
    else if( exrCompressionType == "Run Length Encoding" )
        retval = krakatoasr::COMPRESSION_RLE;
    else if( exrCompressionType == "Zlib Compression (one scan line at a time)" )
        retval = krakatoasr::COMPRESSION_ZIPS;
    else if( exrCompressionType == "Zlib Compression (in blocks of 16 scan lines)" )
        retval = krakatoasr::COMPRESSION_ZIP;
    else if( exrCompressionType == "Piz-Based Wavelet Compression" )
        retval = krakatoasr::COMPRESSION_PIZ;
    else if( exrCompressionType == "Lossy 24-bit Float Compression" )
        retval = krakatoasr::COMPRESSION_PXR24;
    else if( exrCompressionType == "Lossy 4-by-4 Pixel Block Compression (fixed compression rate)" )
        retval = krakatoasr::COMPRESSION_B44;
    else if( exrCompressionType == "Lossy 4-by-4 Pixel Block Compression (flat fields are compressed more)" )
        retval = krakatoasr::COMPRESSION_B44A;
    else {
        // shouldn't happen given our code
        throw std::runtime_error( "Unknown EXRDataCompression Type " );
    }
    FF_LOG( debug ) << "EXRCompression Type: " << frantic::strings::to_tstring( exrCompressionType.asChar() )
                    << std::endl;
    return retval;
}

krakatoasr::exr_bit_depth_t maya_render_save_interface::get_exr_bit_depth( const std::string& bitDepthName ) const {
    krakatoasr::exr_bit_depth_t retval = krakatoasr::BIT_DEPTH_FLOAT;
    MString exrBitDepth = frantic::maya::get_enum_attribute( m_krakatoaSettings, MString( bitDepthName.c_str() ) );

    if( exrBitDepth == "UInt" )
        retval = krakatoasr::BIT_DEPTH_UINT;
    else if( exrBitDepth == "Half" )
        retval = krakatoasr::BIT_DEPTH_HALF;
    else if( exrBitDepth == "Float" )
        retval = krakatoasr::BIT_DEPTH_FLOAT;
    else {
        // shouldn't happen given our code
        throw std::runtime_error( "Unknown bit depth for " + bitDepthName );
    }
    FF_LOG( debug ) << "EXR Bit Depth for " << frantic::strings::to_tstring( bitDepthName ) << ": "
                    << frantic::strings::to_tstring( exrBitDepth.asChar() ) << std::endl;
    return retval;
}
