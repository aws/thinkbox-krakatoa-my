// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <maya/MFnPlugin.h>
#include <maya/MGlobal.h>
#include <maya/MItDag.h>

#include <frantic/maya/plugin_manager.hpp>
#include <frantic/maya/shelf.hpp>
#include <frantic/strings/tstring.hpp>

#include <frantic/magma/maya/maya_magma_initializer.hpp>

#include <frantic/maya/MPxParticleStream.hpp>
#include <frantic/maya/PRTMayaParticle.hpp>

#include "GenerateStickyChannelsCommand.hpp"
#include "KrakatoaParticleJitter.hpp"
#include "KrakatoaPluginDirectory.hpp"
#include "KrakatoaRender.hpp"
#include "KrakatoaRenderSettingsNode.hpp"
#include "KrakatoaSettings.hpp"
#include "KrakatoaVersionCommand.hpp"
#include "PRTExporter.hpp"
#include "PRTFractal.hpp"
#include "PRTLoader.hpp"
#include "PRTModifiers.hpp"
#include "PRTObject.hpp"
#include "PRTObjectUI.hpp"
#include "PRTSaver.hpp"
#include "PRTSurface.hpp"
#include "PRTVolume.hpp"

#include "PRTObjectUIGeometryOverride.hpp"

#include "maya_ksr.hpp"

#include "../KrakatoaVersion.h"

#include <sstream>
#include <stack>
#include <utility>
#include <vector>

#ifdef _WIN32
#define EXPORT __declspec( dllexport )
#elif __GNUC__ >= 4
#define EXPORT __attribute__( ( visibility( "default" ) ) )
#else
#define EXPORT
#endif

#define CHECK_INIT_STATUS( status, msg )                                                                               \
    if( !status ) {                                                                                                    \
        status.perror( msg );                                                                                          \
        return status;                                                                                                 \
    }

namespace {

const frantic::tstring KrakatoaShelfName = _T( "Krakatoa" ); // the shelf name cannot have spaces in it

const frantic::tstring CreatePRTLoaderCommandName = _T( "Create PRTLoader Node" );
const frantic::tstring CreatePRTLoaderToolTip =
    _T( "Creates a new PRTLoader node to add .prt particle data files to the scene" );
const frantic::tstring CreatePRTLoaderIconFileName = _T( "krakatoa16_prtloader_icon.png" );
const frantic::tstring CreatePRTLoaderPython =
    _T( "python(\\\"import MayaKrakatoa; MayaKrakatoa.CreatePRTLoaderNode();\\\");" );

const frantic::tstring CreatePRTVolumeCommandName = _T( "Create PRTVolume Node" );
const frantic::tstring CreatePRTVolumeToolTip =
    _T( "Creates a new PRTVolume node to create a particle cloud from a mesh shape" );
const frantic::tstring CreatePRTVolumeIconFileName = _T( "krakatoa16_prtvolume_icon.png" );
const frantic::tstring CreatePRTVolumePython =
    _T( "python(\\\"import MayaKrakatoa; MayaKrakatoa.CreatePRTVolumeNode();\\\");" );

const frantic::tstring CreatePRTSurfaceCommandName = _T( "Create PRTSurface Node" );
const frantic::tstring CreatePRTSurfaceToolTip =
    _T( "Creates a new PRTSurface node to create a particle surface from a mesh shape" );
const frantic::tstring CreatePRTSurfaceIconFileName = _T( "krakatoa16_prtsurface_icon.png" );
const frantic::tstring CreatePRTSurfacePython =
    _T( "python(\\\"import MayaKrakatoa; MayaKrakatoa.CreatePRTSurfaceNode();\\\");" );

const frantic::tstring CreatePRTFractalCommandName = _T( "Create PRTFractal Node" );
const frantic::tstring CreatePRTFractalToolTip = _T( "Creates a new PRTFractal" );
const frantic::tstring CreatePRTFractalIconFileName = _T( "krakatoa16_prtfractals_icon.png" );
const frantic::tstring CreatePRTFractalPython =
    _T( "python(\\\"import MayaKrakatoa; MayaKrakatoa.CreatePRTFractalNode();\\\");" );

const frantic::tstring OpenPRTSaverCommandName = _T( "Open PRT Saver and Partition Dialog" );
const frantic::tstring OpenPRTSaverToolTip = _T( "Open PRT Saver and Partition Dialog" );
const frantic::tstring OpenPRTSaverIconFileName = _T( "krakatoa16_prtsaver_icon.png" );
const frantic::tstring OpenPRTSaverPython = _T( "python(\\\"import MayaKrakatoa; MayaKrakatoa.OpenPRTSaver();\\\");" );

const frantic::tstring OpenModifierDialogCommandName = _T( "Open Particle Modifier Dialog" );
const frantic::tstring OpenModifierDialogToolTip = _T( "Open Particle Modifier Dialog" );
const frantic::tstring OpenModifierDialogIconFileName = _T( "krakatoa16_modifier_icon.png" );
const frantic::tstring OpenModifierDialogPython =
    _T( "python(\\\"import MayaKrakatoa; MayaKrakatoa.OpenModifierDialog();\\\");" );

const frantic::tstring OpenMagmaDialogCommandName = _T( "Open Magma Modifier Dialog" );
const frantic::tstring OpenMagmaDialogToolTip = _T( "Open Magma Modifier Dialog" );
const frantic::tstring OpenMagmaDialogIconFileName = _T( "krakatoa16_magma_icon.png" );
const frantic::tstring OpenMagmaDialogPython =
    _T( "python(\\\"import MayaKrakatoa; MayaKrakatoa.OpenMagmaDialog();\\\");" );

const frantic::tstring MayaKrakatoaInitScriptPath = _T( "KrakatoaInit.mel" );
const frantic::tstring MayaKrakatoaDeinitScriptPath = _T( "KrakatoaDeinit.mel" );

frantic::maya::plugin_manager s_pluginManager;

} // namespace

