// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include "KrakatoaSettings.hpp"

const MString KrakatoaSettingsNodeName::commandName = "KrakatoaSettingsNodeName";
const MString KrakatoaSettingsNodeName::nodeName = "MayaKrakatoaRenderSettings";
void* KrakatoaSettingsNodeName::creator() { return NULL; }

MStatus KrakatoaSettingsNodeName::doIt( const MArgList& args ) { return MStatus::kFailure; }
