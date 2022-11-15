// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <frantic/maya/PRTObject_base.hpp>

#include <frantic/particles/streams/particle_istream.hpp>
#include <krakatoasr_renderer.hpp>
#include <krakatoasr_renderer/progress_logger.hpp>

#include <maya/MPxNode.h>

class PRTModifiers : public MPxNode, public frantic::maya::PRTObjectBase {
  public:
    // Maya rtti and object creation information
    static void* creator();
    static MStatus initialize();

    static const MTypeId id;
    static const MString typeName;

  private:
    // Input particles to be modified
    static MObject inParticleStream;

    // Output particles
    static MObject outParticleStream;

    // Enabled
    static MObject inEnabled;

    // Params
    static MObject inModifiersMethod;

    static MObject inModifiersName;

    // Set Channel Vec3, Set Channel, Scale Channel, Copy Channel
    // static MObject inChannelDestination;
    // static MObject inChannelSource;
    // static MObject inChannelValue;
    // static MObject inChannelValueVec3;

    // Copy Channel
    static MObject inCopyFull;

    // Repopulate

    // Texture

  public:
    PRTModifiers();
    virtual ~PRTModifiers();
    virtual void postConstructor();
    virtual MStatus compute( const MPlug& plug, MDataBlock& block );

    // inherited from base
    frantic::maya::PRTObjectBase::particle_istream_ptr
    getParticleStream( const frantic::graphics::transform4f& objectSpace, const MDGContext& context,
                       bool isViewport ) const;

    virtual frantic::maya::PRTObjectBase::particle_istream_ptr
    getRenderParticleStream( const frantic::graphics::transform4f& objectSpace, const MDGContext& context ) const {
        return getParticleStream( objectSpace, context, false );
    }

    virtual frantic::maya::PRTObjectBase::particle_istream_ptr
    getViewportParticleStream( const frantic::graphics::transform4f& objectSpace, const MDGContext& context ) const {
        return getParticleStream( objectSpace, context, true );
    }

    bool isEnabled( const MDGContext& context = MDGContext::fsNormal ) const;

    int getModifierMethod( MString& outName, const MDGContext& context = MDGContext::fsNormal ) const;

  private:
    frantic::particles::streams::particle_istream_ptr
    apply_channel_modifiers( const MDGContext& context, bool inViewport,
                             frantic::particles::streams::particle_istream_ptr inputParticles ) const;
};
