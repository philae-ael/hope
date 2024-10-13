#ifndef INCLUDE_APP_CAMERA_H_
#define INCLUDE_APP_CAMERA_H_

#include <core/math.h>

struct CameraMatrices {
  math::Mat4 projection_matrix, camera_from_world, world_from_camera;
  f32 half_far_plane_width, half_far_plane_height, near, far;
};

struct Camera {
  f32 hfov, near, far;
  math::Vec4 position = math::Vec4::Zero;
  // Vec4 scale    = core::Vec4::One;
  math::Quat rotation = math::Quat::Id;

  CameraMatrices matrices(f32 aspect_ratio) const {
    auto half_far_plane_width  = far * std::tan(hfov / 2);
    auto half_far_plane_height = half_far_plane_width / aspect_ratio;
    return {
        math::projection_matrix_from_hfov(near, far, hfov, aspect_ratio),
        rotation.conjugate().into_mat4() * math::translation_matrix(-position),
        math::translation_matrix(position) * rotation.into_mat4(),
        half_far_plane_width,
        half_far_plane_height,
        near,
        far,
    };
  }
};

#endif // INCLUDE_APP_CAMERA_H_
