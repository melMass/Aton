__author__ = "Vahan Sosoyan, Dan Bradham"
__copyright__ = "2016 All rights reserved. See Copyright.txt for more details."
__version__ = "v1.1.6b"

import sys
from timeit import default_timer

import maya.mel as mel
import maya.OpenMaya as OM
import pymel.core as pm
from maya import cmds, OpenMayaUI

try:
    from arnold import *
    import mtoa.core as core
except ImportError:
    cmds.warning("MtoA was not found.")

# Check the Maya version for PySide
mayaVersion = cmds.about(apiVersion=True)
if mayaVersion >= 201700:
    from PySide2 import QtCore, QtWidgets
    from shiboken2 import wrapInstance
else:
    from PySide import QtCore
    from PySide import QtGui as QtWidgets
    from shiboken import wrapInstance

def maya_main_window():
    main_window_ptr = OpenMayaUI.MQtUtil.mainWindow()
    return wrapInstance(long(main_window_ptr), QtWidgets.QWidget)

class Aton(QtWidgets.QDialog):

    def __init__(self, parent = maya_main_window()):
        super(Aton, self).__init__(parent)

        self.windowName = "Aton"
        if cmds.window(self.windowName, exists = True):
            cmds.deleteUI(self.windowName, wnd = True)

        self.timeChangedCB = None
        self.selectionChangedCB = None
        self.frame_sequence = AiFrameSequence()
        self.frame_sequence.started.connect(self.sequence_started)
        self.frame_sequence.stopped.connect(self.sequence_stopped)
        self.frame_sequence.stepped.connect(self.sequence_stepped)
        self.defaultPort = self.getSceneOption(0)
        self.defaultRefinement = self.getSceneOption(12)
        self.setupUi()

    def getActiveCamera(self):
        ''' Returns active camera shape name '''
        cam = cmds.modelEditor(cmds.playblast(ae=1), q=1, cam=1)
        if cmds.listRelatives(cam) != None:
            cam = cmds.listRelatives(cam)[0]
        return cam

    def getPort(self):
        ''' Returns the port number from Aton driver '''
        port = 0
        try: # To init Arnold Render settings
            port = cmds.getAttr("defaultArnoldDisplayDriver.port")
        except ValueError:
            mel.eval("unifiedRenderGlobalsWindow;")
            try: # If aton driver is not loaded
                port = cmds.getAttr("defaultArnoldDisplayDriver.port")
            except ValueError:
                pass
        return port

    def getSceneOption(self, attr):
        ''' Returns requested scene options attribute value '''
        result = 0
        if cmds.getAttr("defaultRenderGlobals.ren") == "arnold":
            result = {0 : lambda: self.getPort(),
                      1 : lambda: self.getActiveCamera(),
                      2 : lambda: cmds.getAttr("defaultResolution.width"),
                      3 : lambda: cmds.getAttr("defaultResolution.height"),
                      4 : lambda: cmds.getAttr("defaultArnoldRenderOptions.AASamples"),
                      5 : lambda: cmds.getAttr("defaultArnoldRenderOptions.ignoreMotionBlur"),
                      6 : lambda: cmds.getAttr("defaultArnoldRenderOptions.ignoreSubdivision"),
                      7 : lambda: cmds.getAttr("defaultArnoldRenderOptions.ignoreDisplacement"),
                      8 : lambda: cmds.getAttr("defaultArnoldRenderOptions.ignoreBump"),
                      9 : lambda: cmds.getAttr("defaultArnoldRenderOptions.ignoreSss"),
                      10 : lambda: cmds.playbackOptions(q=True, minTime=True),
                      11 : lambda: cmds.playbackOptions(q=True, maxTime=True),
                      12 : lambda: cmds.getAttr("defaultArnoldRenderOptions.progressive_rendering")}[attr]()
        return result

    def setupUi(self):
        ''' Building the GUI '''
        def resUpdateUi(value):
            self.resolutionSpinBox.setValue(value * 5)

        def resInfoUpdate(value):
            xres = self.getSceneOption(2) * value / 100
            yres = self.getSceneOption(3) * value / 100
            resolutionInfoLabel.setText("%sx%s"%(xres, yres))

        def camUpdateUi(value):
            self.cameraAaSpinBox.setValue(value)

        def portUpdateUi(value):
            self.portSpinBox.setValue(value + self.defaultPort)

        def sequence_toggled(value):
            isChecked = bool(value)
            self.startLabel.setEnabled(isChecked)
            self.endLabel.setEnabled(isChecked)
            self.stepLabel.setEnabled(isChecked)
            self.startSpinBox.setEnabled(isChecked)
            self.endSpinBox.setEnabled(isChecked)
            self.stepSpinBox.setEnabled(isChecked)

        def resetUi(*args):
            self.portSpinBox.setValue(self.defaultPort)
            portSlider.setValue(0)
            self.extraHostLineEdit.setText("")
            self.cameraComboBox.setCurrentIndex(0)
            self.resolutionSpinBox.setValue(100)
            resolutionSlider.setValue(20)
            self.cameraAaSpinBox.setValue(self.getSceneOption(4))
            cameraAaSlider.setValue(self.getSceneOption(4))
            self.renderRegionXSpinBox.setValue(0)
            self.renderRegionYSpinBox.setValue(0)
            self.renderRegionRSpinBox.setValue(self.getSceneOption(2))
            self.renderRegionTSpinBox.setValue(self.getSceneOption(3))
            overscanSlider.setValue(0)
            self.motionBlurCheckBox.setChecked(self.getSceneOption(5))
            self.subdivsCheckBox.setChecked(self.getSceneOption(6))
            self.displaceCheckBox.setChecked(self.getSceneOption(7))
            self.bumpCheckBox.setChecked(self.getSceneOption(8))
            self.sssCheckBox.setChecked(self.getSceneOption(9))
            self.shaderComboBox.setCurrentIndex(0)
            textureRepeatSlider.setValue(4)
            self.selectedShaderCheckbox.setChecked(0)
            self.startSpinBox.setValue(self.getSceneOption(10))
            self.endSpinBox.setValue(self.getSceneOption(11))
            self.stepSpinBox.setValue(1)
            self.seqCheckBox.setChecked(False)

        self.setObjectName(self.windowName)
        self.setWindowTitle("Aton %s"%__version__)
        self.setWindowFlags(QtCore.Qt.Tool)
        self.setAttribute(QtCore.Qt.WA_AlwaysShowToolTips)
        self.setAttribute(QtCore.Qt.WA_DeleteOnClose)
        self.setMinimumSize(420, 440)
        self.setMaximumSize(420, 440)

        mainLayout = QtWidgets.QVBoxLayout()
        mainLayout.setContentsMargins(5,5,5,5)
        mainLayout.setSpacing(2)

        generalGroupBox = QtWidgets.QGroupBox("General")
        generalGroupBox.setMaximumSize(420, 110)
        generalLayout = QtWidgets.QVBoxLayout(generalGroupBox)

        # Port Layout
        portLayout = QtWidgets.QHBoxLayout()
        portLabel = QtWidgets.QLabel("Port:")
        portLabel.setAlignment(QtCore.Qt.AlignRight|QtCore.Qt.AlignVCenter)
        portLabel.setMinimumSize(75, 20)
        self.portSpinBox = QtWidgets.QSpinBox()
        self.portSpinBox.setButtonSymbols(QtWidgets.QAbstractSpinBox.NoButtons)
        self.portSpinBox.setMaximum(1024)
        self.portSpinBox.setMaximum(9999)
        self.portSpinBox.setValue(self.defaultPort)
        portSlider = QtWidgets.QSlider()
        portSlider.setOrientation(QtCore.Qt.Horizontal)
        portSlider.setMinimum(0)
        portSlider.setMaximum(15)
        portSlider.setValue(0)
        portLayout.addWidget(portLabel)
        portLayout.addWidget(self.portSpinBox)
        portLayout.addWidget(portSlider)

        # Extra Host Layout
        extraHostLayout = QtWidgets.QHBoxLayout()
        extraHostLabel = QtWidgets.QLabel("Extra host:")
        extraHostLabel.setAlignment(QtCore.Qt.AlignRight|QtCore.Qt.AlignVCenter)
        extraHostLabel.setMinimumSize(75, 20)
        extraHostLabel.setMaximumSize(75, 20)
        self.extraHostLineEdit = QtWidgets.QLineEdit()
        extraHostLayout.addWidget(extraHostLabel)
        extraHostLayout.addWidget(self.extraHostLineEdit)

        # Camera Layout
        cameraLayout = QtWidgets.QHBoxLayout()
        cameraLabel = QtWidgets.QLabel("Camera:")
        cameraLabel.setAlignment(QtCore.Qt.AlignRight|QtCore.Qt.AlignVCenter)
        cameraLabel.setMaximumSize(75, 20)
        self.cameraComboBox = QtWidgets.QComboBox()
        self.cameraComboBox.setMinimumSize(0, 20)
        self.cameraComboBox.setMaximumSize(500, 20)
        self.cameraComboBoxDict = {}
        self.cameraComboBox.addItem("Current view")
        for i in cmds.listCameras():
            self.cameraComboBox.addItem(i)
            self.cameraComboBoxDict[cmds.listCameras().index(i)+1] = i
        cameraLayout.addWidget(cameraLabel)
        cameraLayout.addWidget(self.cameraComboBox)

        overridesGroupBox = QtWidgets.QGroupBox("Overrides")
        overridesGroupBox.setMaximumSize(420, 200)
        overridesLayout = QtWidgets.QVBoxLayout(overridesGroupBox)

        # Resolution Layout
        resolutionLayout = QtWidgets.QHBoxLayout()
        resolutionLabel = QtWidgets.QLabel("Resolution %:")
        resolutionLabel.setAlignment(QtCore.Qt.AlignRight|QtCore.Qt.AlignVCenter)
        resolutionLabel.setMinimumSize(75, 20)
        self.resolutionSpinBox = QtWidgets.QSpinBox()
        self.resolutionSpinBox.setButtonSymbols(QtWidgets.QAbstractSpinBox.NoButtons)
        self.resolutionSpinBox.setMinimum(1)
        self.resolutionSpinBox.setMaximum(999)
        self.resolutionSpinBox.setValue(100)
        resolutionSlider = QtWidgets.QSlider()
        resolutionSlider.setOrientation(QtCore.Qt.Horizontal)
        resolutionSlider.setValue(20)
        resolutionSlider.setMaximum(40)
        xres, yres = self.getSceneOption(2), self.getSceneOption(3)
        resolutionInfoLabel = QtWidgets.QLabel("%sx%s"%(xres, yres))
        resolutionInfoLabel.setEnabled(False)
        resolutionLayout.addWidget(resolutionLabel)
        resolutionLayout.addWidget(self.resolutionSpinBox)
        resolutionLayout.addWidget(resolutionSlider)
        resolutionLayout.addWidget(resolutionInfoLabel)

        # Camera Layout
        cameraAaLayout = QtWidgets.QHBoxLayout()
        cameraAaLabel = QtWidgets.QLabel("Camera (AA):")
        cameraAaLabel.setAlignment(QtCore.Qt.AlignRight|QtCore.Qt.AlignVCenter)
        cameraAaLabel.setMinimumSize(75, 20)
        self.cameraAaSpinBox = QtWidgets.QSpinBox()
        self.cameraAaSpinBox.setButtonSymbols(QtWidgets.QAbstractSpinBox.NoButtons)
        self.cameraAaSpinBox.setMaximum(64)
        self.cameraAaSpinBox.setMinimum(-64)
        self.cameraAaSpinBox.setValue(self.getSceneOption(4))
        cameraAaSlider = QtWidgets.QSlider()
        cameraAaSlider.setOrientation(QtCore.Qt.Horizontal)
        cameraAaSlider.setValue(self.cameraAaSpinBox.value())
        cameraAaSlider.setMinimum(-3)
        cameraAaSlider.setMaximum(16)
        cameraAaSlider.valueChanged[int].connect(self.cameraAaSpinBox.setValue)
        cameraAaLayout.addWidget(cameraAaLabel)
        cameraAaLayout.addWidget(self.cameraAaSpinBox)
        cameraAaLayout.addWidget(cameraAaSlider)

        # Render region layout
        renderRegionLayout = QtWidgets.QHBoxLayout()
        renderRegionLabel = QtWidgets.QLabel("Region X:")
        renderRegionLabel.setMinimumSize(75, 20)
        renderRegionLabel.setAlignment(QtCore.Qt.AlignRight|QtCore.Qt.AlignVCenter)
        self.renderRegionXSpinBox = QtWidgets.QSpinBox()
        renderRegionYLabel = QtWidgets.QLabel("Y:")
        self.renderRegionYSpinBox = QtWidgets.QSpinBox()
        renderRegionRLabel = QtWidgets.QLabel("R:")
        self.renderRegionRSpinBox = QtWidgets.QSpinBox()
        renderRegionTLabel = QtWidgets.QLabel("T:")
        self.renderRegionTSpinBox = QtWidgets.QSpinBox()
        renderRegionGetNukeButton = QtWidgets.QPushButton("Get")
        renderRegionGetNukeButton.setMaximumSize(35, 18)
        renderRegionGetNukeButton.clicked.connect(self.getNukeCropNode)
        renderRegionLayout.addWidget(renderRegionLabel)
        renderRegionLayout.addWidget(self.renderRegionXSpinBox)
        renderRegionLayout.addWidget(renderRegionYLabel)
        renderRegionLayout.addWidget(self.renderRegionYSpinBox)
        renderRegionLayout.addWidget(renderRegionRLabel)
        renderRegionLayout.addWidget(self.renderRegionRSpinBox)
        renderRegionLayout.addWidget(renderRegionTLabel)
        renderRegionLayout.addWidget(self.renderRegionTSpinBox)
        renderRegionLayout.addWidget(renderRegionGetNukeButton)

        for i in [renderRegionLabel,
                  renderRegionYLabel,
                  renderRegionRLabel,
                  renderRegionTLabel]:
            i.setAlignment(QtCore.Qt.AlignRight|QtCore.Qt.AlignVCenter)

        for i in [self.renderRegionXSpinBox,
                  self.renderRegionYSpinBox,
                  self.renderRegionRSpinBox,
                  self.renderRegionTSpinBox]:
            i.setRange(-99999,99999)
            i.setMaximumSize(40,20)
            i.setButtonSymbols(QtWidgets.QAbstractSpinBox.NoButtons)

        self.renderRegionRSpinBox.setValue(self.getSceneOption(2))
        self.renderRegionTSpinBox.setValue(self.getSceneOption(3))

        # Overscan Layout
        overscanLayout = QtWidgets.QHBoxLayout()
        overscanLabel = QtWidgets.QLabel("Overscan:")
        overscanLabel.setAlignment(QtCore.Qt.AlignRight|QtCore.Qt.AlignVCenter)
        overscanLabel.setMinimumSize(75, 20)
        self.overscanSpinBox = QtWidgets.QSpinBox()
        self.overscanSpinBox.setButtonSymbols(QtWidgets.QAbstractSpinBox.NoButtons)
        self.overscanSpinBox.setMinimum(0)
        self.overscanSpinBox.setMaximum(9999)
        self.overscanSpinBox.setValue(0)
        overscanSlider = QtWidgets.QSlider()
        overscanSlider.setOrientation(QtCore.Qt.Horizontal)
        overscanSlider.setValue(0)
        overscanSlider.setMaximum(250)
        overscanSlider.valueChanged[int].connect(self.overscanSpinBox.setValue)
        overscanSetButton = QtWidgets.QPushButton("Set")
        overscanSetButton.setMaximumSize(35, 18)
        overscanSetButton.clicked.connect(self.setOverscan)
        overscanLayout.addWidget(overscanLabel)
        overscanLayout.addWidget(self.overscanSpinBox)
        overscanLayout.addWidget(overscanSlider)
        overscanLayout.addWidget(overscanSetButton)

        # Shaders layout
        shaderLayout = QtWidgets.QHBoxLayout()
        shaderLabel = QtWidgets.QLabel("Shader override:")
        shaderLabel.setAlignment(QtCore.Qt.AlignRight|QtCore.Qt.AlignVCenter)
        shaderLabel.setMaximumSize(90, 20)
        self.shaderComboBox = QtWidgets.QComboBox()
        self.shaderComboBox.addItem("Disabled")
        self.shaderComboBox.addItem("Checker")
        self.shaderComboBox.addItem("Grey")
        self.shaderComboBox.addItem("Mirror")
        self.shaderComboBox.addItem("Normal")
        self.shaderComboBox.addItem("Occlusion")
        self.shaderComboBox.addItem("UV")
        self.selectedShaderCheckbox = QtWidgets.QCheckBox("Selected objects only")
        shaderLayout.addWidget(shaderLabel)
        shaderLayout.addWidget(self.shaderComboBox)
        shaderLayout.addWidget(self.selectedShaderCheckbox)

        textureRepeatLayout = QtWidgets.QHBoxLayout()
        textureRepeatLabel = QtWidgets.QLabel("Texture repeat:")
        textureRepeatLabel.setAlignment(QtCore.Qt.AlignRight|QtCore.Qt.AlignVCenter)
        textureRepeatLabel.setMinimumSize(90, 20)
        self.textureRepeatSpinbox = QtWidgets.QSpinBox()
        self.textureRepeatSpinbox.setValue(1)
        self.textureRepeatSpinbox.setButtonSymbols(QtWidgets.QAbstractSpinBox.NoButtons)
        textureRepeatSlider = QtWidgets.QSlider()
        textureRepeatSlider.setMinimum(1)
        textureRepeatSlider.setMaximum(64)
        textureRepeatSlider.setOrientation(QtCore.Qt.Horizontal)
        textureRepeatSlider.valueChanged[int].connect(self.textureRepeatSpinbox.setValue)
        textureRepeatSlider.setValue(4)
        textureRepeatLayout.addWidget(textureRepeatLabel)
        textureRepeatLayout.addWidget(self.textureRepeatSpinbox)
        textureRepeatLayout.addWidget(textureRepeatSlider)

        # Ignore Layout
        ignoresGroupBox = QtWidgets.QGroupBox("Ignore")
        ignoresGroupBox.setMaximumSize(420, 50)
        ignoresLayout = QtWidgets.QVBoxLayout(ignoresGroupBox)
        ignoreLayout = QtWidgets.QHBoxLayout()
        ignoreLabel = QtWidgets.QLabel("Ignore:")
        ignoreLabel.setAlignment(QtCore.Qt.AlignRight|QtCore.Qt.AlignVCenter)
        self.motionBlurCheckBox = QtWidgets.QCheckBox("Motion Blur")
        self.motionBlurCheckBox.setChecked(self.getSceneOption(5))
        self.subdivsCheckBox = QtWidgets.QCheckBox("Subdivs")
        self.subdivsCheckBox.setChecked(self.getSceneOption(6))
        self.displaceCheckBox = QtWidgets.QCheckBox("Displace")
        self.displaceCheckBox.setChecked(self.getSceneOption(7))
        self.bumpCheckBox = QtWidgets.QCheckBox("Bump")
        self.bumpCheckBox.setChecked(self.getSceneOption(8))
        self.sssCheckBox = QtWidgets.QCheckBox("SSS")
        self.sssCheckBox.setChecked(self.getSceneOption(9))
        ignoreLayout.addWidget(self.motionBlurCheckBox)
        ignoreLayout.addWidget(self.subdivsCheckBox)
        ignoreLayout.addWidget(self.displaceCheckBox)
        ignoreLayout.addWidget(self.bumpCheckBox)
        ignoreLayout.addWidget(self.sssCheckBox)

        # Sequence Layout
        sequenceGroupBox = QtWidgets.QGroupBox('Sequence')
        sequenceGroupBox.setMaximumSize(420, 55)
        sequenceLayout = QtWidgets.QHBoxLayout(sequenceGroupBox)
        self.seqCheckBox = QtWidgets.QCheckBox()
        self.seqCheckBox.setMaximumSize(15, 25)
        self.seqCheckBox.stateChanged.connect(sequence_toggled)
        self.startLabel = QtWidgets.QLabel('Start frame:')
        self.startLabel.setEnabled(False)
        self.startSpinBox = QtWidgets.QSpinBox()
        self.startSpinBox.setButtonSymbols(QtWidgets.QAbstractSpinBox.NoButtons)
        self.startSpinBox.setRange(0, 99999)
        self.startSpinBox.setValue(self.getSceneOption(10))
        self.startSpinBox.setEnabled(False)
        self.endLabel = QtWidgets.QLabel('End frame:')
        self.endLabel.setEnabled(False)
        self.endSpinBox = QtWidgets.QSpinBox()
        self.endSpinBox.setButtonSymbols(QtWidgets.QAbstractSpinBox.NoButtons)
        self.endSpinBox.setRange(0, 99999)
        self.endSpinBox.setValue(self.getSceneOption(11))
        self.endSpinBox.setEnabled(False)
        self.stepLabel = QtWidgets.QLabel('By frame:')
        self.stepLabel.setEnabled(False)
        self.stepSpinBox = QtWidgets.QSpinBox()
        self.stepSpinBox.setButtonSymbols(QtWidgets.QAbstractSpinBox.NoButtons)
        self.stepSpinBox.setValue(1)
        self.stepSpinBox.setRange(1, 100)
        self.stepSpinBox.setEnabled(False)
        sequenceLayout.addWidget(self.seqCheckBox)
        sequenceLayout.addWidget(self.startLabel)
        sequenceLayout.addWidget(self.startSpinBox)
        sequenceLayout.addWidget(self.endLabel)
        sequenceLayout.addWidget(self.endSpinBox)
        sequenceLayout.addWidget(self.stepLabel)
        sequenceLayout.addWidget(self.stepSpinBox)

        # Main Buttons Layout
        mainButtonslayout = QtWidgets.QHBoxLayout()
        startButton = QtWidgets.QPushButton("Start / Refresh")
        stopButton = QtWidgets.QPushButton("Stop")
        resetButton = QtWidgets.QPushButton("Reset")
        startButton.clicked.connect(self.render)
        stopButton.clicked.connect(self.stop)
        resetButton.clicked.connect(resetUi)
        mainButtonslayout.addWidget(startButton)
        mainButtonslayout.addWidget(stopButton)
        mainButtonslayout.addWidget(resetButton)

        # Add Layouts to Main
        generalLayout.addLayout(portLayout)
        generalLayout.addLayout(extraHostLayout)

        generalLayout.addLayout(cameraLayout)
        overridesLayout.addLayout(resolutionLayout)
        overridesLayout.addLayout(cameraAaLayout)
        overridesLayout.addLayout(renderRegionLayout)
        overridesLayout.addLayout(overscanLayout)
        overridesLayout.addLayout(shaderLayout)
        overridesLayout.addLayout(textureRepeatLayout)
        ignoresLayout.addLayout(ignoreLayout)

        mainLayout.addWidget(generalGroupBox)
        mainLayout.addWidget(overridesGroupBox)
        mainLayout.addWidget(ignoresGroupBox)
        mainLayout.addWidget(sequenceGroupBox)
        mainLayout.addLayout(mainButtonslayout)

        # UI Updates
        self.connect(portSlider, QtCore.SIGNAL("valueChanged(int)"), portUpdateUi)
        self.connect(resolutionSlider, QtCore.SIGNAL("valueChanged(int)"), resUpdateUi)
        self.connect(self.resolutionSpinBox, QtCore.SIGNAL("valueChanged(int)"), resInfoUpdate)

        # IPR Updates
        self.connect(self.cameraComboBox, QtCore.SIGNAL("currentIndexChanged(int)"), lambda: self.IPRUpdate(0))
        self.connect(self.resolutionSpinBox, QtCore.SIGNAL("valueChanged(int)"), lambda: self.IPRUpdate(1))
        self.connect(self.cameraAaSpinBox, QtCore.SIGNAL("valueChanged(int)"), lambda: self.IPRUpdate(2))
        self.connect(self.renderRegionXSpinBox, QtCore.SIGNAL("valueChanged(int)"), lambda: self.IPRUpdate(1))
        self.connect(self.renderRegionYSpinBox, QtCore.SIGNAL("valueChanged(int)"), lambda: self.IPRUpdate(1))
        self.connect(self.renderRegionRSpinBox, QtCore.SIGNAL("valueChanged(int)"), lambda: self.IPRUpdate(1))
        self.connect(self.renderRegionTSpinBox, QtCore.SIGNAL("valueChanged(int)"), lambda: self.IPRUpdate(1))
        self.connect(self.overscanSpinBox, QtCore.SIGNAL("valueChanged(int)"), lambda: self.IPRUpdate(1))
        self.connect(self.motionBlurCheckBox, QtCore.SIGNAL("toggled(bool)"), lambda: self.IPRUpdate(3))
        self.connect(self.subdivsCheckBox, QtCore.SIGNAL("toggled(bool)"), lambda: self.IPRUpdate(3))
        self.connect(self.displaceCheckBox, QtCore.SIGNAL("toggled(bool)"), lambda: self.IPRUpdate(3))
        self.connect(self.bumpCheckBox, QtCore.SIGNAL("toggled(bool)"), lambda: self.IPRUpdate(3))
        self.connect(self.sssCheckBox, QtCore.SIGNAL("toggled(bool)"), lambda: self.IPRUpdate(3))
        self.connect(self.shaderComboBox, QtCore.SIGNAL("currentIndexChanged(int)"), lambda: self.IPRUpdate(4))
        self.connect(self.textureRepeatSpinbox, QtCore.SIGNAL("valueChanged(int)"), lambda: self.IPRUpdate(5))
        self.connect(self.selectedShaderCheckbox, QtCore.SIGNAL("toggled(bool)"), lambda: self.IPRUpdate(4))

        self.setLayout(mainLayout)

    def getCamera(self):
        ''' Returns current selected camera from GUI '''
        if self.cameraComboBox.currentIndex() == 0:
            camera = self.getSceneOption(1)
        else:
            camera = self.cameraComboBoxDict[self.cameraComboBox.currentIndex()]
            if cmds.listRelatives(camera, s=1) != None:
                camera = cmds.listRelatives(camera, s=1)[0]
        return camera

    def getRegion(self, attr, resScale = True):
        if resScale:
            resValue = self.resolutionSpinBox.value()
        else:
            resValue = 100

        ovrScnValue = self.overscanSpinBox.value() * resValue / 100

        xres = self.getSceneOption(2) * resValue / 100
        yres = self.getSceneOption(3) * resValue / 100

        result = {0 : lambda: xres,
                  1 : lambda: yres,
                  2 : lambda: (self.renderRegionXSpinBox.value() * resValue / 100) - ovrScnValue,
                  3 : lambda: yres - (self.renderRegionTSpinBox.value() * resValue / 100) - ovrScnValue,
                  4 : lambda: (self.renderRegionRSpinBox.value() * resValue / 100) - 1 + ovrScnValue,
                  5 : lambda: (yres - (self.renderRegionYSpinBox.value() * resValue / 100)) - 1 + ovrScnValue}[attr]()

        return result

    def setOverscan(self):
        ovrScnValue = bool(self.overscanSpinBox.value())
        if cmds.getAttr("defaultRenderGlobals.ren") == "arnold":
            message = "Do you want to set the Overscan values in Render Setttings?"
            result = cmds.confirmDialog(title='Overscan',
                                        message=message,
                                        button=['OK', 'Cancel'],
                                        defaultButton='OK',
                                        cancelButton='Cancel',
                                        dismissString='Cancel',
                                        icn="information")
            if result == 'OK':
                rMinX = self.getRegion(2, False)
                rMinY = self.getRegion(3, False)
                rMaxX = self.getRegion(4, False)
                rMaxY = self.getRegion(5, False)
                attr = "defaultArnoldRenderOptions.outputOverscan"
                if ovrScnValue:
                    cmds.setAttr(attr, "%s %s %s %s"%(rMinX, rMinY, rMaxX, rMaxY), type="string")
                else:
                    cmds.setAttr(attr, "", type="string")


    def getNukeCropNode(self, *args):
        ''' Get crop node data from Nuke '''
        def find_between(s, first, last):
            try:
                start = s.index(first) + len(first)
                end = s.index(last, start)
                return s[start:end]
            except ValueError:
                return ""

        clipboard = QtWidgets.QApplication.clipboard()
        data = clipboard.text()

        checkData1 = "set cut_paste_input [stack 0]"
        checkData2 = "Crop {"

        if (checkData1 in data.split('\n', 10)[0]) and \
           (checkData2 in data.split('\n', 10)[3]):
                cropData = find_between(data.split('\n', 10)[4], "box {", "}" ).split()
                nkX, nkY, nkR, nkT = int(float(cropData[0])),\
                                     int(float(cropData[1])),\
                                     int(float(cropData[2])),\
                                     int(float(cropData[3]))

                self.renderRegionXSpinBox.setValue(nkX)
                self.renderRegionYSpinBox.setValue(nkY)
                self.renderRegionRSpinBox.setValue(nkR)
                self.renderRegionTSpinBox.setValue(nkT)

                return cropData

    def render(self):
        ''' Starts the render '''
        try: # If MtoA was not found
            defaultTranslator = cmds.getAttr("defaultArnoldDisplayDriver.aiTranslator")
        except ValueError:
            cmds.warning("Current renderer is not set to Arnold.")
            return

        cmds.setAttr("defaultArnoldDisplayDriver.aiTranslator", "aton", type="string")

        # Updating extraHost and port from UI
        if self.defaultPort != 0:
            extraHost = self.extraHostLineEdit.text()
            port = self.portSpinBox.value()
            cmds.setAttr("defaultArnoldDisplayDriver.extraHost", extraHost, type="string")
            cmds.setAttr("defaultArnoldDisplayDriver.port", port)
        else:
            cmds.warning("Current renderer is not set to Arnold or Aton driver is not loaded.")
            return

        # Adding time changed callback
        if self.timeChangedCB == None:
            self.timeChangedCB = OM.MEventMessage.addEventCallback("timeChanged", self.timeChnaged)

        # Adding selection changed callback
        if self.selectionChangedCB == None:
            self.selectionChangedCB = OM.MEventMessage.addEventCallback('SelectionChanged', self.selectionChanged)

        try: # If render session is not started yet
            cmds.arnoldIpr(mode='stop')
        except RuntimeError:
            pass

        # Temporary makeing hidden cameras visible before scene export
        hCams = []
        for x in cmds.listCameras():
            vis_shape = cmds.getAttr("%s.visibility" %x)
            vis_parent = None
            parent = cmds.listRelatives(x, s=1)
            if parent:
                vis_parent = cmds.getAttr("%s.visibility" % parent[0])
            if not vis_shape or not vis_parent:
                hCams.append(x)
        
        for i in hCams: cmds.showHidden(i)

        if self.sequence_enabled:
            # Set Progressive refinement to off
            cmds.setAttr("defaultArnoldRenderOptions.progressive_rendering", False)

        try: # Start IPR
            cmds.arnoldIpr(cam=self.getCamera(), mode='start')
            sys.stdout.write("// Info: Aton - Render started.\n")
        except RuntimeError:
            cmds.warning("Current renderer is not set to Arnold.")

        # Sequence Rendering
        if self.sequence_enabled:
            self.frame_sequence.start()

        # Update IPR
        self.IPRUpdate()

        # Setting back to default
        for i in hCams: cmds.hide(i)
        cmds.setAttr("defaultArnoldDisplayDriver.aiTranslator", defaultTranslator, type="string")
        cmds.setAttr("defaultArnoldDisplayDriver.port", self.defaultPort)

    def getFrames(self):
        frames = range(self.startSpinBox.value(),
                       self.endSpinBox.value() + 1,
                       self.stepSpinBox.value())
        return frames

    @property
    def sequence_enabled(self):
        return self.seqCheckBox.checkState()

    def sequence_started(self):
        # Setup frame_sequence
        self.frame_sequence.frames = self.getFrames()

        # Setup progress bar
        progressKeys = {"edit":True,
                        "beginProgress":True,
                        "isInterruptable":True,
                        "maxValue": len(self.frame_sequence.frames),
                        "status": 'Aton Frame Sequence'}

        self.gMainProgressBar = mel.eval('$tmp = $gMainProgressBar')
        cmds.progressBar( self.gMainProgressBar, **progressKeys)
        # maya.api bugfix
        if cmds.progressBar(self.gMainProgressBar, q=True, ic=True):
            cmds.progressBar(self.gMainProgressBar, e=True, ep=True)
            cmds.progressBar(self.gMainProgressBar, **progressKeys)

    def sequence_stopped(self):
        # Stop ipr when finished
        self.stop()

        # Restore default progressive refinement
        cmds.setAttr("defaultArnoldRenderOptions.progressive_rendering", self.defaultRefinement)

        # kill progressBar
        cmds.progressBar(self.gMainProgressBar, edit=True, endProgress=True)

    def sequence_stepped(self, frame):
        # Refresh IPR
        self.IPRUpdate()

        # step progressBar
        cmds.progressBar(self.gMainProgressBar, edit=True, step=1)

    def initOvrShaders(self):
        ''' Initilize override shaders '''
        # Checker shader
        self.checkerShader = AiNode("standard")
        checkerTexture = AiNode("MayaChecker")
        self.placeTexture = AiNode("MayaPlace2DTexture")
        AiNodeLink(self.placeTexture, "uvCoord", checkerTexture)
        AiNodeLink(checkerTexture, "Kd", self.checkerShader)

        # Grey Shader
        self.greyShader = AiNode("standard")
        AiNodeSetFlt(self.greyShader, "Kd", 0.225)
        AiNodeSetFlt(self.greyShader, "Ks", 1)
        AiNodeSetFlt(self.greyShader, "specular_roughness", 0.3)
        AiNodeSetBool(self.greyShader, "specular_Fresnel", True)
        AiNodeSetBool(self.greyShader, "Fresnel_use_IOR", True)
        AiNodeSetFlt(self.greyShader, "IOR", 1.3)

        # Mirror Shader
        self.mirrorShader = AiNode("standard")
        AiNodeSetFlt(self.mirrorShader, "Kd", 0)
        AiNodeSetFlt(self.mirrorShader, "Ks", 1)
        AiNodeSetFlt(self.mirrorShader, "specular_roughness", 0.005)
        AiNodeSetBool(self.mirrorShader, "specular_Fresnel", True)
        AiNodeSetFlt(self.mirrorShader, "Ksn", 0.6)

        # Normal Shader
        self.normalShader = AiNode("utility")
        AiNodeSetInt(self.normalShader, "shade_mode", 2)
        AiNodeSetInt(self.normalShader, "color_mode", 2)

        # Occlusion Shader
        self.occlusionShader = AiNode("utility")
        AiNodeSetInt(self.occlusionShader, "shade_mode", 3)

        # UV Shader
        self.uvShader = AiNode("utility")
        AiNodeSetInt(self.uvShader, "shade_mode", 2)
        AiNodeSetInt(self.uvShader, "color_mode", 5)

    def IPRUpdate(self, attr=None):
        ''' This method is called during IPR session '''
        try: # If render session is not started yet
            cmds.arnoldIpr(mode='pause')
        except (AttributeError, RuntimeError):
            return

        options = AiUniverseGetOptions()

        # Camera Update
        if attr == None or attr == 0:
            camera = self.getCamera()
            iterator = AiUniverseGetNodeIterator(AI_NODE_CAMERA)
            while not AiNodeIteratorFinished(iterator):
                node = AiNodeIteratorGetNext(iterator)
                if AiNodeGetName(node) == camera:
                    AiNodeSetPtr(options, "camera", node)

        # Resolution and Region Update
        if attr == None or attr == 1:

            AiNodeSetInt(options, "xres", self.getRegion(0))
            AiNodeSetInt(options, "yres", self.getRegion(1))

            AiNodeSetInt(options, "region_min_x", self.getRegion(2))
            AiNodeSetInt(options, "region_min_y", self.getRegion(3))
            AiNodeSetInt(options, "region_max_x", self.getRegion(4))
            AiNodeSetInt(options, "region_max_y", self.getRegion(5))

        # Camera AA Update
        if attr == None or attr == 2:
            cameraAA = self.cameraAaSpinBox.value()
            options = AiUniverseGetOptions()
            AiNodeSetInt(options, "AA_samples", cameraAA)

        # Ignore options Update
        if attr == None or attr == 3:
            motionBlur = self.motionBlurCheckBox.isChecked()
            subdivs = self.subdivsCheckBox.isChecked()
            displace = self.displaceCheckBox.isChecked()
            bump = self.bumpCheckBox.isChecked()
            sss = self.sssCheckBox.isChecked()

            AiNodeSetBool(options, "ignore_motion_blur", motionBlur)
            AiNodeSetBool(options, "ignore_subdivision", subdivs)
            AiNodeSetBool(options, "ignore_displacement", displace)
            AiNodeSetBool(options, "ignore_bump", bump)
            AiNodeSetBool(options, "ignore_sss", sss)

        # Storing default shader assignments
        if attr == None:
            self.initOvrShaders()
            self.shadersDict = {}
            iterator = AiUniverseGetNodeIterator(AI_NODE_SHAPE)
            while not AiNodeIteratorFinished(iterator):
                node = AiNodeIteratorGetNext(iterator)
                name = AiNodeGetName(node)
                try: # If object name is not exist i.e. "root"
                    sgList = cmds.listConnections(name, type='shadingEngine')
                    if sgList > 0:
                        self.shadersDict[name] = AiNodeGetPtr(node, "shader")
                except ValueError:
                    continue

        # Shader override Update
        shaderIndex = self.shaderComboBox.currentIndex()
        if attr == 4 or shaderIndex > 0:
            iterator = AiUniverseGetNodeIterator(AI_NODE_SHAPE)
            while not AiNodeIteratorFinished(iterator):
                node = AiNodeIteratorGetNext(iterator)
                name = AiNodeGetName(node)

                selChecked = self.selectedShaderCheckbox.isChecked()
                if shaderIndex != 0 and selChecked:
                    selectionList = cmds.ls(dag=1, sl=1, s=1)
                    if selectionList > 0 and name not in selectionList:
                        if name in self.shadersDict:
                            defShader = self.shadersDict[AiNodeGetName(node)]
                            AiNodeSetPtr(node, "shader", defShader)
                        continue

                # Setting overrides
                if name in self.shadersDict:
                    defShader = self.shadersDict[AiNodeGetName(node)]
                    result = {0: lambda: AiNodeSetPtr(node, "shader", defShader),
                              1: lambda: AiNodeSetPtr(node, "shader", self.checkerShader),
                              2: lambda: AiNodeSetPtr(node, "shader", self.greyShader),
                              3: lambda: AiNodeSetPtr(node, "shader", self.mirrorShader),
                              4: lambda: AiNodeSetPtr(node, "shader", self.normalShader),
                              5: lambda: AiNodeSetPtr(node, "shader", self.occlusionShader),
                              6: lambda: AiNodeSetPtr(node, "shader", self.uvShader)}[shaderIndex]()

        # Texture Repeat Udpate
        if attr == None or attr == 5:
            texRepeat = self.textureRepeatSpinbox.value()
            AiNodeSetPnt2(self.placeTexture, "repeatUV", texRepeat, texRepeat)

        try:
            cmds.arnoldIpr(mode='unpause')
        except RuntimeError:
            pass

    def timeChnaged(self, *args):
        ''' Callback method to update the frame number attr '''
        options = AiUniverseGetOptions()
        time = cmds.currentTime(q=1)
        AiNodeSetFlt(options, "frame", time)

    def selectionChanged(self, *args):
        ''' Callback method to update the frame number attr '''
        shaderIndex = self.shaderComboBox.currentIndex()
        selectedObjects = self.selectedShaderCheckbox.isChecked()
        if shaderIndex > 0 and selectedObjects:
            self.IPRUpdate(4)

    def stop(self):
        ''' Stops the render session and removes the callbacks '''
        if self.timeChangedCB != None:
            OM.MEventMessage.removeCallback(self.timeChangedCB)
            self.timeChangedCB = None

        if self.selectionChangedCB != None:
            OM.MEventMessage.removeCallback(self.selectionChangedCB)
            self.selectionChangedCB = None

        try:
            cmds.arnoldIpr(mode='stop')
            sys.stdout.write("// Info: Aton - Render stopped.\n")
        except (AttributeError, RuntimeError):
            return

        self.frame_sequence.stop()

    def closeEvent(self, event):
        ''' Removes callback when closing the GUI '''
        self.stop()
        self.frame_sequence.stop()

    def keyPressEvent(self, event):
        if event.key() == QtCore.Qt.Key_Escape:
            self.frame_sequence.stop()

