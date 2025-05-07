#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup() {
	ofSetFrameRate(60);
	ofBackground(40);

	// Load font for UI
	font.load(OF_TTF_SANS, 12);

	// Initialize variables
	isConnected = false;
	testRunning = false;
	currentTest = -1;

	// Find available serial ports
	availablePorts = ebbControl.listDevices();

	addLogMessage("EBB Control Test Application");
	addLogMessage("Press SPACE to connect to EBB");
	addLogMessage("Press 'r' to run all tests");
	addLogMessage("Press 1-7 to run individual tests");
	addLogMessage("Available serial ports:");

	if (availablePorts.empty()) {
		addLogMessage("No serial ports found");
	} else {
		for (const auto & port : availablePorts) {
			addLogMessage("  " + port);
		}
	}
}

//--------------------------------------------------------------
void ofApp::update() {
	// If a test is running, check if it's time to proceed to the next step
	if (testRunning && currentTest >= 0) {
		float elapsedTime = ofGetElapsedTimef() - testStartTime;

		// Simple timeout mechanism - if a test has been running for more than 10 seconds, cancel it
		if (elapsedTime > 10.0) {
			testRunning = false;
			currentTest = -1;
			addLogMessage("Test timed out");
		}
	}
}

//--------------------------------------------------------------
void ofApp::draw() {
	ofSetColor(255);

	// Draw connection status
	if (isConnected) {
		ofSetColor(0, 255, 0);
		ofDrawCircle(20, 20, 10);
		ofSetColor(255);
		font.drawString("Connected to: " + portName, 40, 25);
	} else {
		ofSetColor(255, 0, 0);
		ofDrawCircle(20, 20, 10);
		ofSetColor(255);
		font.drawString("Not connected", 40, 25);
	}

	// Draw log messages
	float y = 60;
	for (const auto & msg : logMessages) {
		font.drawString(msg, 20, y);
		y += 20;
	}

	// Draw test status
	if (testRunning) {
		ofSetColor(0, 255, 255);
		font.drawString("Test running...", 20, ofGetHeight() - 20);
	}
}

//--------------------------------------------------------------
void ofApp::exit() {
	if (isConnected) {
		// Safely disconnect from EBB
		addLogMessage("Disconnecting from EBB...");
		try {
			ebbControl.disableMotors();
			ebbControl.setPenState(false); // Pen up
			ebbControl.close();
		} catch (const std::exception & e) {
			addLogMessage("Error during disconnect: " + std::string(e.what()));
		}
	}
}

//--------------------------------------------------------------
void ofApp::addLogMessage(const std::string & message) {
	logMessages.push_back(message);
	ofLogNotice() << message;

	// Keep log messages within limit
	const size_t maxMessages = 20;
	while (logMessages.size() > maxMessages) {
		logMessages.erase(logMessages.begin());
	}
}

//--------------------------------------------------------------
bool ofApp::findAndConnect() {
	if (isConnected) {
		addLogMessage("Already connected");
		return true;
	}

	addLogMessage("Searching for EBB device...");

	// Try to connect to each port
	for (const auto & port : availablePorts) {
		addLogMessage("Trying port: " + port);
		try {
			if (ebbControl.setup(port)) {
				// Try to get version to confirm it's an EBB
				std::string version = ebbControl.getFirmwareVersion();
				if (!version.empty()) {
					portName = port;
					isConnected = true;
					addLogMessage("Connected to EBB on port: " + port);
					addLogMessage("Firmware version: " + version);
					return true;
				} else {
					addLogMessage("Connected but couldn't get firmware version");
					ebbControl.close();
				}
			}
		} catch (const std::exception & e) {
			addLogMessage("Error connecting to " + port + ": " + e.what());
			ebbControl.close();
		}
	}

	addLogMessage("Failed to find EBB device");
	return false;
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key) {
	if (key == ' ') {
		// Connect to EBB
		findAndConnect();
	} else if (key == 'r' || key == 'R') {
		// Run all tests
		if (isConnected && !testRunning) {
			addLogMessage("Running all tests...");
			runAllTests();
		} else if (!isConnected) {
			addLogMessage("Not connected to EBB");
		} else {
			addLogMessage("Test already running");
		}
	} else if (key >= '1' && key <= '7') {
		// Run individual tests
		if (isConnected && !testRunning) {
			int testNum = key - '0';
			addLogMessage("Running test " + std::to_string(testNum));

			testRunning = true;
			currentTest = testNum;
			testStartTime = ofGetElapsedTimef();

			switch (testNum) {
			case 1:
				testMotorControl();
				break;
			case 2:
				testPenControl();
				break;
			case 3:
				testNodeCounter();
				break;
			case 4:
				testEngraver();
				break;
			case 5:
				testServo();
				break;
			case 6:
				testConfiguration();
				break;
			case 7:
				testQueryFunctions();
				break;
			default:
				break;
			}

			testRunning = false;
			currentTest = -1;
		} else if (!isConnected) {
			addLogMessage("Not connected to EBB");
		} else {
			addLogMessage("Test already running");
		}
	}
}

