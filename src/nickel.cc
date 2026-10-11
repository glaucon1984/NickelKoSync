#include "nickel.h"

namespace kosync {
namespace nickel {

const Api &api() {
    static const Api value;
    return value;
}

}
}
