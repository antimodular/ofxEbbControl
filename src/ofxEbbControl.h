// ofxEbbControl.h
#pragma once

#include "ofSerial.h"
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
   * @param cmd        Command string (without CR).
   * @param numLines   Number of response lines to read (default 1).
   * @param timeoutMs  Timeout in milliseconds (default 3000).
   * @returns          Concatenated response lines separated by CRLF.
   * @throws           runtime_error if timeout or communication error.
   */
	std::string sendCommand(
		const std::string & cmd,
		int numLines = 1,
		int timeoutMs = 3000) {
		// Transmit command with CR
		std::string out = cmd + "\r";
		serial.writeBytes(
			reinterpret_cast<const unsigned char *>(out.c_str()),
			out.size());

		// Read response
		std::string result;
		auto start = std::chrono::steady_clock::now();

		for (int lineIndex = 0; lineIndex < numLines; ++lineIndex) {
			std::string line;
			while (true) {
				char ch;
				if (serial.readBytes(
						reinterpret_cast<unsigned char *>(&ch),
						1)
					> 0) {
					if (ch == '\n') {
						break;
					}
					if (ch != '\r') {
						line.push_back(ch);
					}
				}
				auto now = std::chrono::steady_clock::now();
				if (
					std::chrono::duration_cast<
						std::chrono::milliseconds>(now - start)
						.count()
					> timeoutMs) {
					throw std::runtime_error(
						"Command '" + cmd + "' timed out");
				}
				std::this_thread::sleep_for(
					std::chrono::milliseconds(2));
			}

			if (!result.empty()) {
				result += "\r\n";
			}
			result += line;
		}

		return result;
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
		auto cmd = disableMotors ? "ES,1" : "ES";
		auto resp = sendCommand(cmd, 2);
		auto lines = split(resp, '\r');
		checkStatus(lines);

		auto vals = split(lines[0], ',');
		return StopInfo {
			vals[0] == "1",
			{ std::stoi(vals[1]), std::stoi(vals[2]) },
			{ std::stoi(vals[3]), std::stoi(vals[4]) }
		};
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
		auto resp = sendCommand("QB", 2);
		auto lines = split(resp, '\r');
		checkStatus(lines);
		return lines[0] == "1";
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
		auto resp = sendCommand("QC", 2);
		auto lines = split(resp, '\r');
		checkStatus(lines);

		auto vals = split(lines[0], ',');
		double ra0 = 3.3 * std::stoi(vals[0]) / 1023.0;
		double vp = 3.3 * std::stoi(vals[1]) / 1023.0;
		double scale = oldBoard ? (1.0 / 11.0) : (1.0 / 9.2);

		return {
			ra0 / 1.76,
			vp / scale + 0.3
		};
	}

	/**
   * Query motor step mode config (QE).
   */
	std::array<int, 2> getMotorConfig() {
		auto resp = sendCommand("QE", 2);
		auto lines = split(resp, '\r');
		checkStatus(lines);

		auto modes = split(lines[0], ',');
		std::map<int, int> mapping = {
			{ 0, MOTOR_DISABLE },
			{ 1, MOTOR_STEP_DIV1 },
			{ 2, MOTOR_STEP_DIV2 },
			{ 4, MOTOR_STEP_DIV4 },
			{ 8, MOTOR_STEP_DIV8 },
			{ 16, MOTOR_STEP_DIV16 }
		};

		return {
			mapping.at(std::stoi(modes[0])),
			mapping.at(std::stoi(modes[1]))
		};
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
		auto resp = sendCommand("QL", 2);
		auto lines = split(resp, '\r');
		checkStatus(lines);
		return std::stoi(lines[0]);
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
   */
	bool isPenDown() {
		auto resp = sendCommand("QP", 2);
		auto lines = split(resp, '\r');
		checkStatus(lines);
		return std::stoi(lines[0]) == PEN_DOWN;
	}

	/**
   * Query servo power status (QR).
   */
	bool isServoPowered() {
		auto resp = sendCommand("QR", 2);
		auto lines = split(resp, '\r');
		checkStatus(lines);
		return std::stoi(lines[0]) == SERVO_POWER_ON;
	}

	/**
   * Query current step positions (QS).
   */
	std::array<int, 2> getStepPositions() {
		auto resp = sendCommand("QS", 2);
		auto lines = split(resp, '\r');
		checkStatus(lines);

		auto s = split(lines[0], ',');
		return { std::stoi(s[0]), std::stoi(s[1]) };
	}

	/**
   * Query EBB nickname (QT).
   */
	std::string getNickname() {
		auto resp = sendCommand("QT", 2);
		auto lines = split(resp, '\r');
		checkStatus(lines);
		return lines[0];
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

private:
	ofSerial serial;
	std::string serialPort;

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
		if (lines.size() < 2 || lines[1] != "OK") {
			throw std::runtime_error("Bad status response");
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
			out.push_back(item);
		}
		return out;
	}

	static std::string stringize(
		const char * fmt,
		...) {
		va_list args;
		va_start(args, fmt);
		int size = std::vsnprintf(nullptr, 0, fmt, args) + 1;
		std::vector<char> buf(size);
		std::vsnprintf(buf.data(), size, fmt, args);
		va_end(args);
		return std::string(buf.data());
	}
};