//--------------------------------------------------------------
void ofApp::testMotorControl() {
	addLogMessage("-- Testing Motor Control --");
	try {
		// Enable motors
		addLogMessage("Enabling motors (1/8 step mode)");
		ebbControl.enableMotors(ofxEbbControl::MOTOR_STEP_DIV16, ofxEbbControl::MOTOR_STEP_DIV16);

		// Clear step position
		addLogMessage("Clearing step position");
		ebbControl.clearStepPosition();

		// Query current position
		auto position = ebbControl.getStepPositions();
		addLogMessage("Initial position: " + std::to_string(position[0]) + ", " + std::to_string(position[1]));

		// Move motors (relative coordinates)
		addLogMessage("Moving to (1000, 1000) steps");
		ebbControl.moveStepperSteps(2000, 1000, 1000);

		// Query new position
		position = ebbControl.getStepPositions();
		addLogMessage("New position: " + std::to_string(position[0]) + ", " + std::to_string(position[1]));

		// Move back home
		addLogMessage("Moving back to home position");
		ebbControl.moveAbsolute(1000, 0, 0);

		// Disable motors
		addLogMessage("Disabling motors");
		ebbControl.enableMotors(ofxEbbControl::MOTOR_DISABLE, ofxEbbControl::MOTOR_DISABLE);

		addLogMessage("Motor test completed successfully");
	} catch (const std::exception & e) {
		addLogMessage("Error in motor test: " + std::string(e.what()));
	}
}

//--------------------------------------------------------------
void ofApp::testPenControl() {
	addLogMessage("-- Testing Pen Control --");
	try {
		// Check initial pen state
		bool initialPenDown = ebbControl.isPenDown();
		addLogMessage("Initial pen state: " + std::string(initialPenDown ? "down" : "up"));

		// Set pen up (false = pen up, true = pen down)
		addLogMessage("Setting pen up");
		ebbControl.setPenState(false);
		ofSleepMillis(500);

		// Set pen down
		addLogMessage("Setting pen down");
		ebbControl.setPenState(true);
		ofSleepMillis(500);

		// Toggle pen
		addLogMessage("Toggling pen");
		ebbControl.togglePen();
		ofSleepMillis(500);

		// Set pen up again
		addLogMessage("Setting pen up again");
		ebbControl.setPenState(false);

		// Check servo power
		bool servoPowered = ebbControl.isServoPowered();
		addLogMessage("Servo power status: " + std::string(servoPowered ? "on" : "off"));

		// Set servo power timeout (10 seconds)
		addLogMessage("Setting servo power timeout to 10 seconds");
		ebbControl.setServoPowerTimeout(10000, true);

		addLogMessage("Pen test completed successfully");
	} catch (const std::exception & e) {
		addLogMessage("Error in pen test: " + std::string(e.what()));
	}
}

//--------------------------------------------------------------
void ofApp::testNodeCounter() {
	addLogMessage("-- Testing Node Counter --");
	try {
		// Get initial node count
		uint32_t initialCount = ebbControl.getNodeCount();
		addLogMessage("Initial node count: " + std::to_string(initialCount));

		// Set node count to 42
		addLogMessage("Setting node count to 42");
		ebbControl.setNodeCount(42);

		// Verify node count
		uint32_t newCount = ebbControl.getNodeCount();
		addLogMessage("New node count: " + std::to_string(newCount));

		// Increment node count
		addLogMessage("Incrementing node count");
		ebbControl.incrementNodeCount();

		// Get updated count
		newCount = ebbControl.getNodeCount();
		addLogMessage("After increment: " + std::to_string(newCount));

		// Decrement node count
		addLogMessage("Decrementing node count");
		ebbControl.decrementNodeCount();

		// Get final count
		newCount = ebbControl.getNodeCount();
		addLogMessage("After decrement: " + std::to_string(newCount));

		// Reset to initial count
		addLogMessage("Resetting to initial count");
		ebbControl.setNodeCount(initialCount);

		addLogMessage("Node counter test completed successfully");
	} catch (const std::exception & e) {
		addLogMessage("Error in node counter test: " + std::string(e.what()));
	}
}

//--------------------------------------------------------------
void ofApp::testEngraver() {
	addLogMessage("-- Testing Engraver Control --");
	try {
		// Turn on engraver at low power
		addLogMessage("Turning on engraver at low power (100)");
		ebbControl.setEngraver(true, 100, true);
		ofSleepMillis(1000);

		// Increase power
		addLogMessage("Increasing engraver power (500)");
		ebbControl.setEngraver(true, 500, true);
		ofSleepMillis(1000);

		// Turn off engraver
		addLogMessage("Turning off engraver");
		ebbControl.setEngraver(false, 0, true);

		addLogMessage("Engraver test completed successfully");
	} catch (const std::exception & e) {
		addLogMessage("Error in engraver test: " + std::string(e.what()));
		// Make sure engraver is off
		try {
			ebbControl.setEngraver(false, 0, true);
		} catch (...) { }
	}
}

