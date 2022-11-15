// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include "KrakatoaRenderSettingsNode.hpp"

#include <frantic/maya/maya_util.hpp>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnDoubleArrayData.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnStringData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnUnitAttribute.h>
#include <maya/MFnVectorArrayData.h>

using namespace frantic::maya;

// IMPORTANT NOTE:
// If you add a new attribute here, it will break loading of old Maya scene files without that attribute.
// To fix this, you must also add that same attribute to this function in KrakatoaRenderer.mel:
// KMY_convertOldKrakatoaSettingsNode

#define NUMCHANNELCOPIERS 3

MTypeId KrakatoaRenderSettingsNode::typeId( 0x00117486 );
MString KrakatoaRenderSettingsNode::typeName = "KrakatoaRenderSettingsNode";

MObject KrakatoaRenderSettingsNode::inForceEXROutput;
MObject KrakatoaRenderSettingsNode::inEXRCompressionType;
MObject KrakatoaRenderSettingsNode::inEXRRgbaBitDepth;
MObject KrakatoaRenderSettingsNode::inEXRNormalBitDepth;
MObject KrakatoaRenderSettingsNode::inEXRVelocityBitDepth;
MObject KrakatoaRenderSettingsNode::inEXRZBitDepth;
MObject KrakatoaRenderSettingsNode::inEXROccludedBitDepth;

MObject KrakatoaRenderSettingsNode::inExportMayaParticles;
MObject KrakatoaRenderSettingsNode::inExportMayaHair;
MObject KrakatoaRenderSettingsNode::inExportPRTLoaders;
MObject KrakatoaRenderSettingsNode::inExportPRTVolumes;
MObject KrakatoaRenderSettingsNode::inExportPRTSurfaces;
MObject KrakatoaRenderSettingsNode::inExportPRTFractals;

MObject KrakatoaRenderSettingsNode::inOverrideBG;
MObject KrakatoaRenderSettingsNode::inBackgroundColor;
MObject KrakatoaRenderSettingsNode::inOverrideColor;
MObject KrakatoaRenderSettingsNode::inColorChannelOverride;
MObject KrakatoaRenderSettingsNode::inOverrideEmission;
MObject KrakatoaRenderSettingsNode::inEmissionChannelOverride;
MObject KrakatoaRenderSettingsNode::inOverrideAbsorption;
MObject KrakatoaRenderSettingsNode::inAbsorptionChannelOverride;
MObject inOverrideDensity;
MObject inDensityChannelOverride;

MObject KrakatoaRenderSettingsNode::inCopyChannel[NUMCHANNELCOPIERS];
MObject KrakatoaRenderSettingsNode::inCopyChannelFrom[NUMCHANNELCOPIERS];
MObject KrakatoaRenderSettingsNode::inCopyChannelTo[NUMCHANNELCOPIERS];

MObject KrakatoaRenderSettingsNode::inSaveNormalPass;
MObject KrakatoaRenderSettingsNode::inSaveVelocityPass;
MObject KrakatoaRenderSettingsNode::inSaveZDepthPass;
MObject KrakatoaRenderSettingsNode::inSaveOccludedPass;

MObject KrakatoaRenderSettingsNode::inPhongSpecularPower;
MObject KrakatoaRenderSettingsNode::inPhongSpecularLevel;
MObject KrakatoaRenderSettingsNode::inUsePhongSpecularPower;
MObject KrakatoaRenderSettingsNode::inUsePhongSpecularLevel;

MObject KrakatoaRenderSettingsNode::inHgEccentricity;
MObject KrakatoaRenderSettingsNode::inUseHgEccentricity;

MObject KrakatoaRenderSettingsNode::inSchlickEccentricity;
MObject KrakatoaRenderSettingsNode::inUseSchlickEccentricity;

MObject KrakatoaRenderSettingsNode::inKkSpecularPower;
MObject KrakatoaRenderSettingsNode::inKkSpecularLevel;
MObject KrakatoaRenderSettingsNode::inUseKkSpecularPower;
MObject KrakatoaRenderSettingsNode::inUseKkSpecularLevel;

MObject KrakatoaRenderSettingsNode::inMarschnerSpecularLevel;
MObject KrakatoaRenderSettingsNode::inMarschnerSpecular2Level;
MObject KrakatoaRenderSettingsNode::inMarschnerSpecularGlossiness;
MObject KrakatoaRenderSettingsNode::inMarschnerSpecular2Glossiness;
MObject KrakatoaRenderSettingsNode::inMarschnerSpecularShift;
MObject KrakatoaRenderSettingsNode::inMarschnerSpecular2Shift;
MObject KrakatoaRenderSettingsNode::inMarschnerGlintLevel;
MObject KrakatoaRenderSettingsNode::inMarschnerGlintSize;
MObject KrakatoaRenderSettingsNode::inMarschnerGlintGlossiness;
MObject KrakatoaRenderSettingsNode::inMarschnerDiffuseLevel;
MObject KrakatoaRenderSettingsNode::inUseMarschnerSpecularLevel;
MObject KrakatoaRenderSettingsNode::inUseMarschnerSpecular2Level;
MObject KrakatoaRenderSettingsNode::inUseMarschnerSpecularGlossiness;
MObject KrakatoaRenderSettingsNode::inUseMarschnerSpecular2Glossiness;
MObject KrakatoaRenderSettingsNode::inUseMarschnerSpecularShift;
MObject KrakatoaRenderSettingsNode::inUseMarschnerSpecular2Shift;
MObject KrakatoaRenderSettingsNode::inUseMarschnerGlintLevel;
MObject KrakatoaRenderSettingsNode::inUseMarschnerGlintSize;
MObject KrakatoaRenderSettingsNode::inUseMarschnerGlintGlossiness;
MObject KrakatoaRenderSettingsNode::inUseMarschnerDiffuseLevel;

