#pragma once 

#include <cmath> 
#include <stdexcept> 

//*******************************************
// Small Library Created For Math Used For Game
//*******************************************
namespace gamemath {
	constexpr float PI = 3.14159265358979323846f;

	class vec3 {
	public:

		float x, y, z;
		vec3(float x, float y, float z) : x(x), y(y), z(z) {

		}

		vec3() {
			x = 0; y = 0; z = 0;
		}


		// Indexed component access: 0 -> x, 1 -> y, 2 -> z.
		float& operator[](int i) {
			if (i < 0 || i >= 3)  throw std::runtime_error("out of range");

			switch (i) {
			case 0: return x;
			case 1: return y;
			case 2: return z;

			}
		}
		vec3 operator-(const vec3& v) const {
			return vec3(this->x - v.x, this->y - v.y, this->z - v.z);
		}
		vec3 operator+(const vec3& v) const {
			return vec3(this->x + v.x, this->y + v.y, this->z + v.z);
		}
		vec3 operator*(float scaler) {
			return vec3(this->x * scaler, this->y * scaler, this->z * scaler);
		}
		vec3& operator+=(const vec3& v) {
			this->x += v.x;
			this->y += v.y;
			this->z += v.z;

			return *this;
		}
		vec3& operator-=(const vec3& v) {
			this->x -= v.x;
			this->y -= v.y;
			this->z -= v.z;

			return *this;
		}

	private:
		friend float* value_ptr(vec3& v);
	};

	// Forward declaration allows vec4 to declare operations involving mat4 before mat4 is defined.
	class mat4;
	// Four-component vector used for homogeneous coordinates and matrix rows/columns.
	class vec4 {
	public:
		float x, y, z, w;
		vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {

		}
		vec4() {
			x = 0; y = 0; z = 0; w = 0;
		}

		float& operator[](int i) {
			if (i < 0 || i >= 4) throw std::runtime_error("out of range");


			switch (i) {
			case 0: return x;
			case 1: return y;
			case 2: return z;
			case 3: return w;

			}
		}

		vec4 operator*(mat4 m);

		vec4 operator-(const vec4& v) const {
			return vec4(this->x - v.x, this->y - v.y, this->z - v.z, this->w - v.w);
		}
		vec4 operator+(const vec4& v) const {
			return vec4(this->x + v.x, this->y + v.y, this->z + v.z, this->w + v.w);
		}
		vec4 operator*(float scaler) {
			return vec4(this->x * scaler, this->y * scaler, this->z * scaler, this->w * scaler);
		}
		vec4& operator+=(const vec4& v) {
			this->x += v.x;
			this->y += v.y;
			this->z += v.z;
			this->w += v.w;

			return *this;
		}
		vec4& operator-=(const vec4& v) {
			this->x -= v.x;
			this->y -= v.y;
			this->z -= v.z;
			this->w -= v.w;

			return *this;
		}

	private:
		friend float* value_ptr(vec4& v);
	};

	// 4x4 matrix stored as four vec4 objects.
	class mat4 {
#define DEFAULT_MAT_VALUE 1.0f 
	public:
		mat4(float t) {
			initializeMat(t);
		}
		mat4() {
			initializeMat(DEFAULT_MAT_VALUE);
		}
		vec4 operator*(vec4 v) {
			return vec4(m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z + m[0][3] * v.w,
				m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z + m[1][3] * v.w,
				m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z + m[2][3] * v.w,
				m[3][0] * v.x + m[3][1] * v.y + m[3][2] * v.z + m[3][3] * v.w);
		}
		vec4& operator[](int i) {
			if (i < 0 || i >= 4) throw std::runtime_error("out of range");
			return m[i];
		}
	private:
		vec4 m[4];
		// Initialize a diagonal matrix whose diagonal entries are t.
		void initializeMat(float t) {
			for (int row = 0; row < 4; ++row) {
				m[row][row] = t;
			}
		}
		friend float* value_ptr(mat4& matrix);
	};

	// Row-vector style vec4 * mat4 multiplication.
	inline vec4 vec4::operator*(mat4 m) {
		return vec4(x * m[0][0] + y * m[1][0] + z * m[2][0] + w * m[3][0],
			x * m[0][1] + y * m[1][1] + z * m[2][1] + w * m[3][1],
			x * m[0][2] + y * m[1][2] + z * m[2][2] + w * m[3][2],
			x * m[0][3] + y * m[1][3] + z * m[2][3] + w * m[3][3]);
	}

	inline vec3 normalize(vec3 v) {
		float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
		return vec3(v.x / length, v.y / length, v.z / length);
	}

	inline float dot(vec3 a, vec3 b) {
		return a.x * b.x + a.y * b.y + a.z * b.z;
	}

	// Cross product returns a vector perpendicular to both input vectors.
	inline vec3 cross(vec3 a, vec3 b) {
		return vec3(a.y * b.z - b.y * a.z,
			b.x * a.z - a.x * b.z,
			a.x * b.y - b.x * a.y
		);
	}

	//Pointer to x value of matrix first row returned so OpenGL can read the matrix as contiguous float data.
	inline float* value_ptr(mat4& mat) {
		return &(mat.m[0].x);
	}

	// Convert an angle from degrees to radians.
	inline float radians(float degree) {
		return degree * PI / 180.0f;
	}

	// Build a right-handed OpenGL perspective matrix with NDC z in [-1, 1].
	inline mat4 perspective(float fov, float aspect, float znear, float zfar) {
		//arranged in column major 
		mat4 result(0.0f);
		result[0][0] = 1.0f / (std::tan(fov / 2.0f) * aspect);
		result[1][1] = 1.0f / (std::tan(fov / 2.0f));
		result[2][2] = (-znear - zfar) / (zfar - znear);
		result[2][3] = -1.0f;
		result[3][2] = (-2.0f * zfar * znear) / (zfar - znear);

		return result;
	}

	// Return a pointer to the first vec3 component
	inline float* value_ptr(vec3& v) {
		return &v.x;
	}


	// Return a pointer to the first vec4 component
	inline float* value_ptr(vec4& v) {
		return &v.x;
	}

	inline mat4 lookAt(const vec3& eye, const vec3& center, const vec3& up) {
		//arranged in column major 
		mat4 result;
		vec3  f = normalize(center - eye);
		vec3  r = normalize(cross(f, up));
		vec3  u = cross(r, f);

		//build matrix that combines both translation and rotation transformation to put vertices from world space into camera space
		result[0][0] = r.x;
		result[0][1] = u.x;
		result[0][2] = -f.x;
		result[1][0] = r.y;
		result[1][1] = u.y;
		result[1][2] = -f.y;
		result[2][0] = r.z;
		result[2][1] = u.z;
		result[2][2] = -f.z;
		result[3][0] = -dot(eye, r);
		result[3][1] = -dot(eye, u);
		result[3][2] = dot(eye, f);
		result[3][3] = 1.0f;

		return result;


	}
}