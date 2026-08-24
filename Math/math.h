/* ===========================================================================
 * MATH - Copyright @ Vasco Alves - See LICENSE at the end of file.
 *
 * Linear algebra and math for games.
 * No types. I'm tired of everyone having a Vec3 type.
 * Ops write in place.
 * Speed over accuracy.
 *
 * TYPES:
 * - vec2 = float[2]
 * - vec3 = float[3]
 * - mat4 = float[16] column-major
 * A pointer that is not backed correctly but the correct amount of memory
 * is undefined behaviour and misuse of the library.
 *
 * =========================================================================== */

#ifndef GUNSTACK_MATH_H
#define GUNSTACK_MATH_H

#define MATH_MAJOR "0"
#define MATH_MINOR "0"
#define MATH_PATCH "0"

/* CHANGE LOG
 * 0.0.0 - @vasco - prototyping
 */

#define MATH_PI      3.14159265358979323846f
#define MATH_PI2     (2.0f * MATH_PI)
#define MATH_PI_HALF (MATH_PI * 0.5f)
#define MATH_PI_POW2 (MATH_PI * MATH_PI)

#define MATH static inline

MATH void math_ceil(float *x);
MATH void math_sinf(float *x);
MATH void math_cosf(float *x);

MATH void math_vec2f(float *v, float x, float y);
MATH void math_vec2f_add(float *a, const float *b);
MATH void math_vec2f_sub(float *a, const float *b);
MATH void math_vec2f_mult(float *a, float s);
MATH void math_vec2f_dot(float *out, const float *a, const float *b);
MATH void math_vec2f_cross(float *out, const float *a, const float *b);

MATH void math_vec3f(float *v, float x, float y, float z);
MATH void math_vec3f_add(float *a, const float *b);
MATH void math_vec3f_sub(float *a, const float *b);
MATH void math_vec3f_mult(float *a, float s);
MATH void math_vec3f_dot(float *out, const float *a, const float *b);
MATH void math_vec3f_cross(float *a, const float *b);

MATH void math_mat4_identity(float *m);
MATH void math_mat4_mul(float *a, const float *b);
MATH void math_mat4_translate(float *m, const float *v);
MATH void math_mat4_translate_by(float *m, const float *v);
MATH void math_mat4_rotate_x(float *m, float rad);
MATH void math_mat4_rotate_x_by(float *m, float rad);
MATH void math_mat4_rotate_y(float *m, float rad);
MATH void math_mat4_rotate_y_by(float *m, float rad);
MATH void math_mat4_rotate_z(float *m, float rad);
MATH void math_mat4_perspective(float *m, float fov_rad, float aspect, float near_z, float far_z);

void
math_ceil(float *x)
{
	int base = (int)*x;
	*x = (*x > (float)base) ? (float)(base + 1) : (float)base;
}

void
math_sinf(float *x)
{
	float y, B, C, P;
	while (*x > MATH_PI)
		*x -= MATH_PI2;
	while (*x < -MATH_PI)
		*x += MATH_PI2;
	B = 4.0f / MATH_PI;
	C = -4.0f / MATH_PI_POW2;
	y = B * *x + C * *x * (*x < 0 ? -*x : *x);
	P = 0.225f;
	*x = P * (y * (y < 0 ? -y : y) - y) + y;
}

void
math_cosf(float *x)
{
	*x += MATH_PI_HALF;
	math_sinf(x);
}

void
math_vec2f(float *v, float x, float y)
{
	v[0] = x;
	v[1] = y;
}

void
math_vec2f_add(float *a, const float *b)
{
	a[0] += b[0];
	a[1] += b[1];
}

void
math_vec2f_sub(float *a, const float *b)
{
	a[0] -= b[0];
	a[1] -= b[1];
}

void
math_vec2f_mult(float *a, float s)
{
	a[0] *= s;
	a[1] *= s;
}

void
math_vec2f_dot(float *out, const float *a, const float *b)
{
	*out = a[0] * b[0] + a[1] * b[1];
}

void
math_vec2f_cross(float *out, const float *a, const float *b)
{
	*out = a[0] * b[1] - a[1] * b[0];
}

void
math_vec3f(float *v, float x, float y, float z)
{
	v[0] = x;
	v[1] = y;
	v[2] = z;
}