MObject KrakatoaRenderSettingsNode::inShadingMode;
MObject KrakatoaRenderSettingsNode::inRenderingMethod;
MObject KrakatoaRenderSettingsNode::inIgnoreSceneLights;
MObject KrakatoaRenderSettingsNode::inVoxelSize;
MObject KrakatoaRenderSettingsNode::inVoxelFilterRadius;
MObject KrakatoaRenderSettingsNode::inUseEmission;
MObject KrakatoaRenderSettingsNode::inUseAbsorption;
MObject KrakatoaRenderSettingsNode::inForceAdditiveMode;
MObject KrakatoaRenderSettingsNode::inLoadPercentage;

MObject KrakatoaRenderSettingsNode::inSelfShadowFilter;
MObject KrakatoaRenderSettingsNode::inDrawPointFilter;
MObject KrakatoaRenderSettingsNode::inLightingPassFilterSize;
MObject KrakatoaRenderSettingsNode::inFinalPassFilterSize;
MObject KrakatoaRenderSettingsNode::inLightingPassDensity;
MObject KrakatoaRenderSettingsNode::inFinalPassDensity;
MObject KrakatoaRenderSettingsNode::inLightingPassDensityExponent;
MObject KrakatoaRenderSettingsNode::inFinalPassDensityExponent;
MObject KrakatoaRenderSettingsNode::inUseLightingPassDensity;
MObject KrakatoaRenderSettingsNode::inEmissionStrength;
MObject KrakatoaRenderSettingsNode::inUseEmissionStrength;
MObject KrakatoaRenderSettingsNode::inEmissionStrengthExponent;

MObject KrakatoaRenderSettingsNode::inEnableMotionBlur;
MObject KrakatoaRenderSettingsNode::inMotionBlurParticleSegments;
MObject KrakatoaRenderSettingsNode::inJitteredMotionBlur;
MObject KrakatoaRenderSettingsNode::inShutterAngle;
MObject KrakatoaRenderSettingsNode::inEnableDOF;
MObject KrakatoaRenderSettingsNode::inSampleRateDOF;
MObject KrakatoaRenderSettingsNode::inDisableCameraBlur;
MObject KrakatoaRenderSettingsNode::inMotionBlurBias;

MObject KrakatoaRenderSettingsNode::inEnableAdaptiveMotionBlur;
MObject KrakatoaRenderSettingsNode::inAdaptiveMotionBlurMinSamples;
MObject KrakatoaRenderSettingsNode::inAdaptiveMotionBlurMaxSamples;
MObject KrakatoaRenderSettingsNode::inAdaptiveMotionBlurSmoothness;
MObject KrakatoaRenderSettingsNode::inAdaptiveMotionBlurExpoenent;

MObject KrakatoaRenderSettingsNode::inEnableBokehShapeMap;
MObject KrakatoaRenderSettingsNode::inBokehShapeMap;
MObject KrakatoaRenderSettingsNode::inEnableBokehBlendMap;
MObject KrakatoaRenderSettingsNode::inBokehBlendMap;
MObject KrakatoaRenderSettingsNode::inBokehBlendInfluence;
MObject KrakatoaRenderSettingsNode::inBokehEnableAnamorphicSqueeze;
MObject KrakatoaRenderSettingsNode::inBokehAnamorphicSqueeze;
MObject KrakatoaRenderSettingsNode::inBokehBlendMipmapScale;
MObject KrakatoaRenderSettingsNode::inAllocateBokehBlendInfluence;

MObject KrakatoaRenderSettingsNode::inEnableMatteObjects;
MObject KrakatoaRenderSettingsNode::inMatteSuperSampling;

MObject KrakatoaRenderSettingsNode::inLogLevel;
MObject KrakatoaRenderSettingsNode::inThreadCount;
MObject KrakatoaRenderSettingsNode::inFrameBufferAvailableMemoryFraction;

KrakatoaRenderSettingsNode::KrakatoaRenderSettingsNode( void ) {}

KrakatoaRenderSettingsNode::~KrakatoaRenderSettingsNode( void ) {}

