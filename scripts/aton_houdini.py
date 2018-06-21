import hou
from PySide2 import QtCore, QtWidgets


def getHost():
    ''' Returns a host name from Aton driver '''
    host = '0'
    return host

def getPort():
    ''' Returns a port number from Aton driver '''
    port = 0
    return port

def getOutputDriverNames():
    ''' Returns a list of all output driver names '''
    return [i.name() for i in hou.nodeType(hou.nodeTypeCategories()['Driver'], 'arnold').instances()]

def getAllCameraNames():
    ''' Returns a list of all camera names '''
    return [i.name() for i in hou.nodeType(hou.nodeTypeCategories()['Object'], 'cam').instances()]

def getCurrentCameraName():
    camera = hou.ui.paneTabOfType(hou.paneTabType.SceneViewer).curViewport().camera()
    if camera is not None:
        return camera.name()

def getSceneOption(attr):
    ''' Returns requested scene option attribute value '''
    return {Aton.host : lambda: getHost(),
            Aton.port : lambda: getPort(),
            Aton.outputDriverNames : lambda: getOutputDriverNames(),
            Aton.cameraNames : lambda: getAllCameraNames(),
            Aton.resX : lambda: 0,
            Aton.resY : lambda: 0,
            Aton.AASamples : lambda: 0,
            Aton.ignoreMotionBlur : lambda: 0, 
            Aton.ignoreSubdivision : lambda: 0,
            Aton.ignoreDisplacement : lambda: 0, 
            Aton.ignoreBump : lambda: 0,
            Aton.ignoreSss : lambda: 0,
            Aton.startFrame : lambda: 0, 
            Aton.endFrame : lambda: 0}[attr]()


class SceneOptions(object):
    pass

class BoxWidget(QtWidgets.QFrame):
        def __init__(self, label, first=True):
            super(BoxWidget, self).__init__()
            self.label = QtWidgets.QLabel(label + ":")
            self.label.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignCenter)

            if first:
                    self.label.setMinimumSize(75, 20)
                    self.label.setMaximumSize(75, 20)

            self.layout = QtWidgets.QHBoxLayout(self)
            self.layout.setSizeConstraint(QtWidgets.QLayout.SetMaximumSize)
            self.layout.setContentsMargins(0, 0, 0, 0)
            self.layout.addWidget(self.label)

class LineEditBox(BoxWidget):
        def __init__(self, label, text='', first=True):
            super(LineEditBox, self).__init__(label, first)

            self.lineEditBox = QtWidgets.QLineEdit()
            self.lineEditBox.setText(u"%s" % text)
            self.layout.addWidget(self.lineEditBox)

        def setEnabled(self, value):
            self.label.setEnabled(value)
            self.lineEditBox.setEnabled(value)

        def text(self):
            return self.lineEditBox.text()

        def setText(self, text):
            self.lineEditBox.setText(text)

class SliderBox(BoxWidget):
        def __init__(self, label, value=0, first=True):
            super(SliderBox, self).__init__(label, first)

            self.spinBox = QtWidgets.QSpinBox()
            self.spinBox.setButtonSymbols(QtWidgets.QAbstractSpinBox.NoButtons)
            self.spinBox.setValue(value)

            self.slider = QtWidgets.QSlider()
            self.slider.setOrientation(QtCore.Qt.Horizontal)
            self.slider.setValue(value)

            self.slider.valueChanged.connect(self.spinBox.setValue)

            self.layout.addWidget(self.spinBox)
            self.layout.addWidget(self.slider)

        def setMinimum(self, spinValue=None, sliderValue=None):
            if spinValue is not None: self.spinBox.setMinimum(spinValue)
            if sliderValue is not None: self.slider.setMinimum(sliderValue)

        def setMaximum(self, spinValue=None, sliderValue=None):
            if spinValue is not None: self.spinBox.setMaximum(spinValue)
            if sliderValue is not None: self.slider.setMaximum(sliderValue)

        def setValue(self, spinValue=None, sliderValue=None):
            if sliderValue is not None: self.slider.setValue(sliderValue)
            if spinValue is not None: self.spinBox.setValue(spinValue)

        def value(self):
            return self.spinBox.value()

        def connect(self, func):
            self.slider.valueChanged.connect(func)

        def setEnabled(self, value):
            self.label.setEnabled(value)
            self.spinBox.setEnabled(value)
            self.slider.setEnabled(value)

        @property
        def valueChanged(self):
            return self.spinBox.valueChanged

