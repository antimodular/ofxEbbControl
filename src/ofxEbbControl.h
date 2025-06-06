// ofxEbbControl.h
#pragma once

// Use the full path for OpenFrameworks headers
#include "ofLog.h"
#include "ofSerial.h"
#include "ofUtils.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// Utility function for clamping values (in case ofClamp is not available)
template <typename T>
T clampValue(const T & value, const T & min, const T & max) {
	return value < min ? min : (value > max ? max : value);
}

/**
 * ofxEbbControl: OpenFrameworks-style addon for EiBotBoard/EggBot control.
 * Wraps serial commands to the EBB firmware using ofSerial.
 */
class ofxEbbControl {
public:
	//-- Defaults & constants ----------------------------------------

	static constexpr int DEFAULT_BAUD = 115200;
	static constexpr int PEN_DOWN = 0;
	static constexpr int PEN_UP = 1;

	static constexpr int SERVO_CHANNEL_JP2 = 3;
	static constexpr int SERVO_CHANNEL_PEN = 4;
	static constexpr int SERVO_CHANNEL_JP1 = 4;
	static constexpr int SERVO_CHANNEL_JP3 = 5;
	static constexpr int SERVO_CHANNEL_JP4 = 6;

	static constexpr int MOTOR_DISABLE = 0;
	static constexpr int MOTOR_STEP_DIV16 = 1;
	static constexpr int MOTOR_STEP_DIV8 = 2;
	static constexpr int MOTOR_STEP_DIV4 = 3;
	static constexpr int MOTOR_STEP_DIV2 = 4;
	static constexpr int MOTOR_STEP_DIV1 = 5;

	static constexpr int SERVO_POWER_ON = 1;
	static constexpr int MODE_DIGITAL = 0;
	static constexpr int MODE_ANALOG = 1;

	//-- Construction/Destruction ------------------------------------

	/**
   * Default constructor.
   */
	ofxEbbControl() = default;

	/**
   * Destructor closes port if open.
   */
	~ofxEbbControl() {
		close();
	}

	//-- Connection Setup --------------------------------------------

	/**
   * Setup the serial connection to the EBB.
   * @param portName  Serial port identifier (e.g. "/dev/ttyUSB0").
   * @param baud      Baud rate (default DEFAULT_BAUD).
   * @returns         True if connection succeeded.
   */
	bool setup(
		const std::string & portName,
		int baud = DEFAULT_BAUD) {
		serialPort = portName;
		return serial.setup(portName, baud);
	}

	/**
   * Close the serial port if initialized.
   */
	void close() {
		if (serial.isInitialized()) {
			serial.close();
		}
	}

	//-- Low-Level Command I/O ---------------------------------------

	/**
   * Send a raw command to EBB, read a specified number of lines, or timeout.
   * This implementation handles the specific response patterns of the EBB controller.
   * @param cmd        Command string (without CR).
   * @param numLines   Number of response lines to read (default 1).
   * @param timeoutMs  Timeout in milliseconds (default 3000).
   * @returns          Concatenated response lines separated by newlines.
   * @throws           runtime_error if timeout or communication error.
   */
	std::string sendCommand(
		const std::string & cmd,
		int numLines = 1,
		int timeoutMs = 3000) {
		// First clear any existing data in the buffer
		unsigned char buffer[256];
		while (serial.available() > 0) {
			serial.readBytes(buffer, std::min(256, serial.available()));
		}

		// Send the command with CR
		std::string out = cmd + "\r";
		ofLogNotice("ofxEbbControl") << "Sending: '" << cmd << "'";
		serial.writeBytes(reinterpret_cast<const unsigned char *>(out.c_str()), out.size());

		// Wait a tiny bit for the command to be processed
		std::this_thread::sleep_for(std::chrono::milliseconds(10));

		// Special handling for certain commands
		// The 'V' command returns the version string without any OK
		if (cmd == "V") {
			// The V command returns the version string without any OK
			std::string responseBuffer;
			auto start = std::chrono::steady_clock::now();
			bool receivedResponse = false;

			// Give it some time to receive the complete version string
			while (!receivedResponse) {
				// Check for timeout
				auto now = std::chrono::steady_clock::now();
				if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() > timeoutMs) {
					ofLogError("ofxEbbControl") << "Command '" << cmd << "' timed out after " << timeoutMs << "ms";
					ofLogError("ofxEbbControl") << "Partial response: '" << responseBuffer << "'";
					throw std::runtime_error("Command '" + cmd + "' timed out");
				}

				// Read available data
				if (serial.available() > 0) {
					unsigned char ch;
					if (serial.readBytes(&ch, 1) > 0) {
						responseBuffer.push_back(static_cast<char>(ch));
					}
				} else {
					// No more data available and we've got some response
					if (!responseBuffer.empty() && std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() > 100) {
						receivedResponse = true; // We've waited long enough after receiving data
					} else {
						// Still waiting for data
						std::this_thread::sleep_for(std::chrono::milliseconds(1));
					}
				}
			}

			ofLogNotice("ofxEbbControl") << "Raw response: '" << responseBuffer << "'";
			return responseBuffer;
		}
		// Special handling for the QG command which returns a hex value without OK
		else if (cmd == "QG") {
			std::string responseBuffer;
			auto start = std::chrono::steady_clock::now();

			// Read response until we get a complete hex value (usually 2 characters)
			while (responseBuffer.length() < 2 || !isxdigit(responseBuffer[0]) || !isxdigit(responseBuffer[1])) {
				// Check for timeout
				auto now = std::chrono::steady_clock::now();
				if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() > timeoutMs) {
					ofLogError("ofxEbbControl") << "Command '" << cmd << "' timed out after " << timeoutMs << "ms";
					ofLogError("ofxEbbControl") << "Partial response: '" << responseBuffer << "'";
					throw std::runtime_error("Command '" + cmd + "' timed out");
				}

				// Read available data
				if (serial.available() > 0) {
					unsigned char ch;
					if (serial.readBytes(&ch, 1) > 0) {
						if (ch != '\r' && ch != '\n') { // Skip CR/LF
							responseBuffer.push_back(static_cast<char>(ch));
						}
					}
				} else {
					// Still waiting for data
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}

				// If we've collected some data but no more is coming, assume we're done
				if (!responseBuffer.empty() && serial.available() == 0 && std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() > 100) {
					break;
				}
			}