void* KrakatoaRenderSettingsNode::creator() { return new KrakatoaRenderSettingsNode; }
MStatus KrakatoaRenderSettingsNode::initialize() {

    MFnTypedAttribute fnTypedAttribute;
    MFnUnitAttribute fnUnitAttribute;
    MFnNumericAttribute fnNumericAttribute;
    MFnEnumAttribute fnEnumAttribute;
    MStatus status;
    MFnStringData fnStringData;
    MObject emptyStringObject = fnStringData.create( "" );

    inForceEXROutput = fnNumericAttribute.create( "forceEXROutput", "forceEXROutput", MFnNumericData::kBoolean, 1 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inForceEXROutput );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inEXRCompressionType = fnEnumAttribute.create( "exrCompressionType", "exrCompressionType", 2 );
    fnEnumAttribute.addField( "No Compression", 0 );
    fnEnumAttribute.addField( "Run Length Encoding", 1 );
    fnEnumAttribute.addField( "Zlib Compression (one scan line at a time)", 2 );
    fnEnumAttribute.addField( "Zlib Compression (in blocks of 16 scan lines)", 3 );
    fnEnumAttribute.addField( "Piz-Based Wavelet Compression", 4 );
    fnEnumAttribute.addField( "Lossy 24-bit Float Compression", 5 );
    fnEnumAttribute.addField( "Lossy 4-by-4 Pixel Block Compression (fixed compression rate)", 6 );
    fnEnumAttribute.addField( "Lossy 4-by-4 Pixel Block Compression (flat fields are compressed more)", 7 );
    status = addAttribute( inEXRCompressionType );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inEXRRgbaBitDepth = fnEnumAttribute.create( "exrRgbaBitDepth", "exrRgbaBitDepth", 1 );
    fnEnumAttribute.addField( "UInt", 0 );
    fnEnumAttribute.addField( "Half", 1 );
    fnEnumAttribute.addField( "Float", 2 );
    status = addAttribute( inEXRRgbaBitDepth );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inEXRNormalBitDepth = fnEnumAttribute.create( "exrNormalBitDepth", "exrNormalBitDepth", 1 );
    fnEnumAttribute.addField( "UInt", 0 );
    fnEnumAttribute.addField( "Half", 1 );
    fnEnumAttribute.addField( "Float", 2 );
    status = addAttribute( inEXRNormalBitDepth );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inEXRVelocityBitDepth = fnEnumAttribute.create( "exrVelocityBitDepth", "exrVelocityBitDepth", 1 );
    fnEnumAttribute.addField( "UInt", 0 );
    fnEnumAttribute.addField( "Half", 1 );
    fnEnumAttribute.addField( "Float", 2 );
    status = addAttribute( inEXRVelocityBitDepth );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inEXRZBitDepth = fnEnumAttribute.create( "exrZBitDepth", "exrZBitDepth", 2 );
    fnEnumAttribute.addField( "Half", 1 );
    fnEnumAttribute.addField( "Float", 2 );
    status = addAttribute( inEXRZBitDepth );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inEXROccludedBitDepth = fnEnumAttribute.create( "exrOccludedBitDepth", "exrOccludedBitDepth", 1 );
    fnEnumAttribute.addField( "UInt", 0 );
    fnEnumAttribute.addField( "Half", 1 );
    fnEnumAttribute.addField( "Float", 2 );
    status = addAttribute( inEXROccludedBitDepth );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inExportMayaParticles =
        fnNumericAttribute.create( "exportMayaParticles", "exportMayaParticles", MFnNumericData::kBoolean, 1 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inExportMayaParticles );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inExportMayaHair = fnNumericAttribute.create( "exportMayaHair", "exportMayaHair", MFnNumericData::kBoolean, 0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inExportMayaHair );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inExportPRTLoaders =
        fnNumericAttribute.create( "exportPRTLoaders", "exportPRTLoaders", MFnNumericData::kBoolean, 1 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inExportPRTLoaders );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inExportPRTVolumes =
        fnNumericAttribute.create( "exportPRTVolumes", "exportPRTVolumes", MFnNumericData::kBoolean, 1 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inExportPRTVolumes );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inExportPRTSurfaces =
        fnNumericAttribute.create( "exportPRTSurfaces", "exportPRTSurfaces", MFnNumericData::kBoolean, 1 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inExportPRTSurfaces );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inExportPRTFractals =
        fnNumericAttribute.create( "exportPRTFractals", "exportPRTFractals", MFnNumericData::kBoolean, 1 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inExportPRTFractals );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inOverrideBG = fnNumericAttribute.create( "overrideBG", "overrideBG", MFnNumericData::kBoolean, 0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inOverrideBG );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inBackgroundColor = fnNumericAttribute.create( "backgroundColor", "backgroundColor", MFnNumericData::k3Double );
    fnNumericAttribute.setUsedAsColor( true );
    fnNumericAttribute.setDefault( 0.3, 0.5, 0.7 );
    status = addAttribute( inBackgroundColor );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inOverrideColor = fnNumericAttribute.create( "overrideColor", "overrideColor", MFnNumericData::kBoolean, 0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inOverrideColor );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inColorChannelOverride =
        fnNumericAttribute.create( "colorChannelOverride", "colorChannelOverride", MFnNumericData::k3Double );
    fnNumericAttribute.setUsedAsColor( true );
    fnNumericAttribute.setDefault( 1.0, 1.0, 1.0 );
    status = addAttribute( inColorChannelOverride );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inOverrideEmission =
        fnNumericAttribute.create( "overrideEmission", "overrideEmission", MFnNumericData::kBoolean, 0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inOverrideEmission );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inEmissionChannelOverride =
        fnNumericAttribute.create( "emissionChannelOverride", "emissionChannelOverride", MFnNumericData::k3Double );
    fnNumericAttribute.setUsedAsColor( true );
    fnNumericAttribute.setDefault( 1.0, 1.0, 1.0 );
    status = addAttribute( inEmissionChannelOverride );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inOverrideAbsorption =
        fnNumericAttribute.create( "overrideAbsorption", "overrideAbsorption", MFnNumericData::kBoolean, 0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inOverrideAbsorption );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inAbsorptionChannelOverride =
        fnNumericAttribute.create( "absorptionChannelOverride", "absorptionChannelOverride", MFnNumericData::k3Double );
    fnNumericAttribute.setUsedAsColor( true );
    fnNumericAttribute.setDefault( 0.0, 0.0, 0.0 );
    status = addAttribute( inAbsorptionChannelOverride );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inOverrideAbsorption =
        fnNumericAttribute.create( "overrideDensity", "overrideDensity", MFnNumericData::kBoolean, 0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inOverrideAbsorption );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inAbsorptionChannelOverride =
        fnNumericAttribute.create( "densityChannelOverride", "densityChannelOverride", MFnNumericData::kDouble, 1.0f );
    fnNumericAttribute.setMin( 0 );
    fnNumericAttribute.setSoftMax( 1 );
    status = addAttribute( inAbsorptionChannelOverride );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    for( int i = 0; i < NUMCHANNELCOPIERS; i++ ) { //
        MString baseName = MString( "copyChannel" ) + ( i + 1 );
        inCopyChannel[i] = fnNumericAttribute.create( baseName, baseName, MFnNumericData::kBoolean, 0 );
        fnNumericAttribute.setConnectable( false );
        status = addAttribute( inCopyChannel[i] );
        CHECK_MSTATUS_AND_RETURN_IT( status );

        inCopyChannelFrom[i] = fnEnumAttribute.create( baseName + "From", baseName + "From", 0 );
        fnEnumAttribute.addField( "Color", 0 );
        fnEnumAttribute.addField( "Emission", 1 );
        fnEnumAttribute.addField( "Absorption", 2 );
        fnEnumAttribute.addField( "Position", 3 );
        fnEnumAttribute.addField( "Velocity", 4 );
        fnEnumAttribute.addField( "Normal", 5 );
        fnEnumAttribute.addField( "Tangent", 6 );
        fnEnumAttribute.addField( "Lighting", 7 );
        status = addAttribute( inCopyChannelFrom[i] );
        CHECK_MSTATUS_AND_RETURN_IT( status );

        inCopyChannelTo[i] = fnEnumAttribute.create( baseName + "To", baseName + "To", 0 );
        fnEnumAttribute.addField( "Color", 0 );
        fnEnumAttribute.addField( "Emission", 1 );
        fnEnumAttribute.addField( "Absorption", 2 );
        status = addAttribute( inCopyChannelTo[i] );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }

    inSaveNormalPass = fnNumericAttribute.create( "saveNormalPass", "saveNormalPass", MFnNumericData::kBoolean, 0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inSaveNormalPass );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inSaveVelocityPass =
        fnNumericAttribute.create( "saveVelocityPass", "saveVelocityPass", MFnNumericData::kBoolean, 0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inSaveVelocityPass );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inSaveZDepthPass = fnNumericAttribute.create( "saveZDepthPass", "saveZDepthPass", MFnNumericData::kBoolean, 0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inSaveZDepthPass );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inSaveOccludedPass =
        fnNumericAttribute.create( "saveOccludedPass", "saveOccludedPass", MFnNumericData::kBoolean, 0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inSaveOccludedPass );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inPhongSpecularPower =
        fnNumericAttribute.create( "phongSpecularPower", "phongSpecularPower", MFnNumericData::kFloat, 10.0 );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( 0.0 );
    fnNumericAttribute.setMax( 1000.0 );
    status = addAttribute( inPhongSpecularPower );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inUsePhongSpecularPower = fnNumericAttribute.create( "usePhongSpecularPowerChannel", "usePhongSpecularPowerChannel",
                                                         MFnNumericData::kBoolean, 0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inUsePhongSpecularPower );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inPhongSpecularLevel =
        fnNumericAttribute.create( "phongSpecularLevel", "phongSpecularLevel", MFnNumericData::kFloat, 100.0 );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( 0.0 );
    fnNumericAttribute.setMax( 1000.0 );
    status = addAttribute( inPhongSpecularLevel );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inUsePhongSpecularPower = fnNumericAttribute.create( "usePhongSpecularLevelChannel", "usePhongSpecularLevelChannel",
                                                         MFnNumericData::kBoolean, 0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inUsePhongSpecularPower );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inHgEccentricity = fnNumericAttribute.create( "hgEccentricity", "hgEccentricity", MFnNumericData::kFloat, 0.0 );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( -1.0 );
    fnNumericAttribute.setMax( 1.0 );
    status = addAttribute( inHgEccentricity );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inUseHgEccentricity = fnNumericAttribute.create( "useHgEccentricityChannel", "useHgEccentricityChannel",
                                                     MFnNumericData::kBoolean, 0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inUseHgEccentricity );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inSchlickEccentricity =
        fnNumericAttribute.create( "schlickEccentricity", "schlickEccentricity", MFnNumericData::kFloat, 0.0 );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( -1 );
    fnNumericAttribute.setMax( 1.0 );
    status = addAttribute( inSchlickEccentricity );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inUseSchlickEccentricity = fnNumericAttribute.create(
        "useSchlickEccentricityChannel", "useSchlickEccentricityChannel", MFnNumericData::kBoolean, 0.0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inUseSchlickEccentricity );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inKkSpecularPower = fnNumericAttribute.create( "kkSpecularPower", "kkSpecularPower", MFnNumericData::kFloat, 10.0 );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( 0.0 );
    fnNumericAttribute.setMax( 10000.0 );
    status = addAttribute( inKkSpecularPower );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inUseKkSpecularPower = fnNumericAttribute.create( "useKkSpecularPowerChannel", "useKkSpecularPowerChannel",
                                                      MFnNumericData::kBoolean, 0.0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inUseKkSpecularPower );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inKkSpecularLevel =
        fnNumericAttribute.create( "kkSpecularLevel", "kkSpecularLevel", MFnNumericData::kFloat, 100.0 );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( 0.0 );
    fnNumericAttribute.setMax( 100000.0 );
    status = addAttribute( inKkSpecularLevel );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inUseKkSpecularLevel = fnNumericAttribute.create( "useKkSpecularLevelChannel", "useKkSpecularLevelChannel",
                                                      MFnNumericData::kBoolean, 0.0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inUseKkSpecularLevel );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inMarschnerSpecularLevel =
        fnNumericAttribute.create( "marschnerSpecularLevel", "marschnerSpecularLevel", MFnNumericData::kFloat, 25.0 );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( 0.0 );
    fnNumericAttribute.setMax( 100000.0 );
    status = addAttribute( inMarschnerSpecularLevel );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inUseMarschnerSpecularLevel = fnNumericAttribute.create(
        "useMarschnerSpecularLevelChannel", "useMarschnerSpecularLevelChannel", MFnNumericData::kBoolean, 0.0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inUseMarschnerSpecularLevel );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inMarschnerSpecular2Level =
        fnNumericAttribute.create( "marschnerSpecular2Level", "marschnerSpecular2Level", MFnNumericData::kFloat, 90 );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( 0.0 );
    fnNumericAttribute.setMax( 10000.0 );
    status = addAttribute( inMarschnerSpecular2Level );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inUseMarschnerSpecular2Level = fnNumericAttribute.create(
        "useMarschnerSpecular2LevelChannel", "useMarschnerSpecular2LevelChannel", MFnNumericData::kBoolean, 0.0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inUseMarschnerSpecular2Level );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inMarschnerSpecularGlossiness = fnNumericAttribute.create(
        "marschnerSpecularGlossiness", "marschnerSpecularGlossiness", MFnNumericData::kFloat, 300.0 );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( 0.0 );
    fnNumericAttribute.setMax( 1000.0 );
    status = addAttribute( inMarschnerSpecularGlossiness );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inUseMarschnerSpecularGlossiness =
        fnNumericAttribute.create( "useMarschnerSpecularGlossinessChannel", "useMarschnerSpecularGlossinessChannel",
                                   MFnNumericData::kBoolean, 0.0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inUseMarschnerSpecularGlossiness );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inMarschnerSpecular2Glossiness = fnNumericAttribute.create(
        "marschnerSpecular2Glossiness", "marschnerSpecular2Glossiness", MFnNumericData::kFloat, 30.0 );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( 0.0 );
    fnNumericAttribute.setMax( 1000.0 );
    status = addAttribute( inMarschnerSpecular2Glossiness );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inUseMarschnerSpecular2Glossiness =
        fnNumericAttribute.create( "useMarschnerSpecular2GlossinessChannel", "useMarschnerSpecular2GlossinessChannel",
                                   MFnNumericData::kBoolean, 0.0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inUseMarschnerSpecular2Glossiness );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inMarschnerSpecularShift =
        fnNumericAttribute.create( "marschnerSpecularShift", "marschnerSpecularShift", MFnNumericData::kFloat, 0.1 );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( -1.0 );
    fnNumericAttribute.setMax( 1.0 );
    status = addAttribute( inMarschnerSpecularShift );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inUseMarschnerSpecularShift = fnNumericAttribute.create(
        "useMarschnerSpecularShiftChannel", "useMarschnerSpecularShiftChannel", MFnNumericData::kBoolean, 0.0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inUseMarschnerSpecularShift );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inMarschnerSpecular2Shift =
        fnNumericAttribute.create( "marschnerSpecular2Shift", "marschnerSpecular2Shift", MFnNumericData::kFloat, -0.1 );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( -1.0 );
    fnNumericAttribute.setMax( 1.0 );
    status = addAttribute( inMarschnerSpecular2Shift );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inUseMarschnerSpecular2Shift = fnNumericAttribute.create(
        "useMarschnerSpecular2ShiftChannel", "useMarschnerSpecular2ShiftChannel", MFnNumericData::kBoolean, 0.0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inUseMarschnerSpecular2Shift );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inMarschnerGlintLevel =
        fnNumericAttribute.create( "marschnerGlintLevel", "marschnerGlintLevel", MFnNumericData::kFloat, 400.0 );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( 0.0 );
    fnNumericAttribute.setMax( 100000.0 );
    status = addAttribute( inMarschnerGlintLevel );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inUseMarschnerGlintLevel = fnNumericAttribute.create(
        "useMarschnerGlintLevelChannel", "useMarschnerGlintLevelChannel", MFnNumericData::kBoolean, 0.0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inUseMarschnerGlintLevel );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inMarschnerGlintSize =
        fnNumericAttribute.create( "marschnerGlintSize", "marschnerGlintSize", MFnNumericData::kFloat, 0.5 );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( 0.0 );
    fnNumericAttribute.setMax( 360.0 );
    status = addAttribute( inMarschnerGlintSize );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inUseMarschnerGlintSize = fnNumericAttribute.create( "useMarschnerGlintSizeChannel", "useMarschnerGlintSizeChannel",
                                                         MFnNumericData::kBoolean, 0.0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inUseMarschnerGlintSize );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inMarschnerGlintGlossiness = fnNumericAttribute.create( "marschnerGlintGlossiness", "marschnerGlintGlossiness",
                                                            MFnNumericData::kFloat, 10.0 );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( 0.0 );
    fnNumericAttribute.setMax( 360.0 );
    status = addAttribute( inMarschnerGlintGlossiness );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inUseMarschnerGlintGlossiness = fnNumericAttribute.create(
        "useMarschnerGlintGlossinessChannel", "useMarschnerGlintGlossinessChannel", MFnNumericData::kBoolean, 0.0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inUseMarschnerGlintGlossiness );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inMarschnerDiffuseLevel =
        fnNumericAttribute.create( "marschnerDiffuseLevel", "marschnerDiffuseLevel", MFnNumericData::kFloat, 100.0 );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( 0.0 );
    fnNumericAttribute.setMax( 100.0 );
    status = addAttribute( inMarschnerDiffuseLevel );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inUseMarschnerDiffuseLevel = fnNumericAttribute.create(
        "useMarschnerDiffuseLevelChannel", "useMarschnerDiffuseLevelChannel", MFnNumericData::kBoolean, 0.0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inUseMarschnerDiffuseLevel );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inShadingMode = fnEnumAttribute.create( "shadingMode", "shadingMode", 0 );
    fnEnumAttribute.addField( "Isotropic", 0 );
    fnEnumAttribute.addField( "Phong Surface", 1 );
    fnEnumAttribute.addField( "Henyey-Greenstein", 2 );
    fnEnumAttribute.addField( "Schlick", 3 );
    fnEnumAttribute.addField( "Kajiya-Kay", 4 );
    fnEnumAttribute.addField( "Marschner", 5 );
    status = addAttribute( inShadingMode );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inRenderingMethod = fnEnumAttribute.create( "renderingMethod", "renderingMethod", 0 );
    fnEnumAttribute.addField( "Particles", 0 );
    fnEnumAttribute.addField( "Voxels", 1 );
    status = addAttribute( inRenderingMethod );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inIgnoreSceneLights =
        fnNumericAttribute.create( "ignoreSceneLights", "ignoreSceneLights", MFnNumericData::kBoolean, 0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inIgnoreSceneLights );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inVoxelSize = fnNumericAttribute.create( "voxelSize", "voxelSize", MFnNumericData::kFloat, 0.5 );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( 0.001 );
    fnNumericAttribute.setMax( 100.0 );
    status = addAttribute( inVoxelSize );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inVoxelFilterRadius =
        fnNumericAttribute.create( "voxelFilterRadius", "voxelFilterRadius", MFnNumericData::kLong, 1 );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( 1 );
    fnNumericAttribute.setMax( 10 );
    status = addAttribute( inVoxelFilterRadius );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inUseEmission = fnNumericAttribute.create( "useEmission", "useEmission", MFnNumericData::kBoolean, 0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inUseEmission );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inUseAbsorption = fnNumericAttribute.create( "useAbsorption", "useAbsorption", MFnNumericData::kBoolean, 0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inUseAbsorption );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inForceAdditiveMode =
        fnNumericAttribute.create( "forceAdditiveMode", "forceAdditiveMode", MFnNumericData::kBoolean, 0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inForceAdditiveMode );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inLoadPercentage = fnNumericAttribute.create( "loadPercentage", "loadPercentage", MFnNumericData::kFloat, 100.0 );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( 0.0 );
    fnNumericAttribute.setMax( 100.0 );
    status = addAttribute( inLoadPercentage );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inSelfShadowFilter = fnEnumAttribute.create( "selfShadowFilter", "selfShadowFilter", 2 );
    fnEnumAttribute.addField( "Nearest", 0 );
    fnEnumAttribute.addField( "Bilinear", 1 );
    fnEnumAttribute.addField( "Bicubic", 2 );
    status = addAttribute( inSelfShadowFilter );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inDrawPointFilter = fnEnumAttribute.create( "drawPointFilter", "drawPointFilter", 1 );
    fnEnumAttribute.addField( "Nearest", 0 );
    fnEnumAttribute.addField( "Bilinear", 1 );
    fnEnumAttribute.addField( "Bicubic", 2 );
    status = addAttribute( inDrawPointFilter );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inLightingPassFilterSize =
        fnNumericAttribute.create( "lightingPassFilterSize", "lightingPassFilterSize", MFnNumericData::kLong, 1 );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( 1 );
    fnNumericAttribute.setMax( 64 );
    status = addAttribute( inLightingPassFilterSize );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inFinalPassFilterSize =
        fnNumericAttribute.create( "finalPassFilterSize", "finalPassFilterSize", MFnNumericData::kLong, 1 );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( 1 );
    fnNumericAttribute.setMax( 64 );
    status = addAttribute( inFinalPassFilterSize );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inLightingPassDensity =
        fnNumericAttribute.create( "lightingPassDensity", "lightingPassDensity", MFnNumericData::kFloat, 5 );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( 0.0 );
    fnNumericAttribute.setMax( 10.0 );
    status = addAttribute( inLightingPassDensity );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inFinalPassDensity = fnNumericAttribute.create( "finalPassDensity", "finalPassDensity", MFnNumericData::kFloat, 5 );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( 0.0 );
    fnNumericAttribute.setMax( 10.0 );
    status = addAttribute( inFinalPassDensity );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inLightingPassDensityExponent = fnNumericAttribute.create(
        "lightingPassDensityExponent", "lightingPassDensityExponent", MFnNumericData::kLong, -1 );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( -32 );
    fnNumericAttribute.setMax( 32 );
    status = addAttribute( inLightingPassDensityExponent );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inFinalPassDensityExponent =
        fnNumericAttribute.create( "finalPassDensityExponent", "finalPassDensityExponent", MFnNumericData::kLong, -1 );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( -32 );
    fnNumericAttribute.setMax( 32 );
    status = addAttribute( inFinalPassDensityExponent );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inUseLightingPassDensity =
        fnNumericAttribute.create( "useLightingPassDensity", "useLightingPassDensity", MFnNumericData::kBoolean, 0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inUseLightingPassDensity );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inEmissionStrength = fnNumericAttribute.create( "emissionStrength", "emissionStrength", MFnNumericData::kFloat, 5 );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( -100.0 );
    fnNumericAttribute.setMax( 100.0 );
    status = addAttribute( inEmissionStrength );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inUseEmissionStrength =
        fnNumericAttribute.create( "useEmissionStrength", "useEmissionStrength", MFnNumericData::kBoolean, 0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inUseEmissionStrength );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inEmissionStrengthExponent =
        fnNumericAttribute.create( "emissionStrengthExponent", "emissionStrengthExponent", MFnNumericData::kLong, -1 );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( -32 );
    fnNumericAttribute.setMax( 32 );
    status = addAttribute( inEmissionStrengthExponent );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inEnableMotionBlur =
        fnNumericAttribute.create( "enableMotionBlur", "enableMotionBlur", MFnNumericData::kBoolean, 0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inEnableMotionBlur );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inMotionBlurParticleSegments = fnNumericAttribute.create( "motionBlurParticleSegments",
                                                              "motionBlurParticleSegments", MFnNumericData::kLong, 2 );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( 1 );
    fnNumericAttribute.setMax( 64 );
    status = addAttribute( inMotionBlurParticleSegments );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inJitteredMotionBlur =
        fnNumericAttribute.create( "jitteredMotionBlur", "jitteredMotionBlur", MFnNumericData::kBoolean, 0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inJitteredMotionBlur );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inShutterAngle = fnNumericAttribute.create( "shutterAngle", "shutterAngle", MFnNumericData::kFloat, 180 );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( 0 );
    fnNumericAttribute.setMax( 2000 );
    fnNumericAttribute.setSoftMax( 360 );
    status = addAttribute( inShutterAngle );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inEnableDOF = fnNumericAttribute.create( "enableDOF", "enableDOF", MFnNumericData::kBoolean, 0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inEnableDOF );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inSampleRateDOF = fnNumericAttribute.create( "sampleRateDOF", "sampleRateDOF", MFnNumericData::kFloat, 0.1 );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( 0.1 );
    fnNumericAttribute.setMax( 10.0 );
    status = addAttribute( inSampleRateDOF );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inDisableCameraBlur =
        fnNumericAttribute.create( "disableCameraBlur", "disableCameraBlur", MFnNumericData::kBoolean, 0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inDisableCameraBlur );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inMotionBlurBias = fnNumericAttribute.create( "motionBlurBias", "motionBlurBias", MFnNumericData::kFloat, 0.0 );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( -1.0 );
    fnNumericAttribute.setMax( 1.0 );
    status = addAttribute( inMotionBlurBias );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inEnableAdaptiveMotionBlur = fnNumericAttribute.create( "enableAdaptiveMotionBlur", "enableAdaptiveMotionBlur",
                                                            MFnNumericData::kBoolean, 0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inEnableAdaptiveMotionBlur );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inAdaptiveMotionBlurMinSamples = fnNumericAttribute.create(
        "adaptiveMotionBlurMinSamples", "adaptiveMotionBlurMinSamples", MFnNumericData::kInt, 2 );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( 1 );
    fnNumericAttribute.setMax( 32768 );
    fnNumericAttribute.setSoftMax( 2048 );
    status = addAttribute( inAdaptiveMotionBlurMinSamples );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inAdaptiveMotionBlurMaxSamples = fnNumericAttribute.create(
        "adaptiveMotionBlurMaxSamples", "adaptiveMotionBlurMaxSamples", MFnNumericData::kInt, 1024 );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( 1 );
    fnNumericAttribute.setMax( 32768 );
    fnNumericAttribute.setSoftMax( 2048 );
    status = addAttribute( inAdaptiveMotionBlurMaxSamples );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inAdaptiveMotionBlurSmoothness = fnNumericAttribute.create(
        "adaptiveMotionBlurSmoothness", "adaptiveMotionBlurSmoothness", MFnNumericData::kFloat, 1.0f );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( 0.0f );
    fnNumericAttribute.setMax( 1024.0f );
    fnNumericAttribute.setSoftMax( 8.0f );
    status = addAttribute( inAdaptiveMotionBlurSmoothness );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inAdaptiveMotionBlurExpoenent = fnNumericAttribute.create(
        "adaptiveMotionBlurExponent", "adaptiveMotionBlurExponent", MFnNumericData::kFloat, 1.0f );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( 0.0f );
    fnNumericAttribute.setMax( 1024.0f );
    fnNumericAttribute.setSoftMax( 8.0f );
    status = addAttribute( inAdaptiveMotionBlurExpoenent );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inEnableBokehShapeMap =
        fnNumericAttribute.create( "enableBokehShapeMap", "enableBokehShapeMap", MFnNumericData::kBoolean, false );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inEnableBokehShapeMap );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inBokehShapeMap = fnNumericAttribute.create( "bokehShapeMap", "bokehShapeMap", MFnNumericData::k3Double );
    fnNumericAttribute.setUsedAsColor( true );
    status = addAttribute( inBokehShapeMap );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inEnableBokehBlendMap =
        fnNumericAttribute.create( "enableBokehBlendMap", "enableBokehBlendMap", MFnNumericData::kBoolean, false );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inEnableBokehBlendMap );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inBokehBlendMap = fnNumericAttribute.create( "bokehBlendMap", "bokehBlendMap", MFnNumericData::k3Double );
    fnNumericAttribute.setUsedAsColor( true );
    status = addAttribute( inBokehBlendMap );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inBokehBlendInfluence =
        fnNumericAttribute.create( "bokehBlendInfluence", "bokehBlendInfluence", MFnNumericData::kFloat, 1.0f );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( 0.01f );
    fnNumericAttribute.setSoftMax( 1.0f );
    status = addAttribute( inBokehBlendInfluence );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inBokehEnableAnamorphicSqueeze = fnNumericAttribute.create(
        "enableBokehAnamorphicSqueeze", "enableBokehAnamorphicSqueeze", MFnNumericData::kBoolean, false );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inBokehEnableAnamorphicSqueeze );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inBokehAnamorphicSqueeze =
        fnNumericAttribute.create( "bokehAnamorphicSqueeze", "bokehAnamorphicSqueeze", MFnNumericData::kFloat, 1.0f );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( 0.1f );
    fnNumericAttribute.setMax( 10.0f );
    status = addAttribute( inBokehAnamorphicSqueeze );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inBokehBlendMipmapScale =
        fnNumericAttribute.create( "bokehBlendMipmapScale", "bokehBlendMipmapScale", MFnNumericData::kInt, 1 );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( 1 );
    fnNumericAttribute.setMax( 256 );
    status = addAttribute( inBokehBlendMipmapScale );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inAllocateBokehBlendInfluence = fnNumericAttribute.create(
        "allocateBokehBlendInfluence", "allocateBokehBlendInfluence", MFnNumericData::kBoolean, false );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inAllocateBokehBlendInfluence );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inEnableMatteObjects =
        fnNumericAttribute.create( "enableMatteObjects", "enableMatteObjects", MFnNumericData::kBoolean, 0 );
    fnNumericAttribute.setConnectable( false );
    status = addAttribute( inEnableMatteObjects );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inMatteSuperSampling =
        fnNumericAttribute.create( "matteSuperSampling", "matteSuperSampling", MFnNumericData::kLong, 1 );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( 1 );
    fnNumericAttribute.setMax( 8 );
    status = addAttribute( inMatteSuperSampling );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inLogLevel = fnEnumAttribute.create( "logLevel", "logLevel", 4 );
    fnEnumAttribute.addField( "none", 0 );
    fnEnumAttribute.addField( "errors", 1 );
    fnEnumAttribute.addField( "warnings", 2 );
    fnEnumAttribute.addField( "progress", 3 );
    fnEnumAttribute.addField( "stats", 4 );
    fnEnumAttribute.addField( "debug", 5 );
    status = addAttribute( inLogLevel );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inThreadCount = fnNumericAttribute.create( "threadCount", "threadCount", MFnNumericData::kLong, 0 );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( -1 );
    fnNumericAttribute.setMax( 1024 );
    fnNumericAttribute.setSoftMax( 32 );
    status = addAttribute( inThreadCount );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    inFrameBufferAvailableMemoryFraction = fnNumericAttribute.create(
        "frameBufferAvailableMemoryFraction", "frameBufferAvailableMemoryFaction", MFnNumericData::kFloat, 0.75f );
    fnNumericAttribute.setConnectable( false );
    fnNumericAttribute.setMin( 0.1f );
    fnNumericAttribute.setMax( 1.0f );
    status = addAttribute( inFrameBufferAvailableMemoryFraction );
    CHECK_MSTATUS_AND_RETURN_IT( status );

    return MStatus::kSuccess;
}
