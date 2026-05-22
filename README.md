> **Note:** To access all shared projects, get information about environment setup, and view other guides, please visit [Explore-In-HMOS-Wearable Index](https://github.com/Explore-In-HMOS-Wearable/hmos-index).

# How to Create ArcText
ArcText is a native C++ module for HarmonyOS that enables the rendering of text along a circular arc. 

It is optimized for circular wearable devices, automatically calculating font size and radius relative to the canvas dimensions to ensure consistent visual results across different screen sizes (e.g., 466px, 454px, 390px). By integrating with the ArkUI Native Node API, it provides high-performance drawing capabilities directly within the ArkUI render pipeline.

# Preview
<div>
  <img src="screenshots/1.png" width="25%" />
  <img src="screenshots/2.png" width="25%" />
</div>

# Use Cases
*   **Adaptive UI Layouts:** Displaying text that conforms to the curved geometry of circular screens without manual layout calculation.
*   **Decorative Typography:** Adding stylistic curved text to the top or bottom of UI components in wearable applications.

# Tech Stack
*   **Languages:** C++, ArkTS
*   **Frameworks:** ArkUI (HarmonyOS)
*   **Native Libraries:**
    *   `libnative_drawing.so`: For low-level 2D graphics rendering (Canvas, Font, TextBlob).
    *   `libace_ndk.z.so`: For native ArkUI node creation and manipulation.
    *   `libace_napi.z.so`: For the Node-API interface between C++ and ArkTS.
    *   `libhilog_ndk.z.so`: For system logging.
*   **Build System:** CMake

# Directory Structure
```
entry/src/main/
├── cpp/
│   ├── CMakeLists.txt
│   ├── napi_init.cpp
│   └── types/
│       └── libentry/
│           ├── Index.d.ts
│           └── oh-package.json5
├── ets/
│   ├── entryability/
│   │   └── EntryAbility.ets
│   ├── entrybackupability/
│   │   └── EntryBackupAbility.ets
│   └── pages/
│       └── Index.ets
```

# Constraints and Restrictions
## Supported Devices
- Huawei Watch 5
- DevEco Studio Simulator

# LICENSE
**ArcText** is distributed under the terms of the **MIT License**.
See the [LICENSE](/LICENSE) for more information.