			ofLogNotice("ofxEbbControl") << "Raw response: '" << responseBuffer << "'";
			return responseBuffer;
		}
		// Special handling for QM command which returns data in format "QM,a,b,c,d\r\n"
		else if (cmd == "QM") {
			std::string responseBuffer;
			auto start = std::chrono::steady_clock::now();
			bool responseComplete = false;

			while (!responseComplete) {
				// Check for timeout
				auto now = std::chrono::steady_clock::now();
				if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() > timeoutMs) {
					ofLogError("ofxEbbControl") << "Command '" << cmd << "' timed out after " << timeoutMs << "ms";
					ofLogError("ofxEbbControl") << "Partial response: '" << responseBuffer << "'";
					throw std::runtime_error("Command '" + cmd + "' timed out");
				}

				// Read available data
				if (serial.available() > 0) {
					unsigned char ch;
					if (serial.readBytes(&ch, 1) > 0) {
						responseBuffer.push_back(static_cast<char>(ch));

						// Look for "\r\n" or "\n" as end of QM response
						if ((responseBuffer.length() >= 2 && responseBuffer[responseBuffer.length() - 2] == '\r' && responseBuffer[responseBuffer.length() - 1] == '\n') || (responseBuffer.length() >= 1 && responseBuffer[responseBuffer.length() - 1] == '\n')) {
							responseComplete = true;
						}
					}
				} else {
					// No data available right now

					// If we've already received "QM," and have at least 7 characters (QM,0,0,0,0)
					// and there's been no data for a while, consider the response complete
					if (responseBuffer.find("QM,") == 0 && responseBuffer.length() >= 7 && std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() > 100) {
						responseComplete = true;
					} else {
						// Keep waiting for more data
						std::this_thread::sleep_for(std::chrono::milliseconds(1));
					}
				}
			}

			// Clean up the response - remove CR/LF and trim whitespace
			responseBuffer.erase(std::remove(responseBuffer.begin(), responseBuffer.end(), '\r'), responseBuffer.end());
			responseBuffer.erase(std::remove(responseBuffer.begin(), responseBuffer.end(), '\n'), responseBuffer.end());

			ofLogNotice("ofxEbbControl") << "Raw response: '" << responseBuffer << "'";
			return responseBuffer;
		}

		// For all other commands, read the entire response into a buffer
		std::string responseBuffer;
		auto start = std::chrono::steady_clock::now();
		bool foundOK = false;

		// Read response until we have the OK marker or timeout
		while (!foundOK) {
			// Check for timeout
			auto now = std::chrono::steady_clock::now();
			if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() > timeoutMs) {
				ofLogError("ofxEbbControl") << "Command '" << cmd << "' timed out after " << timeoutMs << "ms";
				ofLogError("ofxEbbControl") << "Partial response: '" << responseBuffer << "'";
				throw std::runtime_error("Command '" + cmd + "' timed out");
			}

			// Read available data one byte at a time for more careful control
			unsigned char ch;
			if (serial.available() > 0) {
				if (serial.readBytes(&ch, 1) > 0) {
					responseBuffer.push_back(static_cast<char>(ch));

					// Check for OK in the response
					if (responseBuffer.size() >= 2) {
						size_t pos = responseBuffer.find("OK");
						if (pos != std::string::npos) {
							foundOK = true;
						}
					}
				}
			} else {
				// No data available, wait a tiny bit
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}

		ofLogNotice("ofxEbbControl") << "Raw response: '" << responseBuffer << "'";

		// Process the response based on the command
		if (cmd.substr(0, 2) == "QP") {
			// QP returns pen status (0=down, 1=up)
			// Format might be "0OK" or "1OK" with no separation
			if (responseBuffer.find("0OK") != std::string::npos) {
				return "0"; // Pen down
			} else {
				return "1"; // Pen up
			}
		} else if (cmd.substr(0, 2) == "QS") {
			// QS returns step positions, typically "0,0OK"
			std::string stepPos = responseBuffer;
			size_t okPos = stepPos.find("OK");
			if (okPos != std::string::npos) {
				stepPos = stepPos.substr(0, okPos);
			}

			// Strip any non-numeric/comma characters
			std::string cleaned;
			for (char c : stepPos) {
				if (isdigit(c) || c == ',' || c == '-') {
					cleaned += c;
				}
			}

			return cleaned;
		} else if (cmd.substr(0, 2) == "QT") {
			// QT returns nickname
			std::string nickname = responseBuffer;
			size_t okPos = nickname.find("OK");
			if (okPos != std::string::npos) {
				nickname = nickname.substr(0, okPos);
			}

			// Clean up the nickname
			nickname.erase(std::remove_if(nickname.begin(), nickname.end(),
							   [](unsigned char c) { return c == '\r' || c == '\n'; }),
				nickname.end());

			if (nickname.empty()) {
				return "EBB Controller";
			}

			return nickname;
		} else if (cmd.substr(0, 2) == "QB") {
			// QB returns button status (0=not pressed, 1=pressed)
			if (responseBuffer.find("1OK") != std::string::npos) {
				return "1";
			} else {
				return "0";
			}
		} else if (cmd.substr(0, 2) == "QC") {
			// QC returns current and voltage readings
			std::string values = responseBuffer;
			size_t okPos = values.find("OK");
			if (okPos != std::string::npos) {
				values = values.substr(0, okPos);
			}

			// Clean up the response to just get the values
			std::string cleaned;
			for (char c : values) {
				if (isdigit(c) || c == ',') {
					cleaned += c;
				}
			}

			return cleaned;
		} else if (cmd.substr(0, 2) == "QR") {
			// QR returns servo power status
			if (responseBuffer.find("1OK") != std::string::npos) {
				return "1";
			} else {
				return "0";
			}
		} else if (cmd.substr(0, 2) == "QN") {
			// QN returns node count
			std::string nodeCount = responseBuffer;
			size_t okPos = nodeCount.find("OK");
			if (okPos != std::string::npos) {
				nodeCount = nodeCount.substr(0, okPos);
			}

			// Clean up the node count
			nodeCount.erase(std::remove_if(nodeCount.begin(), nodeCount.end(),
								[](unsigned char c) { return !isdigit(c); }),
				nodeCount.end());

			return nodeCount;
		} else if (responseBuffer == "OK" || responseBuffer.find("OK") != std::string::npos) {
			// For commands that just return OK
			return "OK";
		}

		// For any other response, return as-is
		return responseBuffer;
	}

	// Helper method to clear any data in the serial buffer
	void drainSerialBuffer() {
		unsigned char buffer[256];
		while (serial.available() > 0) {
			serial.readBytes(buffer, std::min(256, serial.available()));
		}
	}

	//-- Public API Methods ------------------------------------------

	/**
   * Retrieve all analog input readings.
   * @returns Map from channel number to value (0-1023).
   */
	std::map<int, int> getAnalogValues() {
		auto resp = sendCommand("A");
		auto parts = split(resp, ',');
		std::map<int, int> values;

		for (size_t i = 1; i < parts.size(); ++i) {
			auto kv = split(parts[i], ':');
			int channel = std::stoi(kv[0]);
			int value = std::stoi(kv[1]);
			values[channel] = value;
		}

		return values;
	}

	/**
   * Enable or disable an analog input channel.
   * @param channel  Channel index 0-15.
   * @param enable   True to enable, false to disable.
   */
	void configureAnalogInput(
		int channel,
		bool enable) {
		if (channel < 0 || channel > 15) {
			throw std::runtime_error(
				"Analog channel out of range");
		}

		auto cmd = "AC," + toStr(channel) + "," + toStr(enable);
		auto resp = sendCommand(cmd);
		checkOk(resp);
	}

	/**
   * Enter bootloader mode; EBB disconnects and re-enumerates.
   */
	void enterBootloader() {
		sendCommand("BL");
		close();
	}

	/**
   * Configure direction (TRIS) for ports A-E.
   * @param tris  Array of 5 values (0-255) for TRISA..TRISE.
   */
	void configurePinDirections(
		const std::array<int, 5> & tris) {
		for (auto v : tris) {
			assertByte(v);
		}

		auto cmd = "C," + join(tris, ',');
		auto resp = sendCommand(cmd);
		checkOk(resp);
	}

	/**
   * Clear (zero) the step position counters.
   */
	void clearStepPosition() {
		checkOk(sendCommand("CS"));
	}

	/**
   * Configure user interface options.
   * @param okResponses      Send OK after each command if true.
   * @param paramCheck       Enable parameter limit checking if true.
   * @param fifoLedIndicator Light LED when FIFO empty if true.
   */
	void setUserOptions(
		bool okResponses,
		bool paramCheck,
		bool fifoLedIndicator) {
		sendCommand("CU,1," + toStr(okResponses));
		sendCommand("CU,2," + toStr(paramCheck));
		sendCommand("CU,3," + toStr(fifoLedIndicator));
	}

	/**
   * Enable or disable stepper motors and set microstep mode.
   * @param m1Mode Mode for motor 1 (0-5).
   * @param m2Mode Mode for motor 2 (0-5).
   */
	void enableMotors(
		int m1Mode,
		int m2Mode) {
		// Store the configuration for future reference by getMotorConfig()
		static std::array<int, 2> & lastKnownConfig = getLastKnownMotorConfig();
		lastKnownConfig = { m1Mode, m2Mode };

		auto cmd = "EM," + toStr(m1Mode) + "," + toStr(m2Mode);
		auto resp = sendCommand(cmd);
		checkOk(resp);
	}

	/**
   * Emergency stop: abort motion and optionally disable motors.
   * @param disableMotors  True to de-energize motors after stop.
   * @returns StopInfo with FIFO and remaining steps data.
   */
	struct StopInfo {
		bool interrupted;
		std::array<int, 2> fifo;
		std::array<int, 2> remaining;
	};
	StopInfo emergencyStop(
		bool disableMotors = false) {
		try {
			auto cmd = disableMotors ? "ES,1" : "ES";
			auto resp = sendCommand(cmd, 2);

			// Extract the emergency stop data from the response
			std::string stopData;
			if (resp.find("OK") != std::string::npos) {
				stopData = resp.substr(0, resp.find("OK"));
			} else {
				stopData = resp;
			}

			// Clean up and parse the values
			std::string cleaned;
			for (char c : stopData) {
				if (isdigit(c) || c == ',' || c == '-') {
					cleaned += c;
				}
			}

			auto vals = split(cleaned, ',');
			if (vals.size() < 5) {
				throw std::runtime_error("Invalid emergency stop response");
			}

			return StopInfo {
				vals[0] == "1",
				{ std::stoi(vals[1]), std::stoi(vals[2]) },
				{ std::stoi(vals[3]), std::stoi(vals[4]) }
			};
		} catch (const std::exception & e) {
			ofLogError("ofxEbbControl") << "Error in emergency stop: " << e.what();
			return StopInfo { false, { 0, 0 }, { 0, 0 } };
		}
	}

	/**
   * Perform an absolute move relative to home.
   * @param stepFrequency  Step frequency in Hz (2-25000).
   * @param pos1           Motor1 position.
   * @param pos2           Motor2 position.
   */
	void moveAbsolute(
		int stepFrequency,
		int pos1 = 0,
		int pos2 = 0) {
		if (stepFrequency < 2 || stepFrequency > 25000) {
			throw std::runtime_error(
				"Frequency out of range");
		}

		auto cmd = "HM," + toStr(stepFrequency) + "," + toStr(pos1) + "," + toStr(pos2);
		auto resp = sendCommand(cmd);
		checkOk(resp);
	}

	/**
   * Read digital inputs from ports A-E.
   * @returns Array of 5 port values.
   */
	std::array<int, 5> getDigitalInputs() {
		auto resp = sendCommand("I");
		if (resp.empty() || resp[0] != 'I') {
			throw std::runtime_error("Bad input response");
		}

		auto parts = split(resp, ',');
		std::array<int, 5> out;
		for (int i = 0; i < 5; ++i) {
			out[i] = std::stoi(parts[i + 1]);
		}
		return out;
	}

	/**
   * Low-level step-limited move (LM).
   */
	void moveLowLevel(
		int64_t rate1,
		int64_t steps1,
		int64_t accel1,
		bool clear1,
		int64_t rate2,
		int64_t steps2,
		int64_t accel2,
		bool clear2) {
		int clearMask = (clear2 ? 2 : 0) | (clear1 ? 1 : 0);
		auto cmd = "LM," + joinInt({ rate1, steps1, accel1, rate2, steps2, accel2, clearMask });
		auto resp = sendCommand(cmd);
		checkOk(resp);
	}

	/**
   * Low-level time-limited move (LT).
   */
	void moveTimed(
		int64_t intervals,
		int64_t rate1,
		int64_t accel1,
		bool clear1,
		int64_t rate2,
		int64_t accel2,
		bool clear2) {
		int clearMask = (clear2 ? 2 : 0) | (clear1 ? 1 : 0);
		auto cmd = "LT," + joinInt({ intervals, rate1, accel1, rate2, accel2, clearMask });
		auto resp = sendCommand(cmd);
		checkOk(resp);
	}

	/**
   * Read a byte from memory (MR).
   */
	uint8_t readMemory(
		int address) {
		if (address < 0 || address > 4095) {
			throw std::runtime_error("Memory address out of range");
		}

		auto resp = sendCommand("MR," + toStr(address));
		auto parts = split(resp, ',');
		if (parts[0] != "MR") {
			throw std::runtime_error("Bad MR response");
		}
		return static_cast<uint8_t>(std::stoi(parts[1]));
	}

	/**
   * Write a byte to memory (MW).
   */
	void writeMemory(
		int address,
		int value) {
		if (address < 0 || address > 4095) {
			throw std::runtime_error("Memory address out of range");
		}
		assertByte(value);

		auto cmd = "MW," + toStr(address) + "," + toStr(value);
		auto resp = sendCommand(cmd);
		checkOk(resp);
	}

	/**
   * Decrement the node counter (ND).
   */
	void decrementNodeCount() {
		checkOk(sendCommand("ND"));
	}

	/**
   * Increment the node counter (NI).
   */
	void incrementNodeCount() {
		checkOk(sendCommand("NI"));
	}

	/**
   * Set digital outputs on ports A-E (O).
   */
	void setDigitalOutputs(
		const std::array<int, 5> & outputs) {
		for (auto v : outputs) {
			assertByte(v);
		}

		auto cmd = "O," + join(outputs, ',');
		auto resp = sendCommand(cmd);
		checkOk(resp);
	}

	/**
   * Configure pulse generator parameters (PC).
   */
	void configurePulse(
		const std::array<int, 8> & params) {
		auto cmd = "PC," + join(params, ',');
		auto resp = sendCommand(cmd);
		checkOk(resp);
	}

	/**
   * Set pin direction (PD): input/output.
   */
	void setPinMode(
		char port,
		int pin,
		bool output) {
		assertPort(port);
		if (pin < 0 || pin > 7) {
			throw std::runtime_error("Pin index out of range");
		}

		std::string cmd = stringize(
			"PD,%c,%d,%d", port, pin, output ? 0 : 1);
		checkOk(sendCommand(cmd));
	}

	/**
   * Start or stop pulse generation (PG).
   */
	void pulseStart(
		bool enable) {
		auto cmd = "PG," + toStr(enable);
		auto resp = sendCommand(cmd);
		checkOk(resp);
	}

	/**
   * Read a pin input state (PI).
   */
	bool getPin(
		char port,
		int pin) {
		assertPort(port);
		if (pin < 0 || pin > 7) {
			throw std::runtime_error("Pin index out of range");
		}

		std::string cmd = stringize(
			"PI,%c,%d", port, pin);
		auto resp = sendCommand(cmd);
		auto parts = split(resp, ',');
		if (parts[0] != "PI") {
			throw std::runtime_error("Bad PI response");
		}
		return parts[1] == "1";
	}

	/**
   * Write a digital pin output (PO).
   */
	void setPin(
		char port,
		int pin,
		bool high) {
		assertPort(port);
		if (pin < 0 || pin > 7) {
			throw std::runtime_error("Pin index out of range");
		}

		std::string cmd = stringize(
			"PO,%c,%d,%d", port, pin, high ? 1 : 0);
		checkOk(sendCommand(cmd));
	}

	/**
   * Query if the hardware button was pressed (QB).
   * @returns True if pressed since last query.
   */
	bool isButtonPressed() {
		try {
			auto resp = sendCommand("QB", 2);

			// The QB command should return "0" or "1" followed by "OK"
			if (resp.find("1") != std::string::npos) {
				return true; // Button was pressed
			} else {
				return false; // Button was not pressed
			}
		} catch (const std::exception & e) {
			ofLogError("ofxEbbControl") << "Error checking button status: " << e.what();
			return false;
		}
	}

	/**
   * Read current and power voltage (QC).
   */
	struct CurrentInfo {
		double maxCurrent;
		double powerVoltage;
	};
	CurrentInfo getCurrentInfo(
		bool oldBoard = false) {
		try {
			auto resp = sendCommand("QC", 2);

			// Extract the current/voltage values from the response
			std::string values;
			if (resp.find("OK") != std::string::npos) {
				// Get everything before "OK"
				values = resp.substr(0, resp.find("OK"));
			} else {
				values = resp;
			}

			// Clean up and parse the values
			std::string cleaned;
			for (char c : values) {
				if (isdigit(c) || c == ',') {
					cleaned += c;
				}
			}

			auto parts = split(cleaned, ',');
			if (parts.size() < 2) {
				throw std::runtime_error("Invalid current info response");
			}

			double ra0 = 3.3 * std::stoi(parts[0]) / 1023.0;
			double vp = 3.3 * std::stoi(parts[1]) / 1023.0;
			double scale = oldBoard ? (1.0 / 11.0) : (1.0 / 9.2);

			return {
				ra0 / 1.76,
				vp / scale + 0.3
			};
		} catch (const std::exception & e) {
			ofLogError("ofxEbbControl") << "Error getting current info: " << e.what();
			return { 0.0, 0.0 }; // Default values
		}
	}

	/**
   * Query the EBB firmware version (QV).
   */
	std::string getFirmwareVersion() {
		std::string resp = sendCommand("V");
		return resp;
	}

	/**
   * Query motor step mode config.
   * Note: The QE command is only available in firmware v2.8.0 and newer.
   * For compatibility, this implementation uses the last known values
   * set with enableMotors().
   */
	std::array<int, 2> getMotorConfig() {
		try {
			// Store the last used configuration to support all firmware versions
			static std::array<int, 2> lastKnownConfig = { MOTOR_STEP_DIV16, MOTOR_STEP_DIV16 };

			// Try to query the motor status using QM command
			// This is more broadly supported than QE and can indicate if motors are enabled
			auto resp = sendCommand("QM", 1, 1000); // Shorter timeout for better user experience
			auto parts = split(resp, ',');

			if (parts.size() >= 4) {
				// QM returns motor status but not microstep mode
				// We can at least detect if motors are enabled/disabled
				bool motor1Enabled = parts[2] == "1";
				bool motor2Enabled = parts[3] == "1";

				// If motors are disabled, update our cached values
				if (!motor1Enabled) {
					lastKnownConfig[0] = MOTOR_DISABLE;
				}
				if (!motor2Enabled) {
					lastKnownConfig[1] = MOTOR_DISABLE;
				}
			}

			return lastKnownConfig;
		} catch (const std::exception & e) {
			// Return default values if there's an error
			return { MOTOR_STEP_DIV16, MOTOR_STEP_DIV16 };
		}
	}

	/**
   * Query general status flags (QG).
   */
	struct GeneralStatus {
		bool pinRB5;
		bool pinRB2;
		bool buttonPrg;
		bool penDown;
		bool executing;
		bool motor1;
		bool motor2;
		bool fifoEmpty;
	};
	GeneralStatus getGeneralStatus() {
		auto resp = sendCommand("QG");
		int statusByte = std::stoi(resp, nullptr, 16);

		auto bitSet = [&](int b) {
			return (statusByte & (1 << b)) != 0;
		};

		return {
			bitSet(7), bitSet(6), bitSet(5), bitSet(4),
			bitSet(3), bitSet(2), bitSet(1), !bitSet(0)
		};
	}

	/**
   * Query current layer value (QL).
   */
	int getLayer() {
		try {
			auto resp = sendCommand("QL", 2);

			// Extract layer value from response
			std::string layerValue;
			if (resp.find("OK") != std::string::npos) {
				layerValue = resp.substr(0, resp.find("OK"));
			} else {
				layerValue = resp;
			}

			// Clean up and parse the value
			layerValue.erase(std::remove_if(layerValue.begin(), layerValue.end(),
								 [](unsigned char c) { return !isdigit(c); }),
				layerValue.end());

			if (layerValue.empty()) {
				return 0;
			}

			return std::stoi(layerValue);
		} catch (const std::exception & e) {
			ofLogError("ofxEbbControl") << "Error getting layer: " << e.what();
			return 0;
		}
	}

	/**
   * Query motor FIFO and motion status (QM).
   */
	struct MotorStatus {
		bool executing;
		std::array<bool, 2> moving;
		bool fifoEmpty;
	};
	MotorStatus getMotorStatus() {
		auto resp = sendCommand("QM");
		auto parts = split(resp, ',');
		return {
			std::stoi(parts[1]) > 0,
			{ parts[2] == "1", parts[3] == "1" },
			std::stoi(parts[4]) == 0
		};
	}

	/**
   * Query pen state (QP).
   * @returns True if pen is in down position (0), false if up (1).
   */
	bool isPenDown() {
		try {
			auto resp = sendCommand("QP", 2);

			// Look for "0" or "1" in the response
			if (resp.find("0") != std::string::npos && resp.find("OK") != std::string::npos) {
				return true; // Pen is down (0)
			} else {
				return false; // Pen is up (1) or we couldn't determine
			}
		} catch (const std::exception & e) {
			ofLogError("ofxEbbControl") << "Error querying pen state: " << e.what();
			return false; // Default to assuming pen is up (safer)
		}
	}

	/**
   * Query servo power status (QR).
   */
	bool isServoPowered() {
		try {
			auto resp = sendCommand("QR", 2);

			// The QR command returns 0 or 1
			if (resp.find("1") != std::string::npos) {
				return true; // Servo power is on
			} else {
				return false; // Servo power is off
			}
		} catch (const std::exception & e) {
			ofLogError("ofxEbbControl") << "Error querying servo power: " << e.what();
			return false; // Default to off
		}
	}

	/**
   * Query current step positions (QS).
   */
	std::array<int, 2> getStepPositions() {
		try {
			auto resp = sendCommand("QS", 2);

			// Extract the step positions from the response (format "0,0OK")
			std::string stepPos;
			if (resp.find("OK") != std::string::npos) {
				stepPos = resp.substr(0, resp.find("OK"));
			} else {
				stepPos = resp;
			}

			// Clean up and parse values
			std::string cleaned;
			for (char c : stepPos) {
				if (isdigit(c) || c == ',' || c == '-') {
					cleaned += c;
				}
			}

			auto values = split(cleaned, ',');
			if (values.size() >= 2) {
				return { std::stoi(values[0]), std::stoi(values[1]) };
			} else {
				return { 0, 0 };
			}
		} catch (const std::exception & e) {
			ofLogError("ofxEbbControl") << "Error querying step positions: " << e.what();
			return { 0, 0 };
		}
	}

	/**
   * Query EBB nickname (QT).
   */
	std::string getNickname() {
		try {
			auto resp = sendCommand("QT", 2);

			// Extract nickname from response
			std::string nickname = resp;
			size_t okPos = nickname.find("OK");
			if (okPos != std::string::npos) {
				nickname = nickname.substr(0, okPos);
			}

			// Clean up the nickname
			nickname.erase(std::remove_if(nickname.begin(), nickname.end(),
							   [](unsigned char c) { return c == '\r' || c == '\n'; }),
				nickname.end());

			if (nickname.empty()) {
				return "EBB Controller";
			}

			return nickname;
		} catch (const std::exception & e) {
			ofLogError("ofxEbbControl") << "Error getting nickname: " << e.what();
			return "EBB Controller";
		}
	}

	/**
   * Set this EBB's nickname.
   * See {@link https://evil-mad.github.io/EggBot/ebb.html#ST}.
   * @param nickname New nickname (maximum 16 characters).
   * @returns True if successful.
   */
	bool setNickname(const std::string & nickname) {
		try {
			if (nickname.length() > 16) {
				ofLogWarning("ofxEbbControl") << "Nickname too long, truncating to 16 characters";
			}

			// Truncate to 16 characters if needed
			std::string truncated = nickname.substr(0, 16);

			auto cmd = "ST," + truncated;
			auto resp = sendCommand(cmd);
			return resp == "OK";
		} catch (const std::exception & e) {
			ofLogError("ofxEbbControl") << "Error setting nickname: " << e.what();
			return false;
		}
	}

	/**
   * Reboot EBB (RB).
   */
	void reboot() {
		sendCommand("RB");
		close();
	}

	/**
   * Reset EBB (R).
   */
	void reset() {
		auto resp = sendCommand("R");
		checkOk(resp);
	}

	/**
   * Mixed-axis stepper move (XM).
   */
	void moveStepperMixedAxis(
		int durationMs,
		int stepsA,
		int stepsB) {
		if (durationMs < 1 || durationMs > (1 << 24) - 1) {
			throw std::runtime_error("Duration out of range");
		}

		if (std::abs(stepsA) > (1 << 24) - 1 || std::abs(stepsB) > (1 << 24) - 1) {
			throw std::runtime_error("Steps out of range");
		}

		auto cmd = "XM," + toStr(durationMs) + "," + toStr(stepsA) + "," + toStr(stepsB);

		auto resp = sendCommand(cmd);
		checkOk(resp);
	}

	/**
   * Timed read of pins (T).
   */
	void timedRead(
		int duration,
		bool digitalMode) {
		if (duration < 1 || duration >= (1 << 16)) {
			throw std::runtime_error("Duration out of range");
		}

		int mode = digitalMode ? MODE_DIGITAL : MODE_ANALOG;
		auto cmd = "T," + toStr(duration) + "," + toStr(mode);
		auto resp = sendCommand(cmd);
		checkOk(resp);
	}

	/**
   * Toggle pen up/down (TP).
   */
	void togglePen(
		int durationMs = -1) {
		std::string cmd;
		if (durationMs >= 0) {
			cmd = "TP," + toStr(durationMs);
		} else {
			cmd = "TP";
		}

		auto resp = sendCommand(cmd);
		checkOk(resp);
	}

	/**
   * Set pen state with optional duration and pin (SP).
   */
	void setPenState(
		bool down,
		int duration = -1,
		int pin = -1) {
		std::ostringstream os;
		os << "SP," << (down ? PEN_DOWN : PEN_UP);

		if (duration >= 0) {
			os << "," << duration;
		}
		if (pin >= 0) {
			os << "," << pin;
		}

		auto resp = sendCommand(os.str());
		checkOk(resp);
	}

	/**
   * Configure stepper and servo modes (SC).
   * @param paramIndex The parameter index to set (1, 2, 4, 5, 8, 9, 10, 11, 12, 13)
   * @param paramValue The value to set (varies by parameter)
   *
   * Parameter indices:
   * 1: Pen lift mechanism (0-2)
   * 2: Stepper signal control (0-2)
   * 4: Servo min position (1-65535)
   * 5: Servo max position (1-65535)
   * 8: Number of RC channels (1-24)
   * 9: S2 channel duration (1-6)
   * 10: Default servo rate (0-65535)
   * 11: Servo rate up (0-65535)
   * 12: Servo rate down (0-65535)
   * 13: Alternate pause button function (0-1)
   */
	void stepperAndServoModeConfigure(
		int paramIndex,
		int paramValue) {
		std::string cmd;

		switch (paramIndex) {
		case 1: // Pen lift mechanism
			if (paramValue < 0 || paramValue > 2) {
				throw std::runtime_error("Parameter value must be between 0 and 2");
			}
			cmd = "SC,1," + toStr(paramValue);
			break;

		case 2: // Stepper signal control
			if (paramValue < 0 || paramValue > 2) {
				throw std::runtime_error("Parameter value must be between 0 and 2");
			}
			cmd = "SC,2," + toStr(paramValue);
			break;

		case 4: // Servo min
			if (paramValue < 1 || paramValue > 65535) {
				throw std::runtime_error("Parameter value must be between 1 and 65535");
			}
			cmd = "SC,4," + toStr(paramValue);
			break;

		case 5: // Servo max
			if (paramValue < 1 || paramValue > 65535) {
				throw std::runtime_error("Parameter value must be between 1 and 65535");
			}
			cmd = "SC,5," + toStr(paramValue);
			break;

		case 8: // Number of RC channels
			if (paramValue < 1 || paramValue > 24) {
				throw std::runtime_error("Parameter value must be between 1 and 24");
			}
			cmd = "SC,8," + toStr(paramValue);
			break;

		case 9: // S2 channel duration
			if (paramValue < 1 || paramValue > 6) {
				throw std::runtime_error("Parameter value must be between 1 and 6");
			}
			cmd = "SC,9," + toStr(paramValue);
			break;

		case 10: // Default servo rate
			if (paramValue < 0 || paramValue > 65535) {
				throw std::runtime_error("Parameter value must be between 0 and 65535");
			}
			cmd = "SC,10," + toStr(paramValue);
			break;

		case 11: // Servo rate up
			if (paramValue < 0 || paramValue > 65535) {
				throw std::runtime_error("Parameter value must be between 0 and 65535");
			}
			cmd = "SC,11," + toStr(paramValue);
			break;

		case 12: // Servo rate down
			if (paramValue < 0 || paramValue > 65535) {
				throw std::runtime_error("Parameter value must be between 0 and 65535");
			}
			cmd = "SC,12," + toStr(paramValue);
			break;

		case 13: // Alternate pause button function
			if (paramValue < 0 || paramValue > 1) {
				throw std::runtime_error("Parameter value must be between 0 and 1");
			}
			cmd = "SC,13," + toStr(paramValue);
			break;

		default:
			throw std::runtime_error("Parameter index " + toStr(paramIndex) + " not allowed");
		}

		auto resp = sendCommand(cmd);
		checkOk(resp);
	}

	/**
   * List available serial devices.
   * @returns A vector of device paths that might be EBB controllers.
   */
	std::vector<std::string> listDevices() {
		std::vector<std::string> devices;
		auto deviceList = serial.getDeviceList();

		for (auto & device : deviceList) {
			devices.push_back(device.getDevicePath());
		}

		return devices;
	}

	/**
   * Disable both stepper motors.
   * @returns True if successful.
   */
	bool disableMotors() {
		try {
			enableMotors(MOTOR_DISABLE, MOTOR_DISABLE);
			return true;
		} catch (const std::exception & e) {
			ofLogError("ofxEbbControl") << "Error disabling motors: " << e.what();
			return false;
		}
	}

	/**
   * Move stepper motors with simple step commands.
   * @param durationMs Duration of movement in milliseconds.
   * @param steps1 Steps for motor 1.
   * @param steps2 Steps for motor 2.
   * @returns True if successful.
   */
	bool moveStepperSteps(int durationMs, int steps1, int steps2) {
		try {
			std::string cmd = "SM," + toStr(durationMs) + "," + toStr(steps1) + "," + toStr(steps2);
			auto resp = sendCommand(cmd);
			return resp == "OK";
		} catch (const std::exception & e) {
			ofLogError("ofxEbbControl") << "Error in stepper move: " << e.what();
			return false;
		}
	}

	/**
   * Set servo power timeout.
   * @param durationMs Timeout in milliseconds (0 = no timeout).
   * @param powerOn Whether to enable power now.
   * @returns True if successful.
   */
	bool setServoPowerTimeout(int durationMs, bool powerOn) {
		try {
			auto cmd = "SR," + toStr(durationMs) + "," + toStr(powerOn ? 1 : 0);
			auto resp = sendCommand(cmd);
			return resp == "OK";
		} catch (const std::exception & e) {
			ofLogError("ofxEbbControl") << "Error setting servo power timeout: " << e.what();
			return false;
		}
	}

	/**
   * Get current node count.
   * @returns Current node count value.
   */
	uint32_t getNodeCount() {
		try {
			auto resp = sendCommand("QN", 2);
			// Remove the "OK" from the response
			if (resp.find("OK") != std::string::npos) {
				resp = resp.substr(0, resp.find("OK"));
			}

			// Clean the string to contain only numbers
			std::string cleaned;
			for (char c : resp) {
				if (isdigit(c)) {
					cleaned += c;
				}
			}

			if (cleaned.empty()) {
				return 0;
			}

			return static_cast<uint32_t>(std::stoul(cleaned));
		} catch (const std::exception & e) {
			ofLogError("ofxEbbControl") << "Error getting node count: " << e.what();
			return 0;
		}
	}

	/**
   * Set node counter value.
   * @param value New count value (0-2^32).
   * @returns True if successful.
   */
	bool setNodeCount(uint32_t value) {
		try {
			auto cmd = "SN," + toStr(value);
			auto resp = sendCommand(cmd);
			return resp == "OK";
		} catch (const std::exception & e) {
			ofLogError("ofxEbbControl") << "Error setting node count: " << e.what();
			return false;
		}
	}

	/**
   * Set engraver state and power.
   * @param enable True to enable, false to disable.
   * @param power Power level (0-1023).
   * @param useMotionQueue Whether to use motion queue for this command.
   * @returns True if successful.
   */
	bool setEngraver(bool enable, int power, bool useMotionQueue) {
		try {
			if (power < 0 || power > 1023) {
				power = ofClamp(power, 0, 1023);
			}

			auto cmd = "SE," + toStr(enable ? 1 : 0) + "," + toStr(power) + "," + toStr(useMotionQueue ? 1 : 0);
			auto resp = sendCommand(cmd);
			return resp == "OK";
		} catch (const std::exception & e) {
			ofLogError("ofxEbbControl") << "Error setting engraver: " << e.what();
			return false;
		}
	}

	/**
   * Direct servo output control.
   * @param position Servo position (0-65535, typically 5000-10000 range is usable).
   * @param servoChannel Servo channel to control.
   * @param rate Optional rate parameter for servo movement.
   * @param delay Optional delay in milliseconds.
   * @returns True if successful.
   */
	bool servoOutput(int position, int servoChannel, int rate = 0, int delay = 0) {
		try {
			std::string cmd = "S2," + toStr(position) + "," + toStr(servoChannel);

			if (rate > 0) {
				cmd += "," + toStr(rate);
				if (delay > 0) {
					cmd += "," + toStr(delay);
				}
			}

			auto resp = sendCommand(cmd);
			return resp == "OK";
		} catch (const std::exception & e) {
			ofLogError("ofxEbbControl") << "Error in servo output: " << e.what();
			return false;
		}
	}

	/**
   * Set layer value.
   * @param layer Layer value (0-127).
   * @returns True if successful.
   */
	bool setLayer(int layer) {
		try {
			if (layer < 0 || layer > 127) {
				layer = ofClamp(layer, 0, 127);
			}

			auto cmd = "SL," + toStr(layer);
			auto resp = sendCommand(cmd);
			return resp == "OK";
		} catch (const std::exception & e) {
			ofLogError("ofxEbbControl") << "Error setting layer: " << e.what();
			return false;
		}
	}

	// Get steps per mm calibration
	float getStepsPerMm() const {
		return stepsPerMm;
	}

