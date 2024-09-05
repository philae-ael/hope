#include <core/containers/vec.h>
#include <core/core.h>
#include <core/vulkan.h>
#include <core/vulkan/image.h>
#include <core/vulkan/subsystem.h>

using namespace core::literals;

struct res_t {};
using resh = core::handle_t<res_t>;

struct ResDef {};
struct RefAdapter {};
union GlobalState;
struct Runner {
  GlobalState (*run)();
};

union Data {
  enum class Op {
    SpawnRessource,
    AdaptRessource,
    Run,
  } op;
  struct {
    Op op = Op::SpawnRessource;
    ResDef res_def;
  } SpawnRessource;

  struct {
    Op op = Op::AdaptRessource;
    resh to_adapt;
    RefAdapter adapter;
    resh* adapted;
  } Adapt;

  struct {
    Op op = Op::Run;
    // GlobalState ()
    Runner run;
  } Run;
};
//
// union GlobalState {
//   enum class Op {
//     GetVideoSubsystem,
//   } op;
//
//   struct {
//     Op op = Op::GetVideoSubsystem;
//     ResDef res_def;
//   } GetVideoSubsystem;
// };
//
union AST {
  core::vec<Data> data;
};

namespace details_ {
template <usize N, class... Ts>
struct nth_t;

template <class T, class... As>
struct nth_t<0, T, As...> {
  using type = T;
};
template <usize N, class T, class... As>
struct nth_t<N, T, As...> {
  using type = nth_t<N - 1, As...>;
};
} // namespace details_

template <class... Ts>
struct type_pack {
  static constexpr usize count = sizeof...(Ts);
  template <usize N>
  using nth = details_::nth_t<N, Ts...>::type;
};

template <class F>
struct function_traits : public function_traits<decltype(&F::operator())> {};

template <class Ret, class Class, class... Args>
struct function_traits<Ret (Class::*)(Args...) const> : public function_traits<Ret(Args...)> {};

template <class Ret, class Class, class... Args>
struct function_traits<Ret (Class::*)(Args...)> : public function_traits<Ret(Args...)> {};

template <class Ret, class... Args>
struct function_traits<Ret(Args...)> {
  using ret  = Ret;
  using args = type_pack<Args...>;
};

template <class T>
struct ressource_traits;

template <class T>
concept is_ressource = requires {
  typename ressource_traits<T>::config;
  typename ressource_traits<T>::sync;
};

struct Ctx {
  core::Arena* ar;

  template <class F>
  inline function_traits<F>::ret pass(const F& f) {
    Ctx child{ar};
    return f(child);
  }

  template <class Res>
    requires is_ressource<Res>
  Res spawn_named(
      core::hstr8 name,
      ressource_traits<Res>::config config,
      ressource_traits<Res>::sync sync
  ) {
    return {};
  }
};

using resimage = core::handle_t<vk::image2D>;
template <>
struct ressource_traits<resimage> {
  using config = vk::image2D::Config;
  using sync   = vk::image2D::Sync;
};

void render_imgui(Ctx& ctx) {
  ctx.pass([](Ctx& ctx) {
    auto imgui_output = ctx.spawn_named<resimage>(
        "imgui output"_hs,
        {
            .format = VK_FORMAT_B8G8R8A8_UNORM,
            .extent = {.swapchain = {}},
        },
        {
            VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        }
    );

    return imgui_output;
  });
}
