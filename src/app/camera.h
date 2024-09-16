#ifndef INCLUDE_APP_CAMERA_H_
#define INCLUDE_APP_CAMERA_H_

#include <core/core/math.h>

struct CameraMatrices {
  core::Mat4 projection_matrix, world2camera;
};
struct Camera {
  f32 hfov, near, far;
  core::Vec4 position = core::Vec4::Zero;
  // core::Vec4 scale    = core::Vec4::One;
  core::Quat rotation = core::Quat::Id;

  CameraMatrices matrices(f32 aspect_ratio) const {
    return {
        core::projection_matrix_from_hfov(near, far, hfov, aspect_ratio),
        rotation.conjugate().into_mat4() * core::translation_matrix(-position),
    };
  }
};

#endif // INCLUDE_APP_CAMERA_H_
