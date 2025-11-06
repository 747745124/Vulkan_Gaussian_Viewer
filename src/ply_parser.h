#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include "external/tinyply.h"

struct Gaussian {
    //splat position
    glm::vec3 position{0}; 
    //splat normal
    glm::vec3 normal{0,0,1};
    //splat scale
    glm::vec3 scale{1};
    //splat rotation
    glm::quat rot{1,0,0,0};
    //splat opacity
    float opacity{1};
    //splat f_dc_0, 0-order SH coeffs
    glm::vec3 f_dc_0{1};
    //splat SH coeffs, higher order
    std::vector<float> sh_coeffs;
};

enum class ScaleSpace { Linear, Log }; 

enum class RotationOrder { WXYZ, XYZW }; 

struct ParseOptions {
    ScaleSpace scaleSpace{ ScaleSpace::Linear };
    RotationOrder rotationOrder{ RotationOrder::WXYZ };
};

bool parse_ply(const std::string& filename, std::vector<Gaussian>& gaussians, const ParseOptions& options = {});

