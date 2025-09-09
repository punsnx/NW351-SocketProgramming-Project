#include "timer.hpp"
#include <algorithm>

namespace rudp {

void RttEstimator::sample(std::chrono::milliseconds rtt) {
  const double x = static_cast<double>(rtt.count());

  if (!init_) {
    init_  = true;
    srtt_  = x;
    rdev_  = x / 2.0;
    return;
  }

  // Jacobson/Karels
  const double err = x - srtt_;
  srtt_ += alpha_ * err;
  rdev_ += delta_ * (std::abs(err) - rdev_);
}

std::chrono::milliseconds RttEstimator::timeout() const {
  if (!init_) return std::chrono::milliseconds(500); // default
  double to = mu_ * srtt_ + phi_ * rdev_;
  // clamp to sane bounds
  to = std::clamp(to, 100.0, 5000.0);
  return std::chrono::milliseconds(static_cast<int>(to));
}

void RttEstimator::reset() {
  init_ = false;
  srtt_ = 0.0;
  rdev_ = 0.0;
}

} // namespace rudp
