#pragma once
#include <chrono>

namespace rudp {
class RttEstimator {
 public:
  void sample(std::chrono::milliseconds rtt);
  std::chrono::milliseconds timeout() const;
  void reset();
 private:
  bool init_ = false;
  double srtt_ = 0.0;   // Estimated RTT
  double rdev_ = 0.0;   // Deviation
  const double alpha_ = 0.125; // α
  const double delta_ = 0.25;  // δ
  const double mu_   = 1.0;    // μ
  const double phi_  = 4.0;    // φ
};
}
