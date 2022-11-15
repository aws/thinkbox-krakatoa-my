// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include "PRTIconMesh.hpp"
#include "PRTObject.hpp"

#include <maya/MNodeMessage.h>
#if MAYA_API_VERSION >= 201400
#include <maya/MViewport2Renderer.h>
#include <maya/MDrawContext.h>
#endif

#include <maya/MDagPathArray.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnPluginData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MStatus.h>

#include <frantic/maya/maya_util.hpp>

#include <frantic/magma/maya/maya_magma_mel_window.hpp>

using namespace frantic::maya;

frantic::graphics::transform4f PRTObject::getTransform( const MDGContext& context ) const {
    MStatus stat;

    MFnDagNode dagNode( this->thisMObject(), &stat );
    if( stat == MS::kSuccess ) {
        MDagPathArray dagNodePathArray;
        dagNode.getAllPaths( dagNodePathArray );

        // iterate through all the paths get the transform from the first path that has it
        frantic::graphics::transform4f result;
        for( unsigned int i = 0; i < dagNodePathArray.length(); i++ ) {
            if( maya_util::get_object_world_matrix( dagNodePathArray[i], context, result ) )
                return result;
        }
    }

    frantic::graphics::transform4f emptyTrans;
    return emptyTrans;
}

const frantic::geometry::trimesh3& PRTObject::getRootMesh() const { return get_default_icon_mesh(); }

#if MAYA_API_VERSION >= 201600
MSelectionMask PRTObject::getShapeSelectionMask() const {
    MSelectionMask::SelectionType selType = MSelectionMask::kSelectObjectsMask;
    return MSelectionMask( selType );
}
#endif

void render_end_callback( MHWRender::MDrawContext& context, void* clientData ) {
    MHWRender::MRenderer::disableChangeManagementUntilNextRefresh();
}

void PRTObject::postConstructor() {
    MPxSurfaceShape::postConstructor();

    // Setup Viewport 2.0 rendering callback
    MHWRender::MRenderer* renderer = MHWRender::MRenderer::theRenderer( false );
    if( renderer && renderer->drawAPI() != MHWRender::kNone ) {
        renderer->addNotification( &render_end_callback, "PRTObjectRenderEndCallback",
                                   MHWRender::MPassContext::kEndRenderSemantic, NULL );
    }
}

PRTObject::~PRTObject() {}

void PRTObject::register_viewport_dependency( boost::unordered_set<std::string>& registry, MObject& attr,
                                              MObject& affected ) {
    attributeAffects( attr, affected );
    registry.insert( std::string( MFnAttribute( attr ).name().asChar() ) );
}

void PRTObject::check_dependents_dirty( boost::unordered_set<std::string>& registry, const MPlug& attr,
                                        MObject& toUpdate ) {
// for viewport 2.0. calling the "setGeometryDrawDirty" function.
#if MAYA_API_VERSION >= 201400
    boost::unordered_set<std::string>::const_iterator elemIter =
        registry.find( std::string( MFnAttribute( attr ).name().asChar() ) );

    if( elemIter != registry.end() ) {
        MHWRender::MRenderer::setGeometryDrawDirty( toUpdate );
    }
#endif
}

void PRTObject::register_osx_viewport20_update_hack(
    MString objName, const boost::unordered_set<std::string>& viewportInputDependencies ) {
#ifdef __APPLE__
#if MAYA_API_VERSION >= 201400
    // Viewport 2.0 Hack for OS X. See comment in PRTObjectUIGeometryOverride.hpp.
    // For some reason I can't do this in "postConstructor" because this->name() returns an empty string at that point.
    std::string objNameStr = objName.asChar();
    boost::unordered_set<std::string>::const_iterator iter, iterEnd = viewportInputDependencies.end();
    for( iter = viewportInputDependencies.begin(); iter != iterEnd; ++iter ) {
        std::string attrName = *iter;
        if( !attrName.empty() ) {
            // The MEL script we execute looks something like this:
            // scriptJob -kws -ac "PRTFractal1.inFractalRandomSeed" "ForceViewport20Update \"PRTFractal1\"";
            // What this does is calls our own "ForceViewport20Update" function whenever the attribute specified
            // changes. What our "ForceViewport20Update" function does is manually call
            // "MHWRender::MRenderer::setGeometryDrawDirty" on our node. I wish there was a way to do this without the
            // MEL intermediate. However, our proper "setDependentsDirty" function doesn't appear to ever be called
            // under OS X.
            std::string melCommand = "scriptJob -kws -ac \"" + objNameStr + "." + attrName +
                                     "\" \"ForceViewport20Update \\\"" + objNameStr + "\\\"\";";
            MGlobal::executeCommand( melCommand.c_str() );
        }
    }
#endif
#endif
}
