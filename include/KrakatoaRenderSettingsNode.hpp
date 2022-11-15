// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <maya/MPxLocatorNode.h>
class KrakatoaRenderSettingsNode : public MPxNode {
  public:
    static void* creator();
    static MStatus initialize();
    static MTypeId typeId;
    static MString typeName;

    KrakatoaRenderSettingsNode( void );
    ~KrakatoaRenderSettingsNode( void );

  private:
    static MObject inForceEXROutput;
    static MObject inEXRCompressionType;
    static MObject inEXRRgbaBitDepth;
    static MObject inEXRNormalBitDepth;
    static MObject inEXRVelocityBitDepth;
    static MObject inEXRZBitDepth;
    static MObject inEXROccludedBitDepth;

    static MObject inExportMayaParticles;
    static MObject inExportMayaHair;
    static MObject inExportPRTLoaders;
    static MObject inExportPRTVolumes;
    static MObject inExportPRTSurfaces;
    static MObject inExportPRTFractals;

    static MObject inOverrideBG;
    static MObject inBackgroundColor;
    static MObject inOverrideColor;
    static MObject inColorChannelOverride;
    static MObject inOverrideEmission;
    static MObject inEmissionChannelOverride;
    static MObject inOverrideAbsorption;
    static MObject inAbsorptionChannelOverride;
    static MObject inOverrideDensity;
    static MObject inDensityChannelOverride;

    static MObject inCopyChannel[];
    static MObject inCopyChannelFrom[];
    static MObject inCopyChannelTo[];

    static MObject inSaveNormalPass;
    static MObject inSaveVelocityPass;
    static MObject inSaveZDepthPass;
    static MObject inSaveOccludedPass;

    static MObject inPhongSpecularPower;
    static MObject inPhongSpecularLevel;
    static MObject inUsePhongSpecularPower;
    static MObject inUsePhongSpecularLevel;

    static MObject inHgEccentricity;
    static MObject inUseHgEccentricity;

    static MObject inSchlickEccentricity;
    static MObject inUseSchlickEccentricity;

    static MObject inKkSpecularPower;
    static MObject inKkSpecularLevel;
    static MObject inUseKkSpecularPower;
    static MObject inUseKkSpecularLevel;

    static MObject inMarschnerSpecularLevel;
    static MObject inMarschnerSpecular2Level;
    static MObject inMarschnerSpecularGlossiness;
    static MObject inMarschnerSpecular2Glossiness;
    static MObject inMarschnerSpecularShift;
    static MObject inMarschnerSpecular2Shift;
    static MObject inMarschnerGlintLevel;
    static MObject inMarschnerGlintSize;
    static MObject inMarschnerGlintGlossiness;
    static MObject inMarschnerDiffuseLevel;
    static MObject inUseMarschnerSpecularLevel;
    static MObject inUseMarschnerSpecular2Level;
    static MObject inUseMarschnerSpecularGlossiness;
    static MObject inUseMarschnerSpecular2Glossiness;
    static MObject inUseMarschnerSpecularShift;
    static MObject inUseMarschnerSpecular2Shift;
    static MObject inUseMarschnerGlintLevel;
    static MObject inUseMarschnerGlintSize;
    static MObject inUseMarschnerGlintGlossiness;
    static MObject inUseMarschnerDiffuseLevel;

    static MObject inShadingMode;
    static MObject inRenderingMethod;
    static MObject inIgnoreSceneLights;
    static MObject inVoxelSize;
    static MObject inVoxelFilterRadius;
    static MObject inUseEmission;
    static MObject inUseAbsorption;
    static MObject inForceAdditiveMode;
    static MObject inLoadPercentage;

    static MObject inSelfShadowFilter;
    static MObject inDrawPointFilter;
    static MObject inLightingPassFilterSize;
    static MObject inFinalPassFilterSize;
    static MObject inLightingPassDensity;
    static MObject inFinalPassDensity;
    static MObject inLightingPassDensityExponent;
    static MObject inFinalPassDensityExponent;
    static MObject inUseLightingPassDensity;
    static MObject inEmissionStrength;
    static MObject inUseEmissionStrength;
    static MObject inEmissionStrengthExponent;

    static MObject inEnableMotionBlur;
    static MObject inMotionBlurParticleSegments;
    static MObject inJitteredMotionBlur;
    static MObject inShutterAngle;
    static MObject inEnableDOF;
    static MObject inSampleRateDOF;
    static MObject inDisableCameraBlur;
    static MObject inMotionBlurBias;

    static MObject inEnableAdaptiveMotionBlur;
    static MObject inAdaptiveMotionBlurMinSamples;
    static MObject inAdaptiveMotionBlurMaxSamples;
    static MObject inAdaptiveMotionBlurSmoothness;
    static MObject inAdaptiveMotionBlurExpoenent;

    static MObject inEnableBokehShapeMap;
    static MObject inBokehShapeMap;
    static MObject inEnableBokehBlendMap;
    static MObject inBokehBlendMap;
    static MObject inBokehBlendInfluence;
    static MObject inBokehEnableAnamorphicSqueeze;
    static MObject inBokehAnamorphicSqueeze;
    static MObject inBokehBlendMipmapScale;
    static MObject inAllocateBokehBlendInfluence;

    static MObject inEnableMatteObjects;
    static MObject inMatteSuperSampling;

    static MObject inLogLevel;
    static MObject inThreadCount;
    static MObject inFrameBufferAvailableMemoryFraction;
};
