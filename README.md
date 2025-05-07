# ofxEbbControl 🐣

> Experimental OpenFrameworks addon for EggBot/EiBotBoard control

:warning: **Disclaimer:** This addon is experimental and mostly untested. It's based on an **LLM-aided** automatic C++ port of the [jmpinit/ebb-control](https://github.com/jmpinit/ebb-control) JavaScript library and may contain bugs or incomplete behavior.

---

## ✨ Features

- 🖋️ **Pen Control**: `setPenState()`, `togglePen()`, `isPenDown()`
- ⚙️ **Motion Control**:
  - Basic Movement: `moveAbsolute()`, `moveLowLevel()`, `moveTimed()`
  - Advanced Movement: `moveStepperMixedAxis()`, `drawLine()`, `drawPolygon()`
  - Status: `isMoving()`, `waitForCompletion()`
- 🎛️ **I/O Operations**:
  - Analog: `getAnalogValues()`, `configureAnalogInput()`
  - Digital: `getDigitalInputs()`, `setDigitalOutputs()`, `setPinMode()`, `getPin()`, `setPin()`
- 🔍 **Status Queries**:
  - Motion: `getGeneralStatus()`, `getMotorStatus()`
  - Hardware: `getCurrentInfo()`, `getStepPositions()`, `getButtonState()`
  - Configuration: `getLayer()`, `getNickname()`, `getVersion()`
- 🛠️ **Reliability Features**:
  - Robust command response handling
  - Timeout detection and recovery
  - Specialized handling for different command formats

---

## 🛠 Installation

1. Clone this repository and copy the `ofxEbbControl` folder into your OF `addons/` directory.
2. Ensure that `ofxEbbControl.h` and `addon_config.mk` are included.
3. Use the Project Generator to add **ofxEbbControl** to your project.

---

## 🖥️ Supported OpenFrameworks Versions

This addon supports OpenFrameworks **v0.12.0**. Compatibility with other versions is not guaranteed.

---

## 🚀 Usage Example

```cpp
#include "ofMain.h"
#include "ofxEbbControl.h"

class ofApp : public ofBaseApp {
public:
    ofxEbbControl board;

    void setup() {
        if (!board.setup("/dev/ttyUSB0")) {
            ofLogError() << "⚠️ Failed to open EBB port";
            std::exit(1);
        }
        board.clearStepPosition();
        board.setPenState(false);
    }

    void draw() {
        board.moveAbsolute(1000, 1000, 0);
        board.setPenState(true);
    }
};

int main() {
    ofSetupOpenGL(1024,768,OF_WINDOW);
    ofRunApp(new ofApp());
}
```

---

## 📝 Recent Updates

- **Command Response Handling**: Enhanced error handling for command responses, especially for the `QM` command (Query Motors) which has a specific response format
- **Timeout Fixes**: Improved timeout detection and handling across all command types
- **Response Format Detection**: Added specific handling for various command response formats (V, QG, QM, etc.)
- **Response Cleaning**: Better cleanup of responses including handling of CR/LF characters

---

## 🐛 Known Issues

- The implementation may not handle all edge cases in EBB response formats
- The JS to C++ port might have subtle differences in behavior
- Some commands may require specific firmware versions (see EBB documentation)

---

## 📖 Documentation

- See `src/ofxEbbControl.h` for full API docs and method descriptions
- The complete EBB command reference is available at [Evil Mad Scientist: EBB Command Set](https://evil-mad.github.io/EggBot/ebb.html)

---

## 📜 License

This addon is released under the **MIT License**. See [LICENSE](LICENSE) for details.