class Signal(set):
    '''Qt Signal Clone allows'''
    connect = set.add
    disconnect = set.discard
    def emit(self, *args, **kwargs):
        for fn in list(self):
            fn(*args, **kwargs)

class AiFrameSequence(object):
    '''
    Step through a batch of frames using the specified timeout in seconds. For
    each frame wait for the frame to start rendering, and then stop rendering
    before moving on. Stop stepping early using the stop method.
    AiFrameSequence emits the following signals: started, stopped, stepped,
    frame_changed. stepped emits the step in the inner frame loop, it can be
    used to report progress. frame_changed emits the frame number when the
    frame is changed.

    usage::

       b = AiFrameSequence(xrange(10, 20, 2), 1)
       b.start()
       # OR LIKE THIS
       b = AiFrameSequence()
       b.frames = xrange(10, 20, 2)
       b.timeout = 1
       b.start()
    '''

    def __init__(self, frames=None, timeout=None):
        self.frames = frames or []
        self.timeout = timeout
        self.running = False
        self.started = Signal()
        self.stopped = Signal()
        self.stepped = Signal()
        self.frame_changed = Signal()

    def change_frame(self, frame):
        cmds.currentTime(frame)
        options = AiUniverseGetOptions()
        AiNodeSetFlt(options, "frame", frame)
        self.frame_changed.emit(frame)

    def start(self):
        '''Start stepping through frames'''
        self.running = True
        self.started.emit()
        gMainProgressBar = mel.eval('$tmp = $gMainProgressBar')

        for i, frame in enumerate(self.frames):
            isCancelled = cmds.progressBar(gMainProgressBar, q=True, ic=True)
            if not self.running or isCancelled:
                break
            self.change_frame(frame)
            self.stepped.emit(i)

            # Sleep until frame starts, then finishes
            sleep_until(conditions = [AiRendering, lambda: not AiRendering()],
                        wake_condition = lambda: not self.running,
                        timeout=self.timeout)

        self.running = False
        self.stopped.emit()

    def stop(self):
        '''Stop stepping through frames'''
        self.running = False

def qt_sleep(secs=0):
    '''Non-blocking sleep for Qt'''
    start = default_timer()

    while True:
        QtWidgets.qApp.processEvents()
        if default_timer() - start > secs:
            return

def sleep_until(conditions, wake_condition=None, timeout=None):
    '''
    Process qApp events until a sequence of conditions becomes True. Return
    when each condition returns True or the wake_condition returns True or
    the timeout is reached...
    :param conditions: Sequence of callables returning True or False
    :param wake_condition: Optional callable returning True or False
    :param timeout: Number of seconds to wait before returning
    '''

    start = default_timer()

    for condition in conditions:
        while True:
            if condition():
                break
            if timeout:
                if default_timer() - start > timeout:
                    break
            if wake_condition():
                break

            qt_sleep(0.1)


if __name__ == "__main__":
    aton = Aton()
    aton.show()