class SpinBox(BoxWidget):
        def __init__(self, label, value=0, first=True):
            super(SpinBox, self).__init__(label, first)
            self.spinBox = QtWidgets.QSpinBox()
            self.spinBox.setButtonSymbols(QtWidgets.QAbstractSpinBox.NoButtons)
            self.spinBox.setValue(value)
            self.spinBox.setMaximumSize(50, 20)
            self.spinBox.setRange(-99999, 99999)
        
            self.layout.addWidget(self.spinBox)

        def value(self):
            return self.spinBox.value()

        def setValue(self, value):
            self.spinBox.setValue(value)

        @property
        def valueChanged(self):
            return self.spinBox.valueChanged

class ComboBox(BoxWidget):
        def __init__(self, label, first=True):
            super(ComboBox, self).__init__(label, first)
            self.items = list()

            self.comboBox = QtWidgets.QComboBox()
            self.comboBox.setSizePolicy(QtWidgets.QSizePolicy.Expanding,
                                        QtWidgets.QSizePolicy.Fixed)
            self.currentIndexChanged = self.comboBox.currentIndexChanged

            self.layout.addWidget(self.comboBox)

        def setEnabled(self, value):
            self.label.setEnabled(value)
            self.comboBox.setEnabled(value)

        def setCurrentIndex(self, value):
            self.comboBox.setCurrentIndex(value)

        def currentIndex(self):
            return self.comboBox.currentIndex()

        def currentName(self):
            index = self.comboBox.currentIndex()
            return self.items[index]

        def addItems(self, items):
            for i in items:
                self.comboBox.addItem(i)
            self.items += items

