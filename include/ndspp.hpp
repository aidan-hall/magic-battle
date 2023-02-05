#ifndef NDSPP_H
#define NDSPP_H

#include "nds/arm9/math.h"
#include <cstdint>

namespace nds {

/* a fixed-point number */
struct fix {
  /* state */
  int32_t bits;

  static constexpr fix from_int(int32_t n) { return fix{inttof32(n)}; }
  static constexpr fix from_float(float f) { return fix{floattof32(f)}; }

  /* conversions */
  explicit operator int32_t() const { return f32toint(bits); }
  explicit operator float() const { return f32tofloat(bits); }

  /* methods */
  static inline fix sqrt(const fix &f) { return {sqrtf32(f.bits)}; }
};

inline fix operator*(const fix &a, const fix &b) {
  return nds::fix{mulf32(a.bits, b.bits)};
}
inline fix operator/(const fix &num, const fix &den) {
  return nds::fix{divf32(num.bits, den.bits)};
}
inline fix operator+(const fix &a, const fix &b) {
  return nds::fix{a.bits + b.bits};
}
inline fix operator-(const fix &a, const fix &b) {
  return nds::fix{a.bits - b.bits};
}

inline fix operator*(const fix &a, const int32_t &b) {
  return a * fix::from_int(b);
}
inline fix operator/(const fix &num, const int32_t &den) {
  return num/fix::from_int(den);
}
inline fix operator+(const fix &a, const int32_t &b) {
  return a + fix::from_int(b);
}
inline fix operator-(const fix &a, const int32_t &b) {
  return a - fix::from_int(b);
}


inline bool operator<(const fix &lhs, const fix &rhs) {
  return lhs.bits < rhs.bits;
}
inline bool operator>(const fix &lhs, const fix &rhs) { return rhs < lhs; }
inline bool operator<=(const fix &lhs, const fix &rhs) { return !(lhs > rhs); }
inline bool operator>=(const fix &lhs, const fix &rhs) { return !(lhs < rhs); }

} // namespace nds

#endif /* NDSPP_H */
