#include "stout/cfb/sector_io.h"
#include "stout/io/memory_lock_bytes.h"

// sector_io is a header-only template class.
// This TU verifies it compiles with memory_lock_bytes.
namespace stout::cfb {
    template class sector_io<io::memory_lock_bytes>;
}
