#include <iostream>
#include <fstream>
#include <sstream>
#include "model.h"
#include "geometry.h"

Model::Model(const char *filename) : verts_(), faces_(), faces_2(), verts_2(), norms_(), uv_(), diffusemap_(), normalmap_(), specularmap_() {
    std::ifstream in;
    in.open (filename, std::ifstream::in);
    if (in.fail()) return;
    std::string line;
    while (!in.eof()) {
        std::getline(in, line);
        std::istringstream iss(line.c_str());
        char trash;
        if (!line.compare(0, 2, "v ")) {
            iss >> trash;
            Vec3f v;
            for (int i=0;i<3;i++) iss >> v[i];
            verts_.push_back(v);
            verts_2.push_back(v[0]);
            verts_2.push_back(v[1]);
            verts_2.push_back(v[2]);
        } else if (!line.compare(0, 3, "vn ")) {
            iss >> trash >> trash;
            Vec3f n;
            for (int i=0;i<3;i++) iss >> n[i];
            norms_.push_back(n);
        } else if (!line.compare(0, 3, "vt ")) {
            iss >> trash >> trash;
            Vec2f uv;
            for (int i=0;i<2;i++) iss >> uv[i];
            uv_.push_back(uv);
        }  else if (!line.compare(0, 2, "f ")) {
            std::vector<Vec3i> f;
            Vec3i tmp;
            iss >> trash;
            //iss>>tmp[0];
            //std::cout<<"trash: " <<trash<<","<<tmp[0]<<std::endl;
            while (iss >> tmp[0] >> trash >> tmp[1] >>trash >> tmp[2]) {
                for (int i=0; i<3; i++) tmp[i]--; // in wavefront obj all indices start at 1, not zero
                f.push_back(tmp);
            }
            faces_.push_back(f);
            faces_2.push_back(f[0][0]);
            faces_2.push_back(f[0][1]);
            faces_2.push_back(f[0][2]);
            faces_2.push_back(f[1][0]);
            faces_2.push_back(f[1][1]);
            faces_2.push_back(f[1][2]);
            faces_2.push_back(f[2][0]);
            faces_2.push_back(f[2][1]);
            faces_2.push_back(f[2][2]);
        }
    }
    
    normals_2 = std::vector<float>(verts_2.size());
    uvs_2 = std::vector<float>(2 * (verts_2.size() / 3));
    for(int i = 0; i < faces_.size(); i++) {
        for(int j = 0; j < 3; j++) {
            int ind =faces_[i][j][0];
            Vec3f n = norms_[faces_[i][j][2]];
            Vec2f uv = uv_[faces_[i][j][1]];
            normals_2[ind * 3] = n.x;
            normals_2[ind * 3+1] = n.y;
            normals_2[ind * 3+2] = n.z;
            uvs_2[ind * 2] = uv.x;
            uvs_2[ind * 2+1] = uv.y;
        }
    }
    
    std::cerr << "# v# " << verts_.size() << " f# "  << faces_.size() << " vt# " << uv_.size() << " vn# " << norms_.size() << std::endl;
    load_texture(filename, "_diffuse.tga", diffusemap_);
    load_texture(filename, "_nm_tangent.tga",      normalmap_);
    load_texture(filename, "_spec.tga",    specularmap_);
}

Model::~Model() {}

int Model::nverts() {
    return (int)verts_.size();
}

int Model::nfaces() {
    return (int)faces_.size();
}

std::vector<int> Model::face(int idx) {
    std::vector<int> face;
    for (int i=0; i<(int)faces_[idx].size(); i++) face.push_back(faces_[idx][i][0]);
    return face;
}

Vec3f Model::vert(int i) {
    return verts_[i];
}

Vec3f Model::vert(int iface, int nthvert) {
    return verts_[faces_[iface][nthvert][0]];
}

void Model::load_texture(std::string filename, const char *suffix, TGAImage &img) {
    std::string texfile(filename);
    size_t dot = texfile.find_last_of(".");
    if (dot!=std::string::npos) {
        texfile = texfile.substr(0,dot) + std::string(suffix);
        std::cerr << "texture file " << texfile << " loading " << (img.read_tga_file(texfile.c_str()) ? "ok" : "failed") << std::endl;
        img.flip_vertically();
    }
}

TGAColor Model::diffuse(Vec2f uvf) {
    Vec2i uv(uvf[0]*diffusemap_.get_width(), uvf[1]*diffusemap_.get_height());
    return diffusemap_.get(uv[0], uv[1]);
}

Vec3f Model::normal(Vec2f uvf) {
    Vec2i uv(uvf[0]*normalmap_.get_width(), uvf[1]*normalmap_.get_height());
    TGAColor c = normalmap_.get(uv[0], uv[1]);
    Vec3f res;
    for (int i=0; i<3; i++)
        res[2-i] = (float)c[i]/255.f*2.f - 1.f;
    return res;
}

Vec2f Model::uv(int iface, int nthvert) {
    return uv_[faces_[iface][nthvert][1]];
}

float Model::specular(Vec2f uvf) {
    Vec2i uv(uvf[0]*specularmap_.get_width(), uvf[1]*specularmap_.get_height());
    return specularmap_.get(uv[0], uv[1])[0]/1.f;
}

Vec3f Model::normal(int iface, int nthvert) {
    int idx = faces_[iface][nthvert][2];
    return norms_[idx].normalize();
}

