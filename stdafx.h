// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include <cmath>

#ifdef WIN32

// Including SDKDDKVer.h defines the highest available Windows platform.
// If you wish to build your application for a previous Windows platform, include WinSDKVer.h and
// set the _WIN32_WINNT macro to the platform you wish to support before including SDKDDKVer.h.
//#include <SDKDDKVer.h>
// Specify a requirement for Win2K. See http://msdn.microsoft.com/en-us/library/aa383745(VS.85).aspx
#define _WIN32_WINNT 0x0501
#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#ifndef NOMINMAX
#define NOMINMAX
#endif

// Windows Header Files:
#include <windows.h>

#endif

// Common Maya headers
#include <maya/M3dView.h>
#include <maya/MAnimControl.h>
#include <maya/MCommonRenderSettingsData.h>
#include <maya/MFnCamera.h>
#include <maya/MFnCompoundAttribute.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnDoubleArrayData.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnMatrixData.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnStringData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnUnitAttribute.h>
#include <maya/MFnVectorArrayData.h>
#include <maya/MGlobal.h>
#include <maya/MItDag.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MRenderUtil.h>
#include <maya/MRenderView.h>
#include <maya/MSelectionList.h>
#include <maya/MString.h>
