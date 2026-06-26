#include "rivetmesh/status.h"

namespace rivetmesh {

const char* errcName(Errc code) {
  switch (code) {
    case Errc::eof:
      return "eof";
    case Errc::bad_magic:
      return "bad_magic";
    case Errc::bad_version:
      return "bad_version";
    case Errc::bad_number:
      return "bad_number";
    case Errc::syntax:
      return "syntax";
    case Errc::limit:
      return "limit";
    case Errc::checksum:
      return "checksum";
    case Errc::state:
      return "state";
    case Errc::unknown_section:
      return "unknown_section";
  }
  return "unknown";
}

}  // namespace rivetmesh
