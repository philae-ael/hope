#ifndef INCLUDE_APP_CAMERA_H_
#define INCLUDE_APP_CAMERA_H_

#include <core/math.h>

struct CameraMatrices {
  math::Mat4 projection_matrix, world2camera;
};
struct Camera {
  f32 hfov, near, far;
  math::Vec4 position = math::Vec4::Zero;
  // Vec4 scale    = core::Vec4::One;
  math::Quat rotation = math::Quat::Id;

  CameraMatrices matrices(f32 aspect_ratio) const {
    return {
        math::projection_matrix_from_hfov(near, far, hfov, aspect_ratio),
        rotation.conjugate().into_mat4() * math::translation_matrix(-position),
    };
  }
};

#endif // INCLUDE_APP_CAMERA_H_
