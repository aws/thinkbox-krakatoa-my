# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0
"""
    Sets up a UI for the birth channel generator, allowing for all command line options to be specified via a
    simple interface.
"""
import maya.cmds as cmds
import maya.mel
import subprocess
import os
import sys
from functools import partial

class birth_channel_generator:

    def __init__( self ):
        #Variables for storing the GUI items.
        self.UI_WINDOW = None
        self.FILE_OUTPUT_TEXT = None
        self.FILE_OUTPUT_TEXT = None
        self.CHANNEL_INPUT_MENU = None
        self.OUTPUT_CHANNEL_TEXT = None
        self.ID_CHANNEL_MENU = None
        self.BASE_FRAME_FIELD = None
        self.OVERWRITE_CHANNEL_TOGGLE = None
        self.OVERWRITE_FILE_TOGGLE = None
        self.IGNORE_ERRORS_TOGGLE = None

        # Flags for generating the channel.
        self.inputFile = None
        self.outputFile = None
        self.inputChannel = 'Position'
        self.outputChannel = 'BirthPosition'
        self.idChannel = 'ID'
        self.overwriteChannel = True
        self.overwriteFile = True
        self.baseFrame = 0
        self.ignoreErrors = False

        # Miscellaneous
        self.file_input_dialog = None
        self.file_output_dialog = None
        self.winID = 'Birth Channel Generator'
        self.width = 602
        self.height = 307

        # Setup the birth channel generator filename and path.
        birthChannelPath = os.path.join( os.path.dirname( __file__ ), '..', '..', 'Utilities' )
        # 'nt' is the value of os.name when the os is a windows system.
        if os.name == 'nt':
            self.birthChannelGeneratorFilename = os.path.join( birthChannelPath, 'BirthChannelGenerator.exe' )
        else:
            self.birthChannelGeneratorFilename = os.path.join( birthChannelPath, 'KrakatoaBirthChannelGen' )

    """
        Setup the window for the UI
    """
    def setup_window( self ):

        # Ensure that the window isn't already active.
        if cmds.window( self.winID, exists=True ):
            cmds.deleteUI( self.UI_WINDOW, window=True )

        # Set up the window.
        self.UI_WINDOW = cmds.window( self.winID, title=self.winID, widthHeight=( self.width, self.height ), sizeable=False )
        cmds.columnLayout( rowSpacing=10 )

        # File Sequences
        cmds.frameLayout( label='File Sequences', width=600 )

        # Input file
        cmds.rowColumnLayout( numberOfColumns=3, columnSpacing=[( 2, 5 ), ( 3, 5 )] )
        cmds.text( 'Input File: ', width=100, align='right' )
        self.FILE_INPUT_TEXT = cmds.textField( "InputFileText", placeholderText='Select an input file...', width=300 )
        cmds.button( label='Browse', command=partial( self.browse_input_files ) )
        cmds.setParent( '..' )

        # Output file
        cmds.rowColumnLayout( numberOfColumns=4, columnSpacing=[( 2, 5 ), ( 3, 5 ), ( 4, 5 )] )
        cmds.text( 'Output File: ', width=100, align='right' )
        self.FILE_OUTPUT_TEXT = cmds.textField( "OutputFileText", placeholderText='Select an output file...', width=300 )
        cmds.button( label="Browse", command=partial( self.browse_output_files ) )
        self.OVERWRITE_FILE_TOGGLE = cmds.checkBox( label='Overwrite Existing File', value=True )
        cmds.setParent( '..' )

        cmds.setParent( '..' ) # end file sequences

        # Channels
        cmds.frameLayout( label='Channels', width=600 )

        # Input channel
        cmds.rowColumnLayout( numberOfRows=1, columnSpacing=( 2, 5 ) )
        cmds.text( 'Input Channel: ', width=100, align='right' )
        self.CHANNEL_INPUT_MENU = cmds.optionMenuGrp( width=300 )
        cmds.menuItem( "Position", label='Position' )
        cmds.checkBox( label='Auto-Suggest Output Channel', value=True, onCommand=partial( self.auto_suggest_on ) )
        cmds.setParent( '..' )

        # Output channel
        cmds.rowColumnLayout( numberOfRows=1, columnSpacing=[( 2, 5 ), ( 3, 5 )] )
        cmds.text( 'Output Channel:', width=100, align='right' )
        self.OUTPUT_CHANNEL_TEXT = cmds.textField( "OutputChannelText", text='BirthPosition', width=300 )
        self.OVERWRITE_CHANNEL_TOGGLE = cmds.checkBox( label='Overwrite Existing Channel', value=True )
        cmds.setParent( '..' )

        cmds.setParent( '..' ) # end channels

        # Options
        cmds.frameLayout( label='Options', width=600 )
        
        # Options Row 1
        cmds.rowColumnLayout( numberOfRows=1, columnSpacing=[( 2, 5 ), ( 3, 5 )] )
        cmds.text( 'ID Channel: ', width=100, align='right' )
        self.ID_CHANNEL_MENU = cmds.optionMenuGrp( width=300 )
        cmds.menuItem( "ID", label='ID' )
        self.IGNORE_ERRORS_TOGGLE = cmds.checkBox( label='Ignore Per-Frame Errors', value=False )
        cmds.setParent( '..' )

        # Options Row 2
        cmds.rowColumnLayout( numberOfRows=1, columnSpacing=[( 2, 5 ), ( 3, 5 )] )
        cmds.text( 'Base Frame: ', width=100, align='right' )
        self.BASE_FRAME_FIELD = cmds.intField( width=300, value=0 )
        cmds.setParent( '..' )

        cmds.setParent( '..' ) # end options

        cmds.button( label='GENERATE BIRTH CHANNEL', width=600, height=50, command=partial( self.generate_birth_channels )  )

        cmds.setParent( '..' ) # end column layout

    def show_window( self ):
        cmds.showWindow()
        cmds.window( self.UI_WINDOW, edit=True, widthHeight=( self.width, self.height ) )
        cmds.setFocus( self.OVERWRITE_FILE_TOGGLE )

    # Allows for the user to browse for a specific input file.
    def browse_input_files( self, arg=None ):
        self.file_input_dialog = cmds.fileDialog2( caption='Choose input file:', dialogStyle=2, okCaption='Accept', fileMode=1 )
        if( self.file_input_dialog ):
            cmds.textField( "InputFileText", edit=True, placeholderText='Select an input file...', text=self.file_input_dialog[0], width=300 )

    # Allows for the user to browse for a specific output file.
    def browse_output_files( self, arg=None ):
        self.file_output_dialog = cmds.fileDialog2( caption='Choose output file:', dialogStyle=2, okCaption='Accept', fileMode=0 )
        if( self.file_output_dialog ):
            cmds.textField( "OutputFileText", edit=True, placeholderText='Select an output file...', text=self.file_output_dialog[0], width=300 )

    # Set the the Output Channel to BirthPosition.
    def auto_suggest_on( self, arg=None ):
        cmds.textField( "OutputChannelText", edit=True, text='BirthPosition', width=300 )

    """
        Set up all the options to be passed to the Birth Channel Generator.
        Strings are encoded to utf8 from unicode, all values are taken from the corresponding
        options on the UI.
    """
    def set_options( self ):
        self.inputFile = cmds.textField( self.FILE_INPUT_TEXT, query=True, text=True ).encode( 'utf8' )
        self.outputFile = cmds.textField( self.FILE_OUTPUT_TEXT, query=True, text=True ).encode( 'utf8' )
        self.outputChannel = cmds.textField( self.OUTPUT_CHANNEL_TEXT, query=True, text=True ).encode( 'utf8' )
        self.baseFrame = cmds.intField( self.BASE_FRAME_FIELD, query=True, value=True )
        self.inputChannel = cmds.optionMenuGrp( self.CHANNEL_INPUT_MENU, query=True, value=True ).encode( 'utf8' )
        self.idChannel = cmds.optionMenuGrp( self.ID_CHANNEL_MENU, query=True, value=True ).encode( 'utf8' )
        self.overwriteChannel = cmds.checkBox( self.OVERWRITE_CHANNEL_TOGGLE, query=True, value=True )
        self.overwriteFile = cmds.checkBox( self.OVERWRITE_FILE_TOGGLE, query=True, value=True )
        self.ignoreErrors = cmds.checkBox( self.IGNORE_ERRORS_TOGGLE, query=True, value=True )

    """
        Ensure that the given output file is in a path that exists.
        If the path to the output file does not exist, no output is generated from
        the executable.

        Paths that are given and not absolute are assumed relative to the input file's location.
        If only a filename is provided, the path is assumed to be "[input file's location]/out"

        The user is prompted to create a directory if the output file path does not already exist.
    """
    def set_output_path( self ):
        # Setup the output file path if the output file path is not given as absolute.
        if not os.path.isabs( self.outputFile ):
            self.inputFile = self.inputFile.replace( b'\\', b'/' )
            inputFilePathList = self.inputFile.rsplit( b'/', 1 )
            inputFilePathList.pop()

            if len( inputFilePathList ) > 0:
                outputFilePath = inputFilePathList.pop()
            else:
                outputFilePath = b''

            self.outputFile = self.outputFile.replace( b'\\', b'/' )
            outputFilePathList = self.outputFile.rsplit( b'/', 1 )
            outputFileName = outputFilePathList.pop()
            
            # If the specified output file was given with a directory location, assume it is relative to the input
            # directory, if not, set it to go to [input directory]/out
            if len( outputFilePathList ) > 0:
                outputFilePath = os.path.join( outputFilePath, outputFilePathList.pop() )
            else:
                outputFilePath = os.path.join( outputFilePath, 'out' )

            if self.create_output_directory( outputFilePath ) != 0:
                return 1
            self.outputFile = os.path.join( outputFilePath, outputFileName )

        # Setup the file path if the path is absolute, but the file doesn't already exist.
        elif not os.path.isfile( self.outputFile ):
            self.outputFile = self.outputFile.replace( b'\\', b'/' )
            outputFilePathList = self.outputFile.rsplit( b'/', 1 )
            outputFileName = outputFilePathList.pop()

            if len( outputFilePathList ) > 0:
                outputFilePath = outputFilePathList.pop()

            if self.create_output_directory( outputFilePath ) != 0:
                return 1
        return 0

    """
        If the path doesn't exist it must be created, the user has the option to click 'no' and specify a new output location.
    """
    def create_output_directory( self, outputFilePath ):
        if not os.path.exists( outputFilePath ):
            outputFilePath = outputFilePath.replace( b'\\', b'/' )
            outFileMessage = b'The output directory: \"' + outputFilePath + b'\" does not exist, would you like to create it now?'
            makeDirectory = cmds.confirmDialog( title='Create Directory', message=outFileMessage, button=['Yes','No'], defaultButton='Yes', cancelButton='No', dismissString='No', icon='question' )
            if makeDirectory == 'No':
                return 1
            try:
                os.makedirs( outputFilePath )
            except Exception as error:
                cmds.confirmDialog( title='', message='The directory could not be created. Please manually create or specify a new location.', button=['Close'], icon='critical' )
                return 1
        return 0

    """ 
        Ensure that the output file has an extension, otherwise it will just create a regular file (no extension).
        To work around this, if there is no file extension, the input file's extension will be used.
    """
    def check_output_file_extension( self ):
        if len( self.outputFile.rsplit( b'.', 1 ) ) == 1:
            inputFileCheck = self.inputFile.rsplit( b'.', 1 )
            inputFileExtension = inputFileCheck.pop()
            # Use the extension from the input file. 
            self.outputFile = self.outputFile + b'.' + inputFileExtension
  
    """
        If the files provided are not valid, the function returns 1, birth channel generation is cancelled
        and the UI is left open for adjustment.
    """
    def check_files( self ):
        # Check to see if the input file exists.
        if self.inputFile == '':
            cmds.confirmDialog( title='Input File Error', message='Please enter the name of the desired input file.', button=['Close'], defaultButton='Close', icon='critical' )
            return 1

        elif not os.path.isfile( self.inputFile ):
            cmds.confirmDialog( title='Input File Error', message=b'The input file \"' + self.inputFile + b'\" could not be opened.', button=['Close'], defaultButton='Close', icon='critical' )
            return 1

        # Check to see if the output file text box was editted.
        if self.outputFile == '':
            cmds.confirmDialog( title='Output File Error', message='Please enter the name of the desired output file.', button=['Close'], defaultButton='Close', icon='critical' )
            return 1

        if self.set_output_path() != 0:
            return 1

        self.check_output_file_extension()
        return 0


    """
        Check to ensure that an Output Channel will be specified.
    """
    def check_output_channel( self ):
        if self.outputChannel == '':
            useDefaultChannel = cmds.confirmDialog( title='Output Channel Not Specified', message='The output channel has not been specified. Would you like to use the default value (BirthPosition)?',
                                button=['Yes', 'No'], defaultButton='Yes', cancelButton='No', icon='question', dismissString='No' )
            if useDefaultChannel == 'No':
                return 1
            self.outputChannel = 'BirthPosition'
        return 0


    """
        Ensure that all the data is valid and then call the birth channel generator.
    """
    def generate_birth_channels( self, arg=None ):        
        self.set_options()
        if self.check_files() != 0:
            return 
        if self.check_output_channel() != 0:
            return
        
        try:
            cmds.GenerateStickyChannels( self.inputFile, self.outputFile, self.inputChannel, self.outputChannel, self.idChannel, self.baseFrame , self.ignoreErrors, self.overwriteChannel, self.overwriteFile )
            cmds.deleteUI( self.UI_WINDOW, window=True )
        except Exception as error:
            cmds.confirmDialog( title='Birth Channel Generator Error', message=error.output, button=['OK'], defaultButton='OK', icon='critical' )


"""
    Run the Birth Channel Generator, called when the shelf icon is clicked in Maya.
"""
def OpenBirthChannelGeneratorDialog(): 
    birth_generator = birth_channel_generator()
    birth_generator.setup_window()
    birth_generator.show_window()
