// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include "KrakatoaPluginDirectory.hpp"

#include <maya/M3dView.h>
#include <maya/MFileObject.h>
#include <maya/MRenderView.h>

#include <frantic/files/paths.hpp>
#include <string>

const MString KrakatoaPluginDirectory::commandName = "KrakatoaPluginDirectory";
MString KrakatoaPluginDirectory::s_pluginPath;

void* KrakatoaPluginDirectory::creator() { return new KrakatoaPluginDirectory; }

void KrakatoaPluginDirectory::setPluginPath( const MString& path ) { s_pluginPath = path; }

const MString KrakatoaPluginDirectory::getPluginPath() { return s_pluginPath; }

MStatus KrakatoaPluginDirectory::doIt( const MArgList& args ) {
    setResult( s_pluginPath );
    return MStatus::kSuccess;
}