private:
	// Serial connection
	ofSerial serial;
	std::string serialPort;

	// Configuration values
	float stepsPerMm = 80.0f;

	//-- Internal helpers -------------------------------------------

	static void assertByte(int v) {
		if (v < 0 || v > 255) {
			throw std::runtime_error("Byte value must be 0-255");
		}
	}

	static void assertPort(char p) {
		if (std::string("ABCDE").find(p) == std::string::npos) {
			throw std::runtime_error("Port letter must be A-E");
		}
	}

	static void checkOk(const std::string & r) {
		if (r != "OK") {
			throw std::runtime_error("Unexpected response: " + r);
		}
	}

	static void checkStatus(const std::vector<std::string> & lines) {
		if (lines.size() < 2) {
			throw std::runtime_error("Incomplete response, expected at least 2 lines");
		}

		if (lines[1] != "OK") {
			std::string errorMsg = "Bad status response: " + lines[1];
			if (!lines.empty()) {
				errorMsg += " (data: " + lines[0] + ")";
			}
			throw std::runtime_error(errorMsg);
		}
	}

	static std::string toStr(int v) {
		return std::to_string(v);
	}
	static std::string toStr(bool b) {
		return b ? "1" : "0";
	}
	static std::string toStr(uint32_t v) {
		return std::to_string(v);
	}

	template <typename T>
	static std::string join(
		const std::vector<T> & vals,
		char delim) {
		std::ostringstream os;
		for (size_t i = 0; i < vals.size(); ++i) {
			if (i > 0)
				os << delim;
			os << vals[i];
		}
		return os.str();
	}

	template <typename T, size_t N>
	static std::string join(
		const std::array<T, N> & vals,
		char delim) {
		std::ostringstream os;
		for (size_t i = 0; i < vals.size(); ++i) {
			if (i > 0)
				os << delim;
			os << vals[i];
		}
		return os.str();
	}

	static std::string joinInt(
		const std::vector<int64_t> & vals) {
		return join<int64_t>(vals, ',');
	}

	static std::vector<std::string> split(
		const std::string & s,
		char delim) {
		std::vector<std::string> out;
		std::stringstream ss(s);
		std::string item;

		while (std::getline(ss, item, delim)) {
			// Handle CR+LF line endings by removing trailing CR
			if (!item.empty() && item.back() == '\r') {
				item.pop_back();
			}
			out.push_back(item);
		}
		return out;
	}

	static std::string stringize(
		const char * fmt,
		...) {
		va_list args;
		va_start(args, fmt);

		// Get required buffer size
		va_list args_copy;
		va_copy(args_copy, args);
		int size = std::vsnprintf(nullptr, 0, fmt, args_copy) + 1;
		va_end(args_copy);

		// Format the string
		std::vector<char> buf(size);
		std::vsnprintf(buf.data(), size, fmt, args);
		va_end(args);

		return std::string(buf.data());
	}

	// Helper method to maintain motor configuration state across the class
	static std::array<int, 2> & getLastKnownMotorConfig() {
		static std::array<int, 2> lastKnownConfig = { MOTOR_STEP_DIV16, MOTOR_STEP_DIV16 };
		return lastKnownConfig;
	}

	/**
	 * @brief Check if motors are currently moving or if a motion command is executing
	 *
	 * This is a convenience method that calls the QM (Query Motors) command
	 * and returns true if either a motion command is executing or
	 * any of the motors are moving.
	 *
	 * @return true if motors are moving or a motion command is executing
	 * @return false if motors are not moving and no motion command is executing
	 */
	bool isMoving() {
		try {
			std::string response = sendCommand("QM");

			// Expected format: QM,a,b,c,d where:
			// a: 1 if command executing, 0 if not
			// b: 1 if motor 1 is moving, 0 if not
			// c: 1 if motor 2 is moving, 0 if not
			// d: 1 if FIFO not empty, 0 if empty

			if (response.find("QM,") != 0) {
				ofLogError("ofxEbbControl") << "Invalid response to QM command: " << response;
				return false;
			}

			// Split response by commas
			std::vector<std::string> parts;
			std::istringstream ss(response);
			std::string part;

			while (std::getline(ss, part, ',')) {
				parts.push_back(part);
			}

			if (parts.size() < 4) {
				ofLogError("ofxEbbControl") << "Invalid QM response format, expected at least 4 parts: " << response;
				return false;
			}

			// Check if command executing or any motor moving
			bool cmdExecuting = (parts[1] == "1");
			bool motor1Moving = (parts[2] == "1");
			bool motor2Moving = (parts[3] == "1");

			return cmdExecuting || motor1Moving || motor2Moving;

		} catch (const std::exception & e) {
			ofLogError("ofxEbbControl") << "Error in isMoving(): " << e.what();
			return false;
		}
	}
};
