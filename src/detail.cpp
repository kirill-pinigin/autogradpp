#include <ATen/Config.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>

#if AT_CUDA_ENABLED()
#include <THC/THCTensorRandom.h>
#include <cuda.h>
#include <cuda_runtime.h>
#endif

#include "detail.h"

namespace autograd {
namespace detail {
tag::Engine engine;
}

void backward(Variable loss, bool keep_graph) {
  tag::edge_list edgelst;
  tag::variable_list varlst;
  edgelst.emplace_back(loss.grad_fn(), loss.output_nr());
  varlst.emplace_back(Var(at::ones_like(loss.data()), false));
  // create_graph should be set to true when we want to support double bwd
  detail::engine.execute(edgelst, varlst, keep_graph, false);
}

void backward(Tensor loss, bool keep_graph) {
  Variable tmp(loss);
  backward(tmp, keep_graph);
}

void setSeed(uint64_t seed) {
  at::globalContext().defaultGenerator(at::Backend::CPU).manualSeed(seed);
#if AT_CUDA_ENABLED()
  if (getNumGPUs() > 0) {
    THCRandom_manualSeedAll(at::globalContext().lazyInitCUDA(), seed);
  }
#endif
};

int getNumGPUs() {
#if AT_CUDA_ENABLED()
  int count;
  auto err = cudaGetDeviceCount(&count);
  if (err == cudaErrorNoDevice) {
    return 0;
  } else if (err != cudaSuccess) {
    std::string msg = "CUDA error (";
    msg += std::to_string(err);
    msg += "): ";
    msg += cudaGetErrorString(err);
    throw std::runtime_error(msg);
  }
  return count;
#else
  return 0;
#endif
}

bool hasCuda() {
  return getNumGPUs() > 0;
}

bool hasCudnn() {
  return hasCuda() && AT_CUDNN_ENABLED();
}

#define GEN_TYPE(TYP, NAME) \
  Variant::Variant(TYP x) { variant_ = x; };             \
  bool Variant::is ## NAME () const { return variant_.is<TYP>(); }; \
  TYP Variant::get ## NAME () const { return variant_.get<TYP>(); };

Variant::Variant(Tensor x) : Variant(Variable(x)) { }
Variant::Variant(Variable x) { variant_ = x; }
Variant::Variant(const std::string& x) { variant_ = x; }
Variant::Variant(std::vector<Variant>& x) { variant_ = x; }
Variant::Variant(std::vector<Variant>&& x) { variant_ = std::move(x); }
Variant::Variant(std::initializer_list<Variable> l) {
  auto lst = std::vector<Variant>();
  for (auto& var : l) {
    lst.emplace_back(var);
  }
  variant_ = lst;
}
Variant::Variant(std::unordered_map<std::string, Variant>& x) { variant_ = x; };
Variant::Variant(std::unordered_map<std::string, Variant>&& x) { variant_ = std::move(x); }

Variable const& Variant::get() const { return variant_.get<Variable>(); }
std::string const& Variant::getString() const { return variant_.get<std::string>(); }
std::vector<Variant> const& Variant::getList() const {
  return variant_.get<mapbox::util::recursive_wrapper<std::vector<Variant>>>().get();
}
std::unordered_map<std::string, Variant> const& Variant::getDict() const {
  return variant_.get<mapbox::util::recursive_wrapper<std::unordered_map<std::string, Variant>>>().get();
}
bool Variant::isVariable() const {
  return variant_.is<Variable>();
}
bool Variant::isString() const {
  return variant_.is<std::string>();
}
bool Variant::isList() const {
  return variant_.is<mapbox::util::recursive_wrapper<std::vector<Variant>>>();
}
bool Variant::isDict() const {
  return variant_.is<mapbox::util::recursive_wrapper<std::unordered_map<std::string, Variant>>>();
}

GEN_TYPE(float,      Float);
GEN_TYPE(double,     Double);
GEN_TYPE(bool,       Bool);
GEN_TYPE(int32_t,    Int32);
GEN_TYPE(int64_t,    Int64);

Tensor const& Variant::data() const {
  return get().data();
}
bool Variant::defined() const {
  return get().defined();
}
Variable Variant::detach() const {
  return get().detach();
}
at::Type& Variant::type() const {
  return get().type();
}

#undef GEN_TYPE

} // namespace autograd
