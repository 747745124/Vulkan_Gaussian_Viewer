An simple implementation of Gaussian Splatting viewer in Vulkan, with Bitonic sort. Currently there are some visual artifacts, can be from calculation error in vertex shader.

* The implementation is at least compatible with ARM - macOS environment
* Use `glslc` to compile shader into `.spv` first
* Modify `ParseOptions opts;` in `application.cpp` if the `.ply` file is in linear scale, or the quarternion order is `XYZW`

![Screenshot 2025-11-05 at 03.22.51](https://s2.loli.net/2025/11/06/rQ1b4onRBULzuv7.png)