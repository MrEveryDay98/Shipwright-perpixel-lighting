#pragma once

// (Not named Math.h: on case-insensitive filesystems that would shadow the C library <math.h>.)

#include <cmath>
#include <libultraship/libultraship.h>

namespace Metroid {

struct Vec3 {
    float x = 0, y = 0, z = 0;
};

// N64-convention 4x4 matrix (row vectors, translation in row 3), as libultra's gu functions build them.
struct Mat4 {
    float m[4][4] = {};
};

inline Mat4 Identity() {
    Mat4 r;
    for (int i = 0; i < 4; i++) {
        r.m[i][i] = 1.0f;
    }
    return r;
}

// Row-vector convention: Mul(a, b) applies a first, then b.
inline Mat4 Mul(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            float sum = 0;
            for (int k = 0; k < 4; k++) {
                sum += a.m[i][k] * b.m[k][j];
            }
            r.m[i][j] = sum;
        }
    }
    return r;
}

inline Mat4 Translate(float x, float y, float z) {
    Mat4 r = Identity();
    r.m[3][0] = x;
    r.m[3][1] = y;
    r.m[3][2] = z;
    return r;
}

inline Mat4 Scale(float x, float y, float z) {
    Mat4 r = Identity();
    r.m[0][0] = x;
    r.m[1][1] = y;
    r.m[2][2] = z;
    return r;
}

inline Mat4 RotateY(float deg) {
    float c = cosf(deg * (float)M_PI / 180.0f), s = sinf(deg * (float)M_PI / 180.0f);
    Mat4 r = Identity();
    r.m[0][0] = c;
    r.m[0][2] = -s;
    r.m[2][0] = s;
    r.m[2][2] = c;
    return r;
}

inline Mat4 RotateZ(float deg) {
    float c = cosf(deg * (float)M_PI / 180.0f), s = sinf(deg * (float)M_PI / 180.0f);
    Mat4 r = Identity();
    r.m[0][0] = c;
    r.m[0][1] = s;
    r.m[1][0] = -s;
    r.m[1][1] = c;
    return r;
}

inline Mat4 Perspective(float fovyDeg, float aspect, float nearZ, float farZ) {
    Mat4 r;
    float cot = 1.0f / tanf(fovyDeg * (float)M_PI / 360.0f);
    r.m[0][0] = cot / aspect;
    r.m[1][1] = cot;
    r.m[2][2] = (nearZ + farZ) / (nearZ - farZ);
    r.m[2][3] = -1.0f;
    r.m[3][2] = 2.0f * nearZ * farZ / (nearZ - farZ);
    return r;
}

inline Mat4 LookAt(Vec3 eye, Vec3 at, Vec3 up) {
    Vec3 look = { at.x - eye.x, at.y - eye.y, at.z - eye.z };
    float len = -1.0f / sqrtf(look.x * look.x + look.y * look.y + look.z * look.z);
    look = { look.x * len, look.y * len, look.z * len };

    Vec3 right = { up.y * look.z - up.z * look.y, up.z * look.x - up.x * look.z, up.x * look.y - up.y * look.x };
    len = 1.0f / sqrtf(right.x * right.x + right.y * right.y + right.z * right.z);
    right = { right.x * len, right.y * len, right.z * len };

    up = { look.y * right.z - look.z * right.y, look.z * right.x - look.x * right.z,
           look.x * right.y - look.y * right.x };
    len = 1.0f / sqrtf(up.x * up.x + up.y * up.y + up.z * up.z);
    up = { up.x * len, up.y * len, up.z * len };

    Mat4 r;
    r.m[0][0] = right.x;
    r.m[1][0] = right.y;
    r.m[2][0] = right.z;
    r.m[3][0] = -(eye.x * right.x + eye.y * right.y + eye.z * right.z);
    r.m[0][1] = up.x;
    r.m[1][1] = up.y;
    r.m[2][1] = up.z;
    r.m[3][1] = -(eye.x * up.x + eye.y * up.y + eye.z * up.z);
    r.m[0][2] = look.x;
    r.m[1][2] = look.y;
    r.m[2][2] = look.z;
    r.m[3][2] = -(eye.x * look.x + eye.y * look.y + eye.z * look.z);
    r.m[3][3] = 1.0f;
    return r;
}

// Encode into the RSP's s15.16 fixed-point layout: integer halves in the first 8 words, fractions after.
inline void ToMtx(const Mat4& src, Mtx* dst) {
    int32_t* w = &dst->m[0][0];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j += 2) {
            int32_t a = (int32_t)(src.m[i][j] * 65536.0f);
            int32_t b = (int32_t)(src.m[i][j + 1] * 65536.0f);
            w[i * 2 + j / 2] = (int32_t)(((uint32_t)a & 0xFFFF0000) | ((uint32_t)b >> 16));
            w[8 + i * 2 + j / 2] = (int32_t)(((uint32_t)a << 16) | ((uint32_t)b & 0xFFFF));
        }
    }
}

} // namespace Metroid
