#pragma once

#include "ofMain.h"
#include "ofxEbbControl.h"

class ofApp : public ofBaseApp {

public:
	void setup() override;
	void update() override;
	void draw() override;
	void exit() override;

	void keyPressed(int key) override;
	void keyReleased(int key) override;
	void mouseMoved(int x, int y) override;
	void mouseDragged(int x, int y, int button) override;
	void mousePressed(int x, int y, int button) override;
	void mouseReleased(int x, int y, int button) override;
	void mouseScrolled(int x, int y, float scrollX, float scrollY) override;
	void mouseEntered(int x, int y) override;
	void mouseExited(int x, int y) override;
	void windowResized(int w, int h) override;
	void dragEvent(ofDragInfo dragInfo) override;
	void gotMessage(ofMessage msg) override;

	// EBB Control variables
	ofxEbbControl ebbControl;
	bool isConnected;
	std::string portName;
	std::vector<std::string> availablePorts;

	// Function to find and connect to EBB
	bool findAndConnect();

	// Test functions to demonstrate EBB control capabilities
	void testMotorControl();
	void testPenControl();
	void testNodeCounter();
	void testEngraver();
	void testServo();
	void testConfiguration();
	void testQueryFunctions();
	void runAllTests();

	// UI elements
	ofTrueTypeFont font;
	std::vector<std::string> logMessages;
	void addLogMessage(const std::string & message);

	// Current state
	bool testRunning;
	int currentTest;
	float testStartTime;
};