//--------------------------------------------------------------
void ofApp::testServo() {
	addLogMessage("-- Testing Servo Control --");
	try {
		// Get current servo settings
		auto penDown = ebbControl.isPenDown();

		// Test direct servo control
		addLogMessage("Testing direct servo control");
		addLogMessage("Setting servo to middle position (7500)");
		ebbControl.servoOutput(7500, ofxEbbControl::SERVO_CHANNEL_PEN);
		ofSleepMillis(1000);

		addLogMessage("Setting servo to up position (10000)");
		ebbControl.servoOutput(10000, ofxEbbControl::SERVO_CHANNEL_PEN);
		ofSleepMillis(1000);

		addLogMessage("Setting servo to down position (5000)");
		ebbControl.servoOutput(5000, ofxEbbControl::SERVO_CHANNEL_PEN);
		ofSleepMillis(1000);

		// Restore original pen state
		if (!penDown) {
			ebbControl.setPenState(false);
		} else {
			ebbControl.setPenState(true);
		}

		addLogMessage("Servo test completed successfully");
	} catch (const std::exception & e) {
		addLogMessage("Error in servo test: " + std::string(e.what()));
	}
}

//--------------------------------------------------------------
void ofApp::testConfiguration() {
	addLogMessage("-- Testing Configuration --");
	try {
		// Test layer management
		int currentLayer = ebbControl.getLayer();
		addLogMessage("Current layer: " + std::to_string(currentLayer));

		addLogMessage("Setting layer to 5");
		ebbControl.setLayer(5);

		int newLayer = ebbControl.getLayer();
		addLogMessage("New layer: " + std::to_string(newLayer));

		// Restore original layer
		ebbControl.setLayer(currentLayer);

		// Test setting nickname
		std::string currentName = ebbControl.getNickname();
		addLogMessage("Current nickname: " + currentName);

		std::string testName = "TestEBB";
		addLogMessage("Setting nickname to: " + testName);
		ebbControl.setNickname(testName);

		std::string newName = ebbControl.getNickname();
		addLogMessage("New nickname: " + newName);

		// Restore original nickname
		ebbControl.setNickname(currentName);

		addLogMessage("Configuration test completed successfully");
	} catch (const std::exception & e) {
		addLogMessage("Error in configuration test: " + std::string(e.what()));
	}
}

//--------------------------------------------------------------
void ofApp::testQueryFunctions() {
	addLogMessage("-- Testing Query Functions --");
	try {
		// Query general status
		auto status = ebbControl.getGeneralStatus();
		addLogMessage("General status:");
		addLogMessage("  Pen down: " + std::string(status.penDown ? "yes" : "no"));
		addLogMessage("  Motor 1 moving: " + std::string(status.motor1 ? "yes" : "no"));
		addLogMessage("  Motor 2 moving: " + std::string(status.motor2 ? "yes" : "no"));
		addLogMessage("  Command executing: " + std::string(status.executing ? "yes" : "no"));
		addLogMessage("  FIFO empty: " + std::string(status.fifoEmpty ? "yes" : "no"));

		// Query motor status
		auto motorStatus = ebbControl.getMotorStatus();
		addLogMessage("Motor status:");
		addLogMessage("  Motor 1 moving: " + std::string(motorStatus.moving[0] ? "yes" : "no"));
		addLogMessage("  Motor 2 moving: " + std::string(motorStatus.moving[1] ? "yes" : "no"));
		addLogMessage("  Command executing: " + std::string(motorStatus.executing ? "yes" : "no"));

		// Query motor config
		auto motorConfig = ebbControl.getMotorConfig();
		addLogMessage("Motor configuration:");
		addLogMessage("  Motor 1 mode: " + std::to_string(motorConfig[0]));
		addLogMessage("  Motor 2 mode: " + std::to_string(motorConfig[1]));

		// Query current and voltage
		auto currentInfo = ebbControl.getCurrentInfo();
		addLogMessage("Current/Voltage readings:");
		addLogMessage("  Max current: " + std::to_string(currentInfo.maxCurrent) + " A");
		addLogMessage("  Power voltage: " + std::to_string(currentInfo.powerVoltage) + " V");

		addLogMessage("Query functions test completed successfully");
	} catch (const std::exception & e) {
		addLogMessage("Error in query functions test: " + std::string(e.what()));
	}
}

//--------------------------------------------------------------
void ofApp::runAllTests() {
	if (!isConnected) {
		addLogMessage("Not connected to EBB");
		return;
	}

	testRunning = true;

	// Run all tests in sequence
	addLogMessage("=== Starting All Tests ===");

	testMotorControl();
	ofSleepMillis(500);

	testPenControl();
	ofSleepMillis(500);

	testNodeCounter();
	ofSleepMillis(500);

	testEngraver();
	ofSleepMillis(500);

	testServo();
	ofSleepMillis(500);

	testConfiguration();
	ofSleepMillis(500);

	testQueryFunctions();

	addLogMessage("=== All Tests Completed ===");

	testRunning = false;
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key) {
}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y) {
}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button) {
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button) {
}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button) {
}

//--------------------------------------------------------------
void ofApp::mouseScrolled(int x, int y, float scrollX, float scrollY) {
}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y) {
}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y) {
}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h) {
}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg) {
}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo) {
}
