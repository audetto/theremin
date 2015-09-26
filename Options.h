#include <cstddef>

namespace ASI
{
  struct Options
  {
    size_t octaves;          // default to 2
    double decay;            // default to 50
  };

  bool fillOptions(int argc, char** argv, Options & options);
}
