PODEF
int p_ceil(float x)
{
    int base = (int) x;
    return (x > base) ? base + 1 : base;
}

PODEF float
p_cosf(float x)
{
    x += 1.57079632679f; 

    while (x > P_PI)  x -= P_PI2;
    while (x < -P_PI) x += P_PI2;

    const float B = 4.0f / P_PI;
    const float C = -4.0f / P_PI_POW2;

    float y = B * x + C * x * (x < 0 ? -x : x);

    const float P = 0.225f;
    y = P * (y * (y < 0 ? -y : y) - y) + y;

    return y;
}

PODEF float
p_sinf(float x)
{
    while (x > P_PI) x -= P_PI2;
    while (x < -P_PI) x += P_PI2;

    const float B = 4.0f / P_PI;
    const float C = -4.0f / (P_PI_POW2);

    float y = B * x + C * x * (x < 0 ? -x : x);

    const float P = 0.225f;
    y = P * (y * (y < 0 ? -y : y) - y) + y;

    return y;
}

PODEF Vec2f
p_vec2f(float x, float y)
{
    return (Vec2f) { .x = x, .y = y };
}

PODEF Vec2f
p_vec2f_add(Vec2f a, Vec2f b)
{
    return (Vec2f) {
        .x = a.x + b.x,
        .y = a.y + b.y,
    };
}

PODEF float
p_vec2f_cross(Vec2f a, Vec2f b)
{
    // axby − aybx
    return a.x * b.y - a.y * b.x;
}

PODEF float
p_vec2f_dot(Vec2f a, Vec2f b)
{
    // a · b = ax × bx + ay × by
    return a.x * b.x + a.y * b.y;
}

PODEF Vec2f
p_vec2f_mult(Vec2f a, float b)
{
    return (Vec2f) {
        .x = a.x * b,
        .y = a.y * b,
    };
}

PODEF Vec2f
p_vec2f_sub(Vec2f a, Vec2f b)
{
    return (Vec2f) {
        .x = a.x - b.x,
        .y = a.y - b.y,
    };
}

PODEF Vec2i
p_vec2i(float x, float y)
{
    return (Vec2i) { .x = x, .y = y };
}

PODEF Vec2i
p_vec2i_add(Vec2i a, Vec2i b)
{
    return (Vec2i) {
        .x = a.x + b.x,
        .y = a.y + b.y,
    };
}

PODEF float
p_vec2i_cross(Vec2i a, Vec2i b)
{
    // axby − aybx
    return a.x * b.y - a.y * b.x;
}

PODEF float
p_vec2i_dot(Vec2i a, Vec2i b)
{
    // a · b = ax × bx + ay × by
    return a.x * b.x + a.y * b.y;
}

PODEF Vec2i
p_vec2i_mult(Vec2i a, float b)
{
    return (Vec2i) {
        .x = a.x * b,
        .y = a.y * b,
    };
}

PODEF Vec2i
p_vec2i_sub(Vec2i a, Vec2i b)
{
    return (Vec2i) {
        .x = a.x - b.x,
        .y = a.y - b.y,
    };
}

PODEF Vec3f
p_vec3f(float x, float y, float z)
{
    return (Vec3f) { .x = x, .y = y, .z = z };
}

PODEF P_Mat4
p_mat4_identity(void)
{
    P_Mat4 res = {0};
    res.m[0]  = 1.0f;
    res.m[5]  = 1.0f;
    res.m[10] = 1.0f;
    res.m[15] = 1.0f;
    return res;
}

PODEF P_Mat4
p_mat4_mul(P_Mat4 a, P_Mat4 b)
{
    P_Mat4 res = {0};
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a.m[k * 4 + row] * b.m[col * 4 + k];
            }
            res.m[col * 4 + row] = sum;
        }
    }
    return res;
}

PODEF P_Mat4
p_mat4_translate(Vec3f v)
{
    P_Mat4 res = p_mat4_identity();
    res.m[12] = v.x;
    res.m[13] = v.y;
    res.m[14] = v.z;
    return res;
}

PODEF P_Mat4
p_mat4_translate_by(P_Mat4 m, Vec3f v)
{
    P_Mat4 res = m;
    res.m[12] = m.m[0] * v.x + m.m[4] * v.y + m.m[8]  * v.z + m.m[12];
    res.m[13] = m.m[1] * v.x + m.m[5] * v.y + m.m[9]  * v.z + m.m[13];
    res.m[14] = m.m[2] * v.x + m.m[6] * v.y + m.m[10] * v.z + m.m[14];
    res.m[15] = m.m[3] * v.x + m.m[7] * v.y + m.m[11] * v.z + m.m[15];
    return res;
}

PODEF P_Mat4
p_mat4_rotate_x(float rad)
{
    P_Mat4 res = p_mat4_identity();
    float c = p_cosf(rad);
    float s = p_sinf(rad);
    res.m[5]  =  c;
    res.m[6]  =  s;
    res.m[9]  = -s;
    res.m[10] =  c;
    return res;
}

PODEF P_Mat4
p_mat4_rotate_x_by(P_Mat4 m, float rad)
{
    return p_mat4_mul(m, p_mat4_rotate_x(rad));
}

PODEF P_Mat4
p_mat4_rotate_y(float rad)
{
    P_Mat4 res = p_mat4_identity();
    float c = p_cosf(rad);
    float s = p_sinf(rad);
    res.m[0]  =  c;
    res.m[2]  = -s;
    res.m[8]  =  s;
    res.m[10] =  c;
    return res;
}

PODEF P_Mat4
p_mat4_rotate_y_by(P_Mat4 m, float rad)
{
    return p_mat4_mul(m, p_mat4_rotate_y(rad));
}

PODEF P_Mat4
p_mat4_rotate_z(float rad)
{
    P_Mat4 res = p_mat4_identity();
    float c = p_cosf(rad);
    float s = p_sinf(rad);
    res.m[0] =  c;
    res.m[1] =  s;
    res.m[4] = -s;
    res.m[5] =  c;
    return res;
}

PODEF P_Mat4
p_mat4_perspective(float fov_rad, float aspect, float near_z, float far_z)
{
    P_Mat4 res = {0};
    float tan_half_fov = p_sinf(fov_rad * 0.5f) / p_cosf(fov_rad * 0.5f);

    res.m[0]  = 1.0f / (aspect * tan_half_fov);
    res.m[5]  = 1.0f / tan_half_fov;
    res.m[10] = far_z / (near_z - far_z);
    res.m[11] = -1.0f;
    res.m[14] = (near_z * far_z) / (near_z - far_z);
    return res;
}
