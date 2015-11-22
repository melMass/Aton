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

class RenderConnect(QtGui.QDialog):

	def __init__(self, parent = maya_main_window()):
		super(RenderConnect, self).__init__(parent)

		self.windowName = "RenderConnect"

		if cmds.window(self.windowName, exists = True):
			cmds.deleteUI(self.windowName, wnd = True)

		self.setupUi()

	def getSceneOptions(self):
		sceneOptions = {}
		if cmds.getAttr("defaultRenderGlobals.ren") == "arnold":
			sceneOptions["camera"] = core.ACTIVE_CAMERA
			sceneOptions["width"]  = int(cmds.getAttr("defaultResolution.width"))
			sceneOptions["height"] = int(cmds.getAttr("defaultResolution.height"))
			try:
				sceneOptions["AASamples"] = int(cmds.getAttr("defaultArnoldRenderOptions.AASamples"))
			except ValueError:
				mel.eval("unifiedRenderGlobalsWindow;")
				sceneOptions["AASamples"] = int(cmds.getAttr("defaultArnoldRenderOptions.AASamples"))
			sceneOptions["motionBlur"] = bool(cmds.getAttr("defaultArnoldRenderOptions.motion_blur_enable")
											  and not cmds.getAttr("defaultArnoldRenderOptions.ignoreMotionBlur"))

		else:
			sceneOptions["camera"] = "None"
			sceneOptions["width"]  = 0
			sceneOptions["height"] = 0
			sceneOptions["AASamples"] = 0
			sceneOptions["motionBlur"] = 0
			cmds.warning("Current renderer is not set to Arnold.")

		return sceneOptions

	def setupUi(self):
		self.setObjectName(self.windowName)
		self.setWindowTitle("RenderConnect")
		self.setWindowFlags(QtCore.Qt.Tool)
		self.setAttribute(QtCore.Qt.WA_DeleteOnClose)
		self.setMinimumSize(300, 200)
		self.setMaximumSize(300, 200)

		mainLayout = QtGui.QVBoxLayout()
		mainLayout.setContentsMargins(5,5,5,5)
		mainLayout.setSpacing(2)

		overridesGroupBox = QtGui.QGroupBox("Overrides")
		overridesLayout = QtGui.QVBoxLayout(overridesGroupBox)

		cameraLayout = QtGui.QHBoxLayout()
		cameraLabel = QtGui.QLabel("Camera:")
		cameraLabel.setAlignment(QtCore.Qt.AlignRight|QtCore.Qt.AlignVCenter)
		self.cameraComboBox = QtGui.QComboBox()
		self.cameraComboBoxDict = {}
		self.cameraComboBox.addItem("Current")

		for i in cmds.listCameras():
			self.cameraComboBox.addItem(i)
			self.cameraComboBoxDict[cmds.listCameras().index(i)+1] = i

		motionBlurLabel = QtGui.QLabel("Motion Blur:")
		motionBlurLabel.setAlignment(QtCore.Qt.AlignRight|QtCore.Qt.AlignVCenter)
		self.motionBlurComboBox = QtGui.QComboBox()
		self.motionBlurComboBox.setMaximumSize(70,20)
		self.motionBlurComboBox.addItem("Disable")
		self.motionBlurComboBox.addItem("Enable")
		self.motionBlurComboBox.setCurrentIndex(self.getSceneOptions()["motionBlur"])


		cameraLayout.addWidget(cameraLabel)
		cameraLayout.addWidget(self.cameraComboBox)
		cameraLayout.addWidget(motionBlurLabel)
		cameraLayout.addWidget(self.motionBlurComboBox)

		resolutionLayout = QtGui.QHBoxLayout()
		resolutionLabel = QtGui.QLabel("Resolution %:")
		resolutionLabel.setAlignment(QtCore.Qt.AlignRight|QtCore.Qt.AlignVCenter)

		self.resolutionSpinBox = QtGui.QSpinBox()
		self.resolutionSpinBox.setButtonSymbols(QtGui.QAbstractSpinBox.NoButtons)
		self.resolutionSpinBox.setMaximum(900)
		self.resolutionSpinBox.setValue(100)
		resolutionSlider = QtGui.QSlider()
		resolutionSlider.setOrientation(QtCore.Qt.Horizontal)
		resolutionSlider.setValue(100)
		resolutionSlider.setMaximum(100)
		resolutionSlider.sliderMoved.connect(self.resolutionSpinBox.setValue)

		resolutionLayout.addWidget(resolutionLabel)
		resolutionLayout.addWidget(self.resolutionSpinBox)
		resolutionLayout.addWidget(resolutionSlider)


		cameraAaLayout = QtGui.QHBoxLayout()
		cameraAaLabel = QtGui.QLabel("Camera (AA):")
		cameraAaLabel.setAlignment(QtCore.Qt.AlignRight|QtCore.Qt.AlignVCenter)

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


		mainButtonslayout = QtGui.QHBoxLayout()
		startButton = QtGui.QPushButton("Start/Refresh")
		stopButton = QtGui.QPushButton("Stop")

		startButton.clicked.connect(self.render)
		stopButton.clicked.connect(self.stop)


		mainButtonslayout.addWidget(startButton)
		mainButtonslayout.addWidget(stopButton)

		overridesLayout.addLayout(cameraLayout)
		overridesLayout.addLayout(resolutionLayout)
		overridesLayout.addLayout(cameraAaLayout)

		mainLayout.addWidget(overridesGroupBox)

		mainLayout.addLayout(mainButtonslayout)

		self.setLayout(mainLayout)

	def render(self):
		try:
			cmds.arnoldIpr(mode='stop')
		except RuntimeError:
			pass

		defaultTranslator = cmds.getAttr("defaultArnoldDisplayDriver.aiTranslator")

		try:
			cmds.setAttr("defaultArnoldDisplayDriver.aiTranslator", "nuke", type="string")
		except RuntimeError:
			cmds.warning("RenderConnect driver for Arnold is not installed")
			return

		if self.cameraComboBox.currentIndex() == 0:
			camera = self.getSceneOptions()["camera"]
		else:
			camera = cmds.listRelatives(self.cameraComboBoxDict[self.cameraComboBox.currentIndex()], s=1)[0]

		width = self.getSceneOptions()["width"] * self.resolutionSpinBox.value() / 100
		height = self.getSceneOptions()["height"] * self.resolutionSpinBox.value() / 100
		AASamples = self.cameraAaSpinBox.value()
		motionBlur = not self.motionBlurComboBox.currentIndex()

		core.createOptions()
		cmds.arnoldIpr(cam=camera, width=width, height=height, mode='start')
		nodeIter = AiUniverseGetNodeIterator(AI_NODE_ALL)

		while not AiNodeIteratorFinished(nodeIter):
			node = AiNodeIteratorGetNext(nodeIter)
			AiNodeSetInt(node, "AA_samples", AASamples)
			AiNodeSetBool(node, "ignore_motion_blur", motionBlur)

		# Temp trigger in order to start IPR immediately
		cmds.setAttr("%s.bestFitClippingPlanes"%camera, True)

		cmds.setAttr("defaultArnoldDisplayDriver.aiTranslator", defaultTranslator, type="string")


	def stop(self):
		cmds.arnoldIpr(mode='stop')


if __name__ == "__main__":
	rc = RenderConnect()
	rc.show()
