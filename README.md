# ofxEbbControl 🐣

> Experimental OpenFrameworks addon for EggBot/EiBotBoard control

:warning: **Disclaimer:** This addon is experimental and mostly untested. It’s based on an **LLM-aided** automatic C++ port of the [jmpinit/ebb-control](https://github.com/jmpinit/ebb-control) JavaScript library and may contain bugs or incomplete behavior.


---

## ✨ Features

- 🖋️ **Pen Control**: `setPenState()`, `togglePen()`, `isPenDown()`
- ⚙️ **Motion**: `moveAbsolute()`, `moveLowLevel()`, `moveTimed()`, `moveStepperMixedAxis()`
- 🎛️ **I/O**: `getAnalogValues()`, `configureAnalogInput()`, `getDigitalInputs()`, `setDigitalOutputs()`, `setPinMode()`, `getPin()`, `setPin()`
- 🔍 **Status Queries**: `getGeneralStatus()`, `getMotorStatus()`, `getCurrentInfo()`, `getStepPositions()`, `getLayer()`, `getNickname()`, `getVersion()`

---

## 🛠 Installation

1. Clone this repository and clone the `ofxEbbControl` folder into your OF `addons/` directory.
2. Ensure that `ofxEbbControl.h` and `addon_config.mk` are included.
3. Use the Project Generator to add **ofxEbbControl** to your project.

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

## 📖 Documentation

See `src/ofxEbbControl.h` for full API docs and method descriptions.

---

## 📜 License

This addon is released under the **MIT License**. See [LICENSE](LICENSE) for details.