class Aton(QtWidgets.QWidget):
        
    host = SceneOptions()
    port = SceneOptions()
    outputDriverNames = SceneOptions()
    cameraNames = SceneOptions()
    resX = SceneOptions()
    resY = SceneOptions()
    AASamples = SceneOptions()
    ignoreMotionBlur = SceneOptions()
    ignoreSubdivision = SceneOptions()
    ignoreDisplacement = SceneOptions()
    ignoreBump = SceneOptions()
    ignoreSss = SceneOptions()
    startFrame = SceneOptions()
    endFrame = SceneOptions()
    
    def __init__(self):
        QtWidgets.QWidget.__init__(self)
        mainlayout = QtWidgets.QVBoxLayout()

        self.defaultHost = getSceneOption(Aton.host)
        self.defaultPort = getSceneOption(Aton.port)

        # Init UI
        self.objName = self.__class__.__name__.lower()
        self.setObjectName(self.objName)
        self.setWindowTitle(self.__class__.__name__)
        self.setProperty("saveWindowPref", True)
        self.setProperty("houdiniStyle", True)
        self.setStyleSheet(hou.qt.styleSheet())
        
        # Setup UI
        self.deleteInstances()
        self.setupUI()
    
    def deleteInstances(self):
        for w in QtWidgets.QApplication.instance().topLevelWidgets():
            if w.objectName() == self.objName:
                try:
                    w.close()
                except:
                    pass
                
    def setupUI(self):

        def portUpdateUI(value):
            self.portSlider.spinBox.setValue(value + self.defaultPort)

        def resUpdateUI(value):
            self.resolutionSlider.setValue(value * 5)
            xres = getSceneOption(Aton.resX) * value * 5 / 100
            yres = getSceneOption(Aton.resY) * value * 5 / 100
            resolutionInfoLabel.setText("%sx%s"%(xres, yres))

        def sequence_toggled(value):
            self.startSpinBox.setEnabled(value)
            self.endSpinBox.setEnabled(value)
            self.stepSpinBox.setEnabled(value)

        def resetUI(*args):
            self.hostLineEdit.setText(self.defaultHost)
            self.hostCheckBox.setChecked(True)
            self.portSlider.setValue(self.defaultPort, 0)
            self.portCheckBox.setChecked(True)
            self.cameraComboBox.setCurrentIndex(0)
            self.resolutionSlider.setValue(100, 20)
            self.cameraAaSlider.setValue(getSceneOption(Aton.AASamples))
            self.renderRegionXSpinBox.setValue(0)
            self.renderRegionYSpinBox.setValue(0)
            self.renderRegionRSpinBox.setValue(getSceneOption(Aton.resX))
            self.renderRegionTSpinBox.setValue(getSceneOption(Aton.resY))
            self.overscanSlider.setValue(0, 0)
            self.motionBlurCheckBox.setChecked(getSceneOption(Aton.ignoreMotionBlur))
            self.subdivsCheckBox.setChecked(getSceneOption(Aton.ignoreSubdivision))
            self.displaceCheckBox.setChecked(getSceneOption(Aton.ignoreDisplacement))
            self.bumpCheckBox.setChecked(getSceneOption(Aton.ignoreBump))
            self.sssCheckBox.setChecked(getSceneOption(Aton.ignoreSss))
            self.shaderComboBox.setCurrentIndex(0)
            self.textureRepeatSlider.setValue(1, 1)
            self.selectedShaderCheckbox.setChecked(0)
            self.startSpinBox.setValue(getSceneOption(Aton.startFrame))
            self.endSpinBox.setValue(getSceneOption(Aton.endFrame))
            self.stepSpinBox.setValue(1)
            self.seqCheckBox.setChecked(False)

        self.setAttribute(QtCore.Qt.WA_AlwaysShowToolTips)
        self.setAttribute(QtCore.Qt.WA_DeleteOnClose)

        # Main Layout
        mainLayout = QtWidgets.QVBoxLayout()
        
        # General Group
        generalGroupBox = QtWidgets.QGroupBox("General")
        generalLayout = QtWidgets.QVBoxLayout(generalGroupBox)
        
        # Host Layout
        hostLayout = QtWidgets.QHBoxLayout()
        self.hostLineEdit = LineEditBox("Host", u"%s"%self.defaultHost)
        self.hostCheckBox = QtWidgets.QCheckBox()
        self.hostCheckBox.setChecked(True)
        self.hostCheckBox.stateChanged.connect(self.hostLineEdit.setEnabled)
        hostLayout.addWidget(self.hostLineEdit)
        hostLayout.addWidget(self.hostCheckBox)

        # Port Layout
        portLayout = QtWidgets.QHBoxLayout()
        self.portSlider = SliderBox("Port")
        self.portSlider.setMinimum(0, 0)
        self.portSlider.setMaximum(9999, 15)
        self.portSlider.setValue(self.defaultPort)
        self.portSlider.connect(portUpdateUI)
        self.portCheckBox = QtWidgets.QCheckBox()
        self.portCheckBox.setChecked(True)
        self.portCheckBox.stateChanged.connect(self.portSlider.setEnabled)
        portLayout.addWidget(self.portSlider)
        portLayout.addWidget(self.portCheckBox)

        # Output Driver Layout
        outputDriverLayout = QtWidgets.QHBoxLayout()
        self.outputDriverComboBox = ComboBox("Output")
        self.outputDriverComboBox.addItems(getSceneOption(Aton.outputDriverNames))
        outputDriverLayout.addWidget(self.outputDriverComboBox)

        # Camera Layout
        cameraLayout = QtWidgets.QHBoxLayout()
        self.cameraComboBox = ComboBox("Camera")
        self.cameraComboBox.addItems(getSceneOption(Aton.cameraNames))
        cameraLayout.addWidget(self.cameraComboBox)

        # Overrides Group
        overridesGroupBox = QtWidgets.QGroupBox("Overrides")
        overridesLayout = QtWidgets.QVBoxLayout(overridesGroupBox)

        # Resolution Layout
        resolutionLayout = QtWidgets.QHBoxLayout()
        self.resolutionSlider = SliderBox("Resolution %")
        self.resolutionSlider.setMinimum(1, 1)
        self.resolutionSlider.setMaximum(200, 40)
        self.resolutionSlider.setValue(100, 20)
        self.resolutionSlider.connect(resUpdateUI)
        xres, yres = getSceneOption(Aton.resX), getSceneOption(Aton.resY)
        resolutionInfoLabel = QtWidgets.QLabel(str(xres)+'x'+str(yres))
        resolutionInfoLabel.setMaximumSize(100, 20)
        resolutionInfoLabel.setEnabled(False)
        resolutionLayout.addWidget(self.resolutionSlider)
        resolutionLayout.addWidget(resolutionInfoLabel)

        # Camera AA Layout
        cameraAaLayout = QtWidgets.QHBoxLayout()
        self.cameraAaSlider = SliderBox("Camera (AA)")
        self.cameraAaSlider.setMinimum(-64, -3)
        self.cameraAaSlider.setMaximum(64, 16)
        self.cameraAaSlider.setValue(getSceneOption(Aton.AASamples))
        cameraAaLayout.addWidget(self.cameraAaSlider)

        # Render region layout
        renderRegionLayout = QtWidgets.QHBoxLayout()
        self.renderRegionXSpinBox = SpinBox("Region X")
        self.renderRegionYSpinBox = SpinBox("Y", 0, False)
        self.renderRegionRSpinBox = SpinBox("R", 0, False)
        self.renderRegionTSpinBox = SpinBox("T", 0, False)
        self.renderRegionRSpinBox.setValue(getSceneOption(Aton.resX))
        self.renderRegionTSpinBox.setValue(getSceneOption(Aton.resY))
        renderRegionGetNukeButton = QtWidgets.QPushButton("Get")
        renderRegionGetNukeButton.clicked.connect(self.getNukeCropNode)
        renderRegionLayout.addWidget(self.renderRegionXSpinBox)
        renderRegionLayout.addWidget(self.renderRegionYSpinBox)
        renderRegionLayout.addWidget(self.renderRegionRSpinBox)
        renderRegionLayout.addWidget(self.renderRegionTSpinBox)
        renderRegionLayout.addWidget(renderRegionGetNukeButton)

        # Overscan Layout
        overscanLayout = QtWidgets.QHBoxLayout()
        self.overscanSlider = SliderBox("Overscan")
        self.overscanSlider.setMinimum(0)
        self.overscanSlider.setMaximum(9999, 250)
        self.overscanSlider.setValue(0, 0)
        overscanSetButton = QtWidgets.QPushButton("Set")
        overscanLayout.addWidget(self.overscanSlider)
        overscanLayout.addWidget(overscanSetButton)

        # Shaders layout
        shaderLayout = QtWidgets.QHBoxLayout()
        self.shaderComboBox = ComboBox("Shader override", False)
        self.shaderComboBox.addItems(["Disabled", "Checker", "Grey", "Mirror", "Normal", "Occlusion", "UV"])
        self.selectedShaderCheckbox = QtWidgets.QCheckBox("Selected objects only")
        shaderLayout.addWidget(self.shaderComboBox)
        shaderLayout.addWidget(self.selectedShaderCheckbox)

        textureRepeatLayout = QtWidgets.QHBoxLayout()
        self.textureRepeatSlider = SliderBox("Texture repeat", 1, False)
        self.textureRepeatSlider.setMinimum(None, 1)
        self.textureRepeatSlider.setMaximum(None, 64)
        textureRepeatLayout.addWidget(self.textureRepeatSlider)

        # Ignore Group
        ignoresGroupBox = QtWidgets.QGroupBox("Ignore")
        ignoresGroupBox.setMaximumSize(9999, 75)

        # Ignore Layout
        ignoresLayout = QtWidgets.QVBoxLayout(ignoresGroupBox)
        ignoreLayout = QtWidgets.QHBoxLayout()
        ignoreLabel = QtWidgets.QLabel("Ignore:")
        ignoreLabel.setAlignment(QtCore.Qt.AlignRight|QtCore.Qt.AlignVCenter)
        self.motionBlurCheckBox = QtWidgets.QCheckBox("Motion Blur")
        self.motionBlurCheckBox.setChecked(getSceneOption(Aton.ignoreMotionBlur))
        self.subdivsCheckBox = QtWidgets.QCheckBox("Subdivs")
        self.subdivsCheckBox.setChecked(getSceneOption(Aton.ignoreSubdivision))
        self.displaceCheckBox = QtWidgets.QCheckBox("Displace")
        self.displaceCheckBox.setChecked(getSceneOption(Aton.ignoreDisplacement))
        self.bumpCheckBox = QtWidgets.QCheckBox("Bump")
        self.bumpCheckBox.setChecked(getSceneOption(Aton.ignoreBump))
        self.sssCheckBox = QtWidgets.QCheckBox("SSS")
        self.sssCheckBox.setChecked(getSceneOption(Aton.ignoreSss))
        ignoreLayout.addWidget(self.motionBlurCheckBox)
        ignoreLayout.addWidget(self.subdivsCheckBox)
        ignoreLayout.addWidget(self.displaceCheckBox)
        ignoreLayout.addWidget(self.bumpCheckBox)
        ignoreLayout.addWidget(self.sssCheckBox)

        # Sequence Group
        sequenceGroupBox = QtWidgets.QGroupBox('Sequence')
        sequenceGroupBox.setMaximumSize(9999, 75)

        # Sequence Layout
        sequenceLayout = QtWidgets.QHBoxLayout(sequenceGroupBox)
        self.seqCheckBox = QtWidgets.QCheckBox()
        self.seqCheckBox.setMaximumSize(15, 25)
        self.seqCheckBox.stateChanged.connect(sequence_toggled)
        self.startSpinBox = SpinBox('Start frame', False)
        self.startSpinBox.setValue(getSceneOption(Aton.startFrame))
        self.startSpinBox.setEnabled(False)
        self.endSpinBox = SpinBox('End frame', False)
        self.endSpinBox.setValue(getSceneOption(Aton.endFrame))
        self.endSpinBox.setEnabled(False)
        self.stepSpinBox = SpinBox('By frame', False)
        self.stepSpinBox.setValue(1)
        self.stepSpinBox.setEnabled(False)
        sequenceLayout.addWidget(self.seqCheckBox)
        sequenceLayout.addWidget(self.startSpinBox)
        sequenceLayout.addWidget(self.endSpinBox)
        sequenceLayout.addWidget(self.stepSpinBox)

        # Main Buttons Layout
        mainButtonslayout = QtWidgets.QHBoxLayout()
        startButton = QtWidgets.QPushButton("Start / Refresh")
        stopButton = QtWidgets.QPushButton("Stop")
        resetButton = QtWidgets.QPushButton("Reset")
        startButton.clicked.connect(self.startRender)
        stopButton.clicked.connect(self.stopRender)
        resetButton.clicked.connect(resetUI)
        mainButtonslayout.addWidget(startButton)
        mainButtonslayout.addWidget(stopButton)
        mainButtonslayout.addWidget(resetButton)

        # Add Layouts to Main
        generalLayout.addLayout(hostLayout)
        generalLayout.addLayout(portLayout)
        generalLayout.addLayout(outputDriverLayout)
        overridesLayout.addLayout(cameraLayout)
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

        self.setLayout(mainLayout)
    
    def startRender(self):
        pass
    
    def stopRender(self):
        pass
    
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

Aton().show()
