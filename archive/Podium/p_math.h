typedef struct {int x, y; } Vec2i;
typedef struct {float x, y; } Vec2f;
typedef struct {float x, y, z; } Vec3f;
typedef struct { float m[16];} P_Mat4; // column-major: m[col * 4 + row]
PODEF int   p_ceil(float x);
PODEF float p_cosf(float x);
PODEF float p_sinf(float x);

PODEF Vec2f p_vec2f(float, float);
PODEF Vec2f p_vec2f_add(Vec2f, Vec2f);
PODEF float p_vec2f_cross(Vec2f, Vec2f);
PODEF float p_vec2f_dot(Vec2f, Vec2f);
PODEF Vec2f p_vec2f_mult(Vec2f a, float b);
PODEF Vec2f p_vec2f_sub(Vec2f, Vec2f);
PODEF Vec2i p_vec2i(float, float);
PODEF Vec2i p_vec2i_add(Vec2i, Vec2i);
PODEF float p_vec2i_cross(Vec2i, Vec2i);
PODEF float p_vec2i_dot(Vec2i, Vec2i);
PODEF Vec2i p_vec2i_mult(Vec2i a, float b);
PODEF Vec2i p_vec2i_sub(Vec2i, Vec2i);

PODEF Vec3f p_vec3f(float, float, float);

PODEF P_Mat4 p_mat4_identity(void);
PODEF P_Mat4 p_mat4_mul(P_Mat4 a, P_Mat4 b);
PODEF P_Mat4 p_mat4_translate(Vec3f v);
PODEF P_Mat4 p_mat4_translate_by(P_Mat4 m, Vec3f v);
PODEF P_Mat4 p_mat4_rotate_x(float rad);
PODEF P_Mat4 p_mat4_rotate_x_by(P_Mat4 m, float rad);
PODEF P_Mat4 p_mat4_rotate_y(float rad);
PODEF P_Mat4 p_mat4_rotate_y_by(P_Mat4 m, float rad);
PODEF P_Mat4 p_mat4_rotate_z(float rad);
PODEF P_Mat4 p_mat4_perspective(float fov_rad, float aspect, float near_z, float far_z);
