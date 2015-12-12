import mtoa.core as core
import maya.mel as mel

from arnold import *
from maya import cmds, OpenMayaUI
from PySide import QtCore
from PySide import QtGui
from shiboken import wrapInstance

def maya_main_window():
	main_window_ptr = OpenMayaUI.MQtUtil.mainWindow()
	return wrapInstance(long(main_window_ptr), QtGui.QWidget)

class Aton(QtGui.QDialog):

	def __init__(self, parent = maya_main_window()):
		super(Aton, self).__init__(parent)

		self.windowName = "Aton"

		if cmds.window(self.windowName, exists = True):
			cmds.deleteUI(self.windowName, wnd = True)

		self.setupUi()

	def getSceneOptions(self):
		sceneOptions = {}
		if cmds.getAttr("defaultRenderGlobals.ren") == "arnold":
			sceneOptions["camera"] = core.ACTIVE_CAMERA
			sceneOptions["width"]  = cmds.getAttr("defaultResolution.width")
			sceneOptions["height"] = cmds.getAttr("defaultResolution.height")
			try:
				sceneOptions["AASamples"] = cmds.getAttr("defaultArnoldRenderOptions.AASamples")
			except ValueError:
				mel.eval("unifiedRenderGlobalsWindow;")
			sceneOptions["AASamples"] = cmds.getAttr("defaultArnoldRenderOptions.AASamples")
			sceneOptions["motionBlur"] = not (cmds.getAttr("defaultArnoldRenderOptions.motion_blur_enable") \
										 and not cmds.getAttr("defaultArnoldRenderOptions.ignoreMotionBlur"))
			sceneOptions["subdivs"] = cmds.getAttr("defaultArnoldRenderOptions.ignoreSubdivision")
			sceneOptions["displace"] = cmds.getAttr("defaultArnoldRenderOptions.ignoreDisplacement")
			sceneOptions["bump"] = cmds.getAttr("defaultArnoldRenderOptions.ignoreBump")
			sceneOptions["sss"] = cmds.getAttr("defaultArnoldRenderOptions.ignoreSss")

		else:
			sceneOptions["camera"] = "None"
			sceneOptions["width"]  = 0
			sceneOptions["height"] = 0
			sceneOptions["AASamples"] = 0
			sceneOptions["motionBlur"] = 0
			sceneOptions["subdivs"] = 0
			sceneOptions["displace"] = 0
			sceneOptions["bump"] = 0
			sceneOptions["sss"] = 0
			cmds.warning("Current renderer is not set to Arnold.")

		return sceneOptions

	def setupUi(self):
		self.setObjectName(self.windowName)
		self.setWindowTitle("Aton")
		self.setWindowFlags(QtCore.Qt.Tool)
		self.setAttribute(QtCore.Qt.WA_DeleteOnClose)
		self.setMinimumSize(375, 200)
		self.setMaximumSize(375, 200)

		mainLayout = QtGui.QVBoxLayout()
		mainLayout.setContentsMargins(5,5,5,5)
		mainLayout.setSpacing(2)

		overridesGroupBox = QtGui.QGroupBox("Overrides")
		overridesLayout = QtGui.QVBoxLayout(overridesGroupBox)

		cameraLayout = QtGui.QHBoxLayout()
		cameraLabel = QtGui.QLabel("Camera:")
		cameraLabel.setAlignment(QtCore.Qt.AlignRight|QtCore.Qt.AlignVCenter)
		cameraLabel.setMaximumSize(75, 20)
		cameraLabel.setMinimumSize(75, 20)
		self.cameraComboBox = QtGui.QComboBox()
		self.cameraComboBoxDict = {}
		self.cameraComboBox.addItem("Current (%s)"%self.getSceneOptions()["camera"])
		for i in cmds.listCameras():
			self.cameraComboBox.addItem(i)
			self.cameraComboBoxDict[cmds.listCameras().index(i)+1] = i
		cameraLayout.addWidget(cameraLabel)
		cameraLayout.addWidget(self.cameraComboBox)


		resolutionLayout = QtGui.QHBoxLayout()
		resolutionLabel = QtGui.QLabel("Resolution %:")
		resolutionLabel.setAlignment(QtCore.Qt.AlignRight|QtCore.Qt.AlignVCenter)
		resolutionLabel.setMinimumSize(75, 20)
		self.resolutionSpinBox = QtGui.QSpinBox()
		self.resolutionSpinBox.setButtonSymbols(QtGui.QAbstractSpinBox.NoButtons)
		self.resolutionSpinBox.setMinimum(1)
		self.resolutionSpinBox.setMaximum(900)
		self.resolutionSpinBox.setValue(100)
		resolutionSlider = QtGui.QSlider()
		resolutionSlider.setOrientation(QtCore.Qt.Horizontal)
		resolutionSlider.setValue(20)
		resolutionSlider.setMaximum(20)
		resolutionSlider.sliderMoved.connect(self.resolutionSpinBox.setValue)
		resolutionLayout.addWidget(resolutionLabel)
		resolutionLayout.addWidget(self.resolutionSpinBox)
		resolutionLayout.addWidget(resolutionSlider)

		def updateUi():
			self.resolutionSpinBox.setValue(resolutionSlider.value()*5)
			self.cameraAaSpinBox.setValue(cameraAaSlider.value())


		cameraAaLayout = QtGui.QHBoxLayout()
		cameraAaLabel = QtGui.QLabel("Camera (AA):")
		cameraAaLabel.setAlignment(QtCore.Qt.AlignRight|QtCore.Qt.AlignVCenter)
		cameraAaLabel.setMinimumSize(75, 20)
		self.cameraAaSpinBox = QtGui.QSpinBox()
		self.cameraAaSpinBox.setButtonSymbols(QtGui.QAbstractSpinBox.NoButtons)
		self.cameraAaSpinBox.setMaximum(64)
		self.cameraAaSpinBox.setValue(self.getSceneOptions()["AASamples"])
		cameraAaSlider = QtGui.QSlider()
		cameraAaSlider.setOrientation(QtCore.Qt.Horizontal)
		cameraAaSlider.setValue(self.cameraAaSpinBox.value())
		cameraAaSlider.setMaximum(16)
		cameraAaSlider.sliderMoved.connect(self.cameraAaSpinBox.setValue)
		cameraAaLayout.addWidget(cameraAaLabel)
		cameraAaLayout.addWidget(self.cameraAaSpinBox)
		cameraAaLayout.addWidget(cameraAaSlider)


		ignoreLayout = QtGui.QHBoxLayout()
		ignoreLabel = QtGui.QLabel("Ignore:")
		ignoreLabel.setAlignment(QtCore.Qt.AlignRight|QtCore.Qt.AlignVCenter)
		self.motionBlurCheckBox = QtGui.QCheckBox("Motion Blur")
		self.motionBlurCheckBox.setChecked(not self.getSceneOptions()["motionBlur"])
		self.subdivsCheckBox = QtGui.QCheckBox("Subdivs")
		self.subdivsCheckBox.setChecked(not self.getSceneOptions()["subdivs"])
		self.displaceCheckBox = QtGui.QCheckBox("Displace")
		self.displaceCheckBox.setChecked(not self.getSceneOptions()["displace"])
		self.bumpCheckBox = QtGui.QCheckBox("Bump")
		self.bumpCheckBox.setChecked(not self.getSceneOptions()["bump"])
		self.sssCheckBox = QtGui.QCheckBox("SSS")
		self.sssCheckBox.setChecked(not self.getSceneOptions()["sss"])
		ignoreLayout.addWidget(self.motionBlurCheckBox)
		ignoreLayout.addWidget(self.subdivsCheckBox)
		ignoreLayout.addWidget(self.displaceCheckBox)
		ignoreLayout.addWidget(self.bumpCheckBox)
		ignoreLayout.addWidget(self.sssCheckBox)


		mainButtonslayout = QtGui.QHBoxLayout()
		startButton = QtGui.QPushButton("Start / Refresh")
		stopButton = QtGui.QPushButton("Stop")

		startButton.clicked.connect(self.render)
		stopButton.clicked.connect(self.stop)

		mainButtonslayout.addWidget(startButton)
		mainButtonslayout.addWidget(stopButton)

		overridesLayout.addLayout(cameraLayout)
		overridesLayout.addLayout(resolutionLayout)
		overridesLayout.addLayout(cameraAaLayout)
		overridesLayout.addLayout(ignoreLayout)

		mainLayout.addWidget(overridesGroupBox)
		mainLayout.addLayout(mainButtonslayout)

		self.connect(resolutionSlider, QtCore.SIGNAL("valueChanged(int)"), updateUi)
		self.connect(cameraAaSlider, QtCore.SIGNAL("valueChanged(int)"), updateUi)

		self.setLayout(mainLayout)

	def render(self):
		try:
			cmds.arnoldIpr(mode='stop')
		except RuntimeError:
			pass

		defaultTranslator = cmds.getAttr("defaultArnoldDisplayDriver.aiTranslator")

		try:
			cmds.setAttr("defaultArnoldDisplayDriver.aiTranslator", "aton", type="string")
		except RuntimeError:
			cmds.warning("Aton driver for Arnold is not installed")
			return

		if self.cameraComboBox.currentIndex() == 0:
			camera = self.getSceneOptions()["camera"]
		else:
			camera = cmds.listRelatives(self.cameraComboBoxDict[self.cameraComboBox.currentIndex()], s=1)[0]

		width = self.getSceneOptions()["width"] * self.resolutionSpinBox.value() / 100
		height = self.getSceneOptions()["height"] * self.resolutionSpinBox.value() / 100
		AASamples = self.cameraAaSpinBox.value()
		motionBlur = not self.motionBlurCheckBox.isChecked()
		subdivs = not self.subdivsCheckBox.isChecked()
		displace = not self.displaceCheckBox.isChecked()
		bump = not self.bumpCheckBox.isChecked()
		sss = not self.sssCheckBox.isChecked()

		core.createOptions()
		cmds.arnoldIpr(cam=camera, width=width, height=height, mode='start')
		nodeIter = AiUniverseGetNodeIterator(AI_NODE_ALL)

		while not AiNodeIteratorFinished(nodeIter):
			node = AiNodeIteratorGetNext(nodeIter)
			AiNodeSetInt(node, "AA_samples", AASamples)
			AiNodeSetBool(node, "ignore_motion_blur", motionBlur)
			AiNodeSetBool(node, "ignore_subdivision", subdivs)
			AiNodeSetBool(node, "ignore_displacement ", displace)
			AiNodeSetBool(node, "ignore_bump", bump)
			AiNodeSetBool(node, "ignore_sss", sss)

		# Temp trigger in order to start IPR immediately
		cmds.setAttr("%s.bestFitClippingPlanes"%camera, True)

		cmds.setAttr("defaultArnoldDisplayDriver.aiTranslator", defaultTranslator, type="string")


	def stop(self):
		cmds.arnoldIpr(mode='stop')


if __name__ == "__main__":
	rc = Aton()
	rc.show()
