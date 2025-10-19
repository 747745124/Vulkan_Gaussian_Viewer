#include "ply_parser.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <algorithm>
#include <cstring>

#define TINYPLY_IMPLEMENTATION
#include "external/tinyply.h"

using namespace tinyply;

static const float *asFloat(const std::shared_ptr<PlyData> &d)
{
    return d ? reinterpret_cast<const float *>(d->buffer.get()) : nullptr;
}

bool parse_ply(const std::string &filename, std::vector<Gaussian> &gaussians, const ParseOptions &options)
{
    try
    {
        std::ifstream is(filename, std::ios::binary);
        if (!is)
        {
            std::cerr << "Failed to open PLY file: " << filename << std::endl;
            return false;
        }

        PlyFile pf;
        if (!pf.parse_header(is))
        {
            std::cerr << "Failed to parse PLY header: " << filename << std::endl;
            return false;
        }

        // Discover dynamic f_rest_* properties from header
        std::vector<std::string> frestProps;
        for (const auto &elem : pf.get_elements())
        {
            if (elem.name == "vertex")
            {
                for (const auto &prop : elem.properties)
                {
                    if (prop.name.rfind("f_rest_", 0) == 0)
                    {
                        frestProps.push_back(prop.name);
                    }
                }
            }
        }
        std::sort(frestProps.begin(), frestProps.end(), [](const std::string &a, const std::string &b) {
            // sort by numeric suffix
            int ai = 0, bi = 0;
            try { ai = std::stoi(a.substr(7)); } catch (...) { ai = 0; }
            try { bi = std::stoi(b.substr(7)); } catch (...) { bi = 0; }
            return ai < bi;
        });

        // Request properties
        auto pos = pf.request_properties_from_element("vertex", {"x", "y", "z"});

        std::shared_ptr<PlyData> nrm, fdc, opa, scl, rot, frest;
        try { nrm = pf.request_properties_from_element("vertex", {"nx", "ny", "nz"}); } catch (...) {}
        try { fdc = pf.request_properties_from_element("vertex", {"f_dc_0", "f_dc_1", "f_dc_2"}); } catch (...) {}
        try { opa = pf.request_properties_from_element("vertex", {"opacity"}); } catch (...) {}
        try { scl = pf.request_properties_from_element("vertex", {"scale_0", "scale_1", "scale_2"}); } catch (...) {}
        try { rot = pf.request_properties_from_element("vertex", {"rot_0", "rot_1", "rot_2", "rot_3"}); } catch (...) {}
        if (!frestProps.empty())
        {
            try { frest = pf.request_properties_from_element("vertex", frestProps); } catch (...) {}
        }

        pf.read(is);

        if (!pos)
        {
            std::cerr << "PLY missing vertex positions (x,y,z)" << std::endl;
            return false;
        }

        const size_t N = pos->count;
        gaussians.clear();
        gaussians.resize(N);

        const float *P = asFloat(pos);
        const float *Nn = asFloat(nrm);
        const float *Dc = asFloat(fdc);
        const float *Op = asFloat(opa);
        const float *Sc = asFloat(scl);
        const float *Qt = asFloat(rot);
        const float *Fr = asFloat(frest);

        const size_t numFrest = frestProps.size();

        for (size_t i = 0; i < N; ++i)
        {
            Gaussian g; // defaults from struct
            g.position = glm::vec3(P[3 * i + 0], P[3 * i + 1], P[3 * i + 2]);

            if (Nn)
            {
                g.normal = glm::vec3(Nn[3 * i + 0], Nn[3 * i + 1], Nn[3 * i + 2]);
            }
            if (Dc)
            {
                g.f_dc_0 = glm::vec3(Dc[3 * i + 0], Dc[3 * i + 1], Dc[3 * i + 2]);
            }
            if (Op)
            {
                g.opacity = Op[i];
            }
            if (Sc)
            {
                glm::vec3 s(Sc[3 * i + 0], Sc[3 * i + 1], Sc[3 * i + 2]);
                if (options.scaleSpace == ScaleSpace::Log)
                {
                    s = glm::exp(s);
                }
                g.scale = s;
            }
            if (Qt)
            {
                if (options.rotationOrder == RotationOrder::WXYZ)
                {
                    g.rot = glm::quat(Qt[4 * i + 0], Qt[4 * i + 1], Qt[4 * i + 2], Qt[4 * i + 3]);
                }
                else // XYZW
                {
                    g.rot = glm::quat(Qt[4 * i + 3], Qt[4 * i + 0], Qt[4 * i + 1], Qt[4 * i + 2]);
                }
            }
            if (Fr && numFrest > 0)
            {
                g.sh_coeffs.resize(numFrest);
                const float *base = Fr + i * numFrest;
                std::copy(base, base + numFrest, g.sh_coeffs.begin());
            }

            gaussians[i] = g;
        }

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "PLY parse error: " << e.what() << std::endl;
        return false;
    }
}
