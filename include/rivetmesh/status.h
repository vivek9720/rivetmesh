#pragma once

#include <cstddef>
#include <string>
#include <utility>

namespace rivetmesh {

enum class Errc {
  eof,
  bad_magic,
  bad_version,
  bad_number,
  syntax,
  limit,
  checksum,
  state,
  unknown_section,
};

struct Status {
  Errc code = Errc::state;
  std::string message;
  std::size_t offset = 0;
};

inline Status fail(Errc code, std::string message, std::size_t offset = 0) {
  return Status{code, std::move(message), offset};
}

template <typename T>
class Outcome {
 public:
  static Outcome ok(T value) {
    Outcome out;
    out.ok_ = true;
    out.value_ = std::move(value);
    return out;
  }

  static Outcome err(Status status) {
    Outcome out;
    out.ok_ = false;
    out.status_ = std::move(status);
    return out;
  }

  bool ok() const { return ok_; }
  explicit operator bool() const { return ok_; }
  const T& value() const { return value_; }
  T& value() { return value_; }
  T take() { return std::move(value_); }
  const Status& status() const { return status_; }

 private:
  bool ok_ = false;
  T value_{};
  Status status_{};
};

template <>
class Outcome<void> {
 public:
  static Outcome success() {
    Outcome out;
    out.ok_ = true;
    return out;
  }

  static Outcome err(Status status) {
    Outcome out;
    out.ok_ = false;
    out.status_ = std::move(status);
    return out;
  }

  bool ok() const { return ok_; }
  explicit operator bool() const { return ok_; }
  const Status& status() const { return status_; }

 private:
  bool ok_ = false;
  Status status_{};
};

const char* errcName(Errc code);

}  // namespace rivetmesh
