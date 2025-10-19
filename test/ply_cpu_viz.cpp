#include <vector>
#include <string>
#include <iostream>
#include <cmath>
#include <algorithm>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "external/stb_image_write.h"

#include "base/ply_parser.h"
#include "base/camera.h"

struct Mat4 { float m[16]; };

static Mat4 mul(const Mat4 &a, const Mat4 &b)
{
    Mat4 r{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
        {
            r.m[i*4 + j] = 0;
            for (int k = 0; k < 4; ++k) r.m[i*4 + j] += a.m[i*4 + k] * b.m[k*4 + j];
        }
    return r;
}

static void projPoint(const Mat4 &mvp, const glm::vec3 &p, int W, int H, int &sx, int &sy, float &w)
{
    float x = p.x, y = p.y, z = p.z;
    float X = mvp.m[0]*x + mvp.m[4]*y + mvp.m[8]*z + mvp.m[12];
    float Y = mvp.m[1]*x + mvp.m[5]*y + mvp.m[9]*z + mvp.m[13];
    float Z = mvp.m[2]*x + mvp.m[6]*y + mvp.m[10]*z + mvp.m[14];
    float Wv = mvp.m[3]*x + mvp.m[7]*y + mvp.m[11]*z + mvp.m[15];
    w = Wv;
    if (Wv == 0) { sx = sy = -1; return; }
    float ndcX = X / Wv, ndcY = Y / Wv;
    sx = static_cast<int>((ndcX * 0.5f + 0.5f) * (float)W);
    sy = static_cast<int>(((1.0f - (ndcY * 0.5f + 0.5f)) * (float)H));
}

static Mat4 toMat4(const glm::mat4 &g)
{
    Mat4 m{};
    const float *p = (const float *)&g[0][0];
    for (int i = 0; i < 16; ++i) m.m[i] = p[i];
    return m;
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        std::cerr << "Usage: ply_cpu_viz <input.ply> <output.png>\n";
        return 2;
    }
    std::string inPath = argv[1];
    std::string outPath = argv[2];

    std::vector<Gaussian> gs;
    ParseOptions opts;
    opts.scaleSpace = ScaleSpace::Linear;
    opts.rotationOrder = RotationOrder::XYZW;
    if (!parse_ply(inPath, gs, opts))
    {
        std::cerr << "Failed to parse: " << inPath << "\n";
        return 3;
    }

    const int W = 800, H = 600;
    std::vector<uint8_t> img(W * H * 3, 0);

    // Simple camera
    glm::vec3 eye(0, 0, 3), center(0, 0, 0), up(0, 1, 0);
    glm::mat4 view = glm::lookAt(eye, center, up);
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), (float)W / (float)H, 0.01f, 100.0f);
    // GLM with depth 0..1 expected in Vulkan; for our CPU viz we keep it as is
    glm::mat4 model(1.0f);
    Mat4 mvp = toMat4(proj * view * model);

    // Render each point as a single pixel (nearest), colored by f_dc_0
    for (const auto &g : gs)
    {
        int sx, sy; float w;
        projPoint(mvp, g.position, W, H, sx, sy, w);
        if (sx < 0 || sy < 0 || sx >= W || sy >= H) continue;
        size_t idx = (size_t)(sy * W + sx) * 3;
        uint8_t r = (uint8_t)std::clamp(g.f_dc_0.r * 255.0f, 0.0f, 255.0f);
        uint8_t gcol = (uint8_t)std::clamp(g.f_dc_0.g * 255.0f, 0.0f, 255.0f);
        uint8_t b = (uint8_t)std::clamp(g.f_dc_0.b * 255.0f, 0.0f, 255.0f);
        img[idx + 0] = r;
        img[idx + 1] = gcol;
        img[idx + 2] = b;
    }

    if (!stbi_write_png(outPath.c_str(), W, H, 3, img.data(), W * 3))
    {
        std::cerr << "Failed to write PNG: " << outPath << "\n";
        return 4;
    }

    std::cout << "Wrote " << outPath << "\n";
    return 0;
}