void
math_vec3f_add(float *a, const float *b)
{
	a[0] += b[0];
	a[1] += b[1];
	a[2] += b[2];
}

void
math_vec3f_sub(float *a, const float *b)
{
	a[0] -= b[0];
	a[1] -= b[1];
	a[2] -= b[2];
}

void
math_vec3f_mult(float *a, float s)
{
	a[0] *= s;
	a[1] *= s;
	a[2] *= s;
}

void
math_vec3f_dot(float *out, const float *a, const float *b)
{
	*out = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void
math_vec3f_cross(float *a, const float *b)
{
	float x = a[1] * b[2] - a[2] * b[1];
	float y = a[2] * b[0] - a[0] * b[2];
	float z = a[0] * b[1] - a[1] * b[0];
	a[0] = x;
	a[1] = y;
	a[2] = z;
}

void
math_mat4_identity(float *m)
{
	int i;
	for (i = 0; i < 16; i++)
		m[i] = 0.0f;
	m[0] = m[5] = m[10] = m[15] = 1.0f;
}

void
math_mat4_mul(float *a, const float *b)
{
	float res[16];
	int col, row, k;
	for (col = 0; col < 4; ++col) {
		for (row = 0; row < 4; ++row) {
			float sum = 0.0f;
			for (k = 0; k < 4; ++k)
				sum += a[k * 4 + row] * b[col * 4 + k];
			res[col * 4 + row] = sum;
		}
	}
	for (k = 0; k < 16; k++)
		a[k] = res[k];
}

void
math_mat4_translate(float *m, const float *v)
{
	math_mat4_identity(m);
	m[12] = v[0];
	m[13] = v[1];
	m[14] = v[2];
}

void
math_mat4_translate_by(float *m, const float *v)
{
	m[12] = m[0] * v[0] + m[4] * v[1] + m[8]  * v[2] + m[12];
	m[13] = m[1] * v[0] + m[5] * v[1] + m[9]  * v[2] + m[13];
	m[14] = m[2] * v[0] + m[6] * v[1] + m[10] * v[2] + m[14];
	m[15] = m[3] * v[0] + m[7] * v[1] + m[11] * v[2] + m[15];
}

void
math_mat4_rotate_x(float *m, float rad)
{
	float c = rad, s = rad;
	math_cosf(&c);
	math_sinf(&s);
	math_mat4_identity(m);
	m[5]  =  c;
	m[6]  =  s;
	m[9]  = -s;
	m[10] =  c;
}

void
math_mat4_rotate_x_by(float *m, float rad)
{
	float r[16];
	math_mat4_rotate_x(r, rad);
	math_mat4_mul(m, r);
}

void
math_mat4_rotate_y(float *m, float rad)
{
	float c = rad, s = rad;
	math_cosf(&c);
	math_sinf(&s);
	math_mat4_identity(m);
	m[0]  =  c;
	m[2]  = -s;
	m[8]  =  s;
	m[10] =  c;
}

void
math_mat4_rotate_y_by(float *m, float rad)
{
	float r[16];
	math_mat4_rotate_y(r, rad);
	math_mat4_mul(m, r);
}

void
math_mat4_rotate_z(float *m, float rad)
{
	float c = rad, s = rad;
	math_cosf(&c);
	math_sinf(&s);
	math_mat4_identity(m);
	m[0] =  c;
	m[1] =  s;
	m[4] = -s;
	m[5] =  c;
}

void
math_mat4_perspective(float *m, float fov_rad, float aspect, float near_z, float far_z)
{
	float half = fov_rad * 0.5f;
	float c = half, s = half;
	float tan_half_fov;
	int i;
	math_sinf(&s);
	math_cosf(&c);
	tan_half_fov = s / c;
	for (i = 0; i < 16; i++)
		m[i] = 0.0f;
	m[0]  = 1.0f / (aspect * tan_half_fov);
	m[5]  = 1.0f / tan_half_fov;
	m[10] = far_z / (near_z - far_z);
	m[11] = -1.0f;
	m[14] = (near_z * far_z) / (near_z - far_z);
}

#endif /* GUNSTACK_MATH_H */

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2026 Vasco Alves
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