using namespace frantic::maya;

/**
 * Called on Plugin Load
 */
EXPORT MStatus initializePlugin( MObject obj ) {

    printf( "Krakatoa MY Initializing\n" );

    std::stringstream versionStream;
    versionStream << FRANTIC_MAJOR_VERSION << "." << FRANTIC_MINOR_VERSION;
    s_pluginManager.initialize( obj, _T( "Thinkbox Software" ), frantic::strings::to_tstring( versionStream.str() ),
                                _T( "1.0" ) );
    MStatus status;

    try {
        // initialize maya magma functionality
        frantic::magma::maya::initialize( s_pluginManager );

        status =
            s_pluginManager.register_command( KrakatoaVersionCommand::commandName, KrakatoaVersionCommand::creator );
        CHECK_MSTATUS_AND_RETURN_IT( status );
        status = s_pluginManager.register_command( prt_exporter::commandName, prt_exporter::creator );
        CHECK_MSTATUS_AND_RETURN_IT( status );
        status = s_pluginManager.register_shape( PRTLoader::typeName, PRTLoader::typeId, PRTLoader::creator,
                                                 PRTLoader::initialize, PRTObjectUI::creator,
                                                 &PRTObjectUI::drawClassification );
        CHECK_MSTATUS_AND_RETURN_IT( status );
        status = s_pluginManager.register_node( KrakatoaRenderSettingsNode::typeName,
                                                KrakatoaRenderSettingsNode::typeId, KrakatoaRenderSettingsNode::creator,
                                                KrakatoaRenderSettingsNode::initialize, MPxNode::kDependNode );
        CHECK_MSTATUS_AND_RETURN_IT( status );
        status = s_pluginManager.register_node( PRTSaver::typeName, PRTSaver::typeId, PRTSaver::creator,
                                                PRTSaver::initialize, MPxNode::kDependNode );
        CHECK_MSTATUS_AND_RETURN_IT( status );
        status = s_pluginManager.register_shape( PRTFractal::typeName, PRTFractal::typeId, PRTFractal::creator,
                                                 PRTFractal::initialize, PRTObjectUI::creator,
                                                 &PRTObjectUI::drawClassification );
        CHECK_MSTATUS_AND_RETURN_IT( status );
        status = s_pluginManager.register_shape( PRTVolume::typeName, PRTVolume::typeId, PRTVolume::creator,
                                                 PRTVolume::initialize, PRTObjectUI::creator,
                                                 &PRTObjectUI::drawClassification );
        CHECK_MSTATUS_AND_RETURN_IT( status );
        status = s_pluginManager.register_shape( PRTSurface::typeName, PRTSurface::typeId, PRTSurface::creator,
                                                 PRTSurface::initialize, PRTObjectUI::creator,
                                                 &PRTObjectUI::drawClassification );
        CHECK_MSTATUS_AND_RETURN_IT( status );
        status = s_pluginManager.register_node( PRTModifiers::typeName, PRTModifiers::id, PRTModifiers::creator,
                                                PRTModifiers::initialize, MPxNode::kDependNode );
        CHECK_MSTATUS_AND_RETURN_IT( status );

        status = s_pluginManager.register_command( KrakatoaRender::commandName, KrakatoaRender::creator );
        CHECK_MSTATUS_AND_RETURN_IT( status );
        status = s_pluginManager.register_command( KrakatoaBatchRender::commandName, KrakatoaBatchRender::creator );
        CHECK_MSTATUS_AND_RETURN_IT( status );
        status =
            s_pluginManager.register_command( KrakatoaPluginDirectory::commandName, KrakatoaPluginDirectory::creator );
        CHECK_MSTATUS_AND_RETURN_IT( status );
        KrakatoaPluginDirectory::setPluginPath( s_pluginManager.get_plugin_path().c_str() );
        status = s_pluginManager.register_command( KrakatoaSettingsNodeName::commandName,
                                                   KrakatoaSettingsNodeName::creator );
        CHECK_MSTATUS_AND_RETURN_IT( status );
        status = s_pluginManager.register_command( KrakatoaLocalBatchRenderOptionsString::commandName,
                                                   KrakatoaLocalBatchRenderOptionsString::creator );
        CHECK_MSTATUS_AND_RETURN_IT( status );
        status = s_pluginManager.register_command( KrakatoaLocalBatchRender::commandName,
                                                   KrakatoaLocalBatchRender::creator );
        CHECK_MSTATUS_AND_RETURN_IT( status );
        status = s_pluginManager.register_command( RandomizeFractals::commandName, RandomizeFractals::creator );
        CHECK_MSTATUS_AND_RETURN_IT( status );
        status = s_pluginManager.register_command( krakatoaParticleJitter::commandName, krakatoaParticleJitter::creator,
                                                   krakatoaParticleJitter::newSyntax );
        CHECK_MSTATUS_AND_RETURN_IT( status );
        status = s_pluginManager.register_command( AddFractalTransformKeyframes::commandName,
                                                   AddFractalTransformKeyframes::creator );
        CHECK_MSTATUS_AND_RETURN_IT( status );
        status = s_pluginManager.register_mel_script_files( MayaKrakatoaInitScriptPath, MayaKrakatoaDeinitScriptPath,
                                                            _T("Maya Krakatoa Initialization Scripts") );
        CHECK_MSTATUS_AND_RETURN_IT( status );
        status =
            s_pluginManager.register_command( GenerateStickyChannels::commandName, GenerateStickyChannels::creator );
        CHECK_MSTATUS_AND_RETURN_IT( status );

        // for viewport 2.0 only.
#if MAYA_API_VERSION >= 201400
        status = s_pluginManager.register_geometry_override_creator(
            PRTObjectUI::drawClassification, PRTFractal::drawRegistrantId, PRTObjectUIGeometryOverride::create );
        CHECK_MSTATUS_AND_RETURN_IT( status );

        status = s_pluginManager.register_geometry_override_creator(
            PRTObjectUI::drawClassification, PRTLoader::drawRegistrantId, PRTObjectUIGeometryOverride::create );
        CHECK_MSTATUS_AND_RETURN_IT( status );

        status = s_pluginManager.register_geometry_override_creator(
            PRTObjectUI::drawClassification, PRTVolume::drawRegistrantId, PRTObjectUIGeometryOverride::create );
        CHECK_MSTATUS_AND_RETURN_IT( status );

        status = s_pluginManager.register_geometry_override_creator(
            PRTObjectUI::drawClassification, PRTSurface::drawRegistrantId, PRTObjectUIGeometryOverride::create );
        CHECK_MSTATUS_AND_RETURN_IT( status );

        // Update Viewport 2.0 hack for OS X.
        status = s_pluginManager.register_command( ForceViewport20Update::commandName, ForceViewport20Update::creator );
        CHECK_MSTATUS_AND_RETURN_IT( status );

#endif

        // This is just to keep focus on the current tab, since by default it will focus on the newly created tab
        frantic::tstring previousShelf = get_current_shelf();

        // either create a new shelf under this name if it doesn't already exist, or clear the contents of the shelf
        // out, and re-populate it with fresh data
        if( !shelf_exists( KrakatoaShelfName ) ) {
            create_shelf( KrakatoaShelfName );
        } else {
            clear_shelf( KrakatoaShelfName );
        }

        frantic::tstring pluginPath = frantic::strings::to_tstring( s_pluginManager.get_plugin_path() );

        // create PRTLoader command button
        create_shelf_button( KrakatoaShelfName, CreatePRTLoaderCommandName, CreatePRTLoaderPython,
                             CreatePRTLoaderToolTip, CreatePRTLoaderIconFileName );
        // create PRTVolume command button
        create_shelf_button( KrakatoaShelfName, CreatePRTVolumeCommandName, CreatePRTVolumePython,
                             CreatePRTVolumeToolTip, CreatePRTVolumeIconFileName );
        // create PRTSurface command button
        create_shelf_button( KrakatoaShelfName, CreatePRTSurfaceCommandName, CreatePRTSurfacePython,
                             CreatePRTSurfaceToolTip, CreatePRTSurfaceIconFileName );
        // create PRTFractal command button
        create_shelf_button( KrakatoaShelfName, CreatePRTFractalCommandName, CreatePRTFractalPython,
                             CreatePRTFractalToolTip, CreatePRTFractalIconFileName );
        // create PRTSaver command button
        create_shelf_button( KrakatoaShelfName, OpenPRTSaverCommandName, OpenPRTSaverPython, OpenPRTSaverToolTip,
                             OpenPRTSaverIconFileName );
        // create Modifier Dialog command button
        create_shelf_button( KrakatoaShelfName, OpenModifierDialogCommandName, OpenModifierDialogPython,
                             OpenModifierDialogToolTip, OpenModifierDialogIconFileName );
        // create Magma Dialog command button
        create_shelf_button( KrakatoaShelfName, OpenMagmaDialogCommandName, OpenMagmaDialogPython,
                             OpenMagmaDialogToolTip, OpenMagmaDialogIconFileName );

        // return to the original shelf
        switch_to_shelf( previousShelf );

    } catch( std::exception& e ) {
        // if we run into any exceptions while trying to load the plugins, just rollback everything done so far
        s_pluginManager.unregister_all();
        status.perror( e.what() );
        status = MStatus::kFailure;
    } catch( ... ) {
        s_pluginManager.unregister_all();
        status.perror( "Krakatoa MY: Unknown exception thrown during initialization." );
        status = MStatus::kFailure;
    }

    return status;
}

/**
 * Called on Plugin UnLoad
 */
EXPORT MStatus uninitializePlugin( MObject obj ) {

    s_pluginManager.unregister_all();
    return MStatus::kSuccess;
}
