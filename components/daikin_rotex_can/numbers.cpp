#include "esphome/components/daikin_rotex_can/numbers.h"

namespace esphome {
namespace daikin_rotex_can {

void GenericNumber::control(float value) {
    this->publish_state(value);
    sendSet(m_pCanbus, value * get_config().divider);
}

TRequest::TVariant GenericNumber::handleValue(uint16_t value) {
    TRequest::TVariant variant = value / m_config.divider;
    publish_state(std::get<float>(variant));
    return variant;
}

}  // namespace seeed_mr24hpc1
}  // namespace esphome
