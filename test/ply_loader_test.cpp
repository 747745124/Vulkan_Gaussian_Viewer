#include <fstream>
#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>

#include "base/ply_parser.h"

static void write_ascii_ply(const std::string &path, size_t N)
{
    std::ofstream os(path, std::ios::binary);
    if (!os)
    {
        throw std::runtime_error("cannot open output: " + path);
    }

    // Header: positions, normals, f_dc_*, opacity, scale_*, rot_*, f_rest_0..2
    os << "ply\n";
    os << "format ascii 1.0\n";
    os << "element vertex " << N << "\n";
    os << "property float x\n";
    os << "property float y\n";
    os << "property float z\n";
    os << "property float nx\n";
    os << "property float ny\n";
    os << "property float nz\n";
    os << "property float f_dc_0\n";
    os << "property float f_dc_1\n";
    os << "property float f_dc_2\n";
    os << "property float opacity\n";
    os << "property float scale_0\n";
    os << "property float scale_1\n";
    os << "property float scale_2\n";
    os << "property float rot_0\n"; // x
    os << "property float rot_1\n"; // y
    os << "property float rot_2\n"; // z
    os << "property float rot_3\n"; // w
    os << "property float f_rest_0\n";
    os << "property float f_rest_1\n";
    os << "property float f_rest_2\n";
    os << "end_header\n";

    for (size_t i = 0; i < N; ++i)
    {
        float x = static_cast<float>(i);
        float y = static_cast<float>(2 * i);
        float z = static_cast<float>(3 * i);
        float nx = 0.0f, ny = 1.0f, nz = 0.0f;
        float f0 = static_cast<float>(i) + 0.1f;
        float f1 = static_cast<float>(i) + 0.2f;
        float f2 = static_cast<float>(i) + 0.3f;
        float opacity = 0.5f;
        // choose scale values that are reasonable for both linear and log tests
        float s0 = 0.0f + 0.1f * static_cast<float>(i);
        float s1 = 0.1f + 0.1f * static_cast<float>(i);
        float s2 = 0.2f + 0.1f * static_cast<float>(i);
        // rotation stored as XYZW
        float qx = 0.01f * static_cast<float>(i);
        float qy = 0.02f * static_cast<float>(i);
        float qz = 0.03f * static_cast<float>(i);
        float qw = 1.0f; // not normalized; sufficient for mapping test
        float r0 = qx, r1 = qy, r2 = qz, r3 = qw; // XYZW
        float fr0 = 10.0f + static_cast<float>(i);
        float fr1 = 20.0f + static_cast<float>(i);
        float fr2 = 30.0f + static_cast<float>(i);

        os << x << ' ' << y << ' ' << z << ' '
           << nx << ' ' << ny << ' ' << nz << ' '
           << f0 << ' ' << f1 << ' ' << f2 << ' '
           << opacity << ' '
           << s0 << ' ' << s1 << ' ' << s2 << ' '
           << r0 << ' ' << r1 << ' ' << r2 << ' ' << r3 << ' '
           << fr0 << ' ' << fr1 << ' ' << fr2 << '\n';
    }
}

static bool approxEqual(float a, float b, float eps = 1e-5f)
{
    return std::fabs(a - b) <= eps * (1.0f + std::max(std::fabs(a), std::fabs(b)));
}

int main()
{
    try
    {
        const std::string path = "/Users/naoyuki/vk_tutorial/build/ply_test_ascii.ply";
        const size_t N = 20;
        write_ascii_ply(path, N);

        std::vector<Gaussian> gs;

        // Test Linear scale, XYZW rotation order
        ParseOptions opts1;
        opts1.scaleSpace = ScaleSpace::Linear;
        opts1.rotationOrder = RotationOrder::XYZW;
        if (!parse_ply(path, gs, opts1))
        {
            std::cerr << "parse_ply failed for Linear/XYZW" << std::endl;
            return 2;
        }
        if (gs.size() != N)
        {
            std::cerr << "unexpected count: " << gs.size() << std::endl;
            return 3;
        }
        // Validate a couple of entries
        {
            const Gaussian &g0 = gs[0];
            if (!approxEqual(g0.position.x, 0.0f) || !approxEqual(g0.position.y, 0.0f) || !approxEqual(g0.position.z, 0.0f)) return 4;
            if (!approxEqual(g0.normal.y, 1.0f)) return 5;
            if (!approxEqual(g0.f_dc_0.x, 0.1f) || !approxEqual(g0.f_dc_0.y, 0.2f) || !approxEqual(g0.f_dc_0.z, 0.3f)) return 6;
            if (!approxEqual(g0.opacity, 0.5f)) return 7;
            if (!approxEqual(g0.scale.x, 0.0f) || !approxEqual(g0.scale.y, 0.1f) || !approxEqual(g0.scale.z, 0.2f)) return 8;
            // XYZW mapping: file rot_3 is w, rot_0..2 are x,y,z
            if (!approxEqual(g0.rot.w, 1.0f) || !approxEqual(g0.rot.x, 0.0f) || !approxEqual(g0.rot.y, 0.0f) || !approxEqual(g0.rot.z, 0.0f)) return 9;
            if (g0.sh_coeffs.size() != 3) return 10;
            if (!approxEqual(g0.sh_coeffs[0], 10.0f) || !approxEqual(g0.sh_coeffs[1], 20.0f) || !approxEqual(g0.sh_coeffs[2], 30.0f)) return 11;
        }
        {
            const size_t i = 7;
            const Gaussian &g = gs[i];
            if (!approxEqual(g.position.x, static_cast<float>(i))) return 12;
            if (!approxEqual(g.scale.y, 0.1f + 0.1f * static_cast<float>(i))) return 13;
            if (!approxEqual(g.rot.x, 0.01f * static_cast<float>(i))) return 14;
            if (!approxEqual(g.rot.w, 1.0f)) return 15;
            if (!approxEqual(g.sh_coeffs[2], 30.0f + static_cast<float>(i))) return 16;
        }

        // Test Log scale, same file
        ParseOptions opts2;
        opts2.scaleSpace = ScaleSpace::Log;
        opts2.rotationOrder = RotationOrder::XYZW;
        if (!parse_ply(path, gs, opts2))
        {
            std::cerr << "parse_ply failed for Log/XYZW" << std::endl;
            return 20;
        }
        {
            const size_t i = 5;
            const Gaussian &g = gs[i];
            float s0 = 0.0f + 0.1f * static_cast<float>(i);
            float s1 = 0.1f + 0.1f * static_cast<float>(i);
            float s2 = 0.2f + 0.1f * static_cast<float>(i);
            if (!approxEqual(g.scale.x, std::exp(s0)) || !approxEqual(g.scale.y, std::exp(s1)) || !approxEqual(g.scale.z, std::exp(s2))) return 21;
        }

        std::cout << "PLY loader tests passed" << std::endl;
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
}
