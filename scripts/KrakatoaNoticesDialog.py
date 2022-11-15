# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0
import maya.cmds as cmds
import os

kmyNoticesWindowId = 'kmyNoticesDlg'
kmyNoticesText = None
kmyNoticesError = "Could not read third_party_licenses.txt"

def open_kmy_notices_dialog():
    global kmyNoticesWindowId
    global kmyNoticesText
    global kmyNoticesError

    if cmds.window( kmyNoticesWindowId, exists=True ):
        cmds.deleteUI( kmyNoticesWindowId )

    if kmyNoticesText == None or kmyNoticesText == kmyNoticesError :
        noticesPath = os.path.join( os.path.dirname( __file__ ), '..', '..', 'Legal', 'third_party_licenses.txt' )
        try:
            with open( noticesPath, 'r' ) as theFile:
                kmyNoticesText = theFile.read()
        except IOError:
            kmyNoticesText = kmyNoticesError

    cmds.window( kmyNoticesWindowId, title="Krakatoa MY Notices", width=516 )

    cmds.formLayout( "resizeForm" )

    sf = cmds.scrollField( wordWrap=True, text=kmyNoticesText, editable=False )

    cmds.formLayout( "resizeForm", edit=True, attachForm=[( sf, 'top', 0 ), ( sf, 'right', 0 ), ( sf, 'left', 0 ), ( sf, 'bottom', 0 )] )

    cmds.setParent( '..' )

    cmds.showWindow()