// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include "PRTSaver.hpp"

MTypeId PRTSaver::typeId( 0x00117499 );
MString PRTSaver::typeName = "ksr_exporter_ui_settings_node";

PRTSaver::PRTSaver( void ) {}

PRTSaver::~PRTSaver( void ) {}

void* PRTSaver::creator() { return new PRTSaver; }
MStatus PRTSaver::initialize() { return MStatus::kSuccess; }
