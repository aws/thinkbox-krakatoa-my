# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0
#
# MayaKrakatoa.py
#
# This is a utility file that should house any common script code required by MayaKrakatoa, since
# its much easier to share code as a python module than as a mel source file.  Eventually, we should
# try migrating more of our code over to python to avoid MEL's awkwardness
#
from __future__ import print_function

import maya.cmds
import maya.mel
import re

MAYA_KRAKATOA_NAME = 'MayaKrakatoa'

# Useful internal method to loaded the plugin
def _LoadMayaKrakatoa(app_name):
	if not maya.cmds.pluginInfo(app_name, query=True, loaded=True ):
		results = maya.cmds.loadPlugin(app_name)
		print('_LoadMayaKrakatoa %s' % results)
		if results != None and len(results) == 0:
			raise Exception('Could not load %s, cannot proceed.' % app_name)

def _AutoLoadMayaKrakatoa():
	if not maya.cmds.pluginInfo( MAYA_KRAKATOA_NAME, query=True, loaded=True ):
		result = maya.cmds.confirmDialog(title='Warning',
						message='MayaKrakatoa plugin is not loaded. Please go to Window -> Setting/Preferences -> Plug-in Manager to load MayaKrakatoa',
						button=['Ok'], defaultButton='Ok', cancelButton='Ok', dismissString='Ok')
#
# Methods related to handling partition files
#
_currentPartitionFileGroup = 'current'
_totalPartitionFileGroup = 'total'
_partitionFileRegex = "_part(?P<" + _currentPartitionFileGroup + ">[0-9]+)of(?P<" + _totalPartitionFileGroup + ">[0-9]+)_"

def IsPartitionFile( fileName ):
	if re.search( _partitionFileRegex, fileName ):
		return True
	else:
		return False

def GetPartitionFileInfo( fileName ):
	m = re.search( _partitionFileRegex, fileName )
	if m:
		return ( int(m.group(_currentPartitionFileGroup)), int(m.group(_totalPartitionFileGroup)) )
	else:
		return ()

def ReplacePartitionNumber(fileName, paritionIdx):
	current, total = GetPartitionFileInfo(fileName)
	numDigits = len(str(total))
	return re.sub( _partitionFileRegex, '_part' + str(paritionIdx).zfill(numDigits) + 'of' + str(total) + '_', fileName )

def WildCardPartitionNumber(fileName):
	current, total = GetPartitionFileInfo(fileName)
	return re.sub( _partitionFileRegex, '_part*of' + str(total) + '_', fileName )

#
# Methods related to creating Maya Krakatoa nodes
# Eventually, we may want to use UI contexts to allow the user to place them in the scene using the mouse
#
def _CreateRelatedTransform(baseName):
	transformNode = maya.cmds.createNode( 'transform', name=baseName+'Transform#' )
	matches = re.search( '[0-9]+$', transformNode )
	idx = int(matches.group( 0 ))
	return ( transformNode, idx )

def CreatePRTVolumeNode():
	_AutoLoadMayaKrakatoa()
	if maya.cmds.pluginInfo( MAYA_KRAKATOA_NAME, query=True, loaded=True ):
		sel = maya.cmds.ls(sl=True)
		transformNode, idx = _CreateRelatedTransform( 'PRTVolume' )
		prtVolumeNode = maya.cmds.createNode( 'PRTVolume', name='PRTVolume'+str(idx), parent=transformNode )
		maya.cmds.connectAttr( prtVolumeNode + '.worldMatrix' , prtVolumeNode + '.currXForm' )
		for node in sel:
			typ = maya.cmds.nodeType( node )
			if typ == 'mesh' or typ == 'transform':
				mystr = ('PRTVolumeAutoAttachSelectedMesh("'+ node + '","' + prtVolumeNode + '");')
				maya.mel.eval(mystr)
				break
		return prtVolumeNode
	return None

def CreatePRTSurfaceNode():
	_AutoLoadMayaKrakatoa()
	if maya.cmds.pluginInfo( MAYA_KRAKATOA_NAME, query=True, loaded=True ):
		sel = maya.cmds.ls(sl=True)
		transformNode, idx = _CreateRelatedTransform( 'PRTSurface' )
		prtSurfaceNode = maya.cmds.createNode( 'PRTSurface', name='PRTSurface'+str(idx), parent=transformNode )
		maya.cmds.connectAttr( prtSurfaceNode + '.worldMatrix' , prtSurfaceNode + '.currXForm' )
		for node in sel:
			typ = maya.cmds.nodeType( node )
			if typ == 'mesh' or typ == 'transform':
				mystr = ('PRTSurfaceAutoAttachSelectedMesh("'+ node + '","' + prtSurfaceNode + '");')
				maya.mel.eval(mystr)
				break
		return prtSurfaceNode
	return None

def CreatePRTLoaderNode():
	_AutoLoadMayaKrakatoa()
	if maya.cmds.pluginInfo( MAYA_KRAKATOA_NAME, query=True, loaded=True ):
		transformNode, idx = _CreateRelatedTransform( 'PRTLoader' )
		prtLoaderNode = maya.cmds.createNode( 'PRTLoader', name='PRTLoader'+str(idx), parent=transformNode )
		maya.cmds.connectAttr( 'time1.outTime', prtLoaderNode + '.inTime' )
		return prtLoaderNode
	return None

def CreatePRTFractalNode():
	_AutoLoadMayaKrakatoa()
	if maya.cmds.pluginInfo( MAYA_KRAKATOA_NAME, query=True, loaded=True ):
		transformNode, idx = _CreateRelatedTransform( 'PRTFractal' )
		prtFractalNode = maya.cmds.createNode( 'PRTFractal', name='PRTFractal'+str(idx), parent=transformNode )
		return prtFractalNode
	return None

def OpenPRTSaver():
	_AutoLoadMayaKrakatoa()
	if maya.cmds.pluginInfo( MAYA_KRAKATOA_NAME, query=True, loaded=True ):
		maya.mel.eval('displayPRTExporterDialog()')
		return ()
	return None

def OpenModifierDialog():
	_AutoLoadMayaKrakatoa()
	if maya.cmds.pluginInfo( MAYA_KRAKATOA_NAME, query=True, loaded=True ):
		maya.mel.eval('KMYMODED_displayKMYModEditorDialog()')
		return ()
	return None

def OpenMagmaDialog():
	_AutoLoadMayaKrakatoa()
	if maya.cmds.pluginInfo( MAYA_KRAKATOA_NAME, query=True, loaded=True ):
		maya.mel.eval('OpenKrakatoaMagmaEditor()')
		return ()
	return None

def LoadMayaKrakatoa():
	_AutoLoadMayaKrakatoa()
	if maya.cmds.pluginInfo( MAYA_KRAKATOA_NAME, query=True, loaded=True ):
		return ()
	return None
