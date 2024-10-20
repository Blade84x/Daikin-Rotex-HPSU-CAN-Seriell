#include "esphome/components/daikin_rotex_can/request.h"
#include "esphome/components/daikin_rotex_can/selects.h"
#include "esphome/components/esp32_can/esp32_can.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace daikin_rotex_can {

std::array<uint16_t, 7> TRequest::calculate_reponse(TMessage const& message) {
    const uint16_t DC = 0xFFFF;
    std::array<uint16_t, 7> response = {DC, DC, DC, DC, DC, DC, DC};
    if (message[2] == 0xFA) {   // https://github.com/crycode-de/ioBroker.canbus/blob/master/well-known-messages/configs/rotex-hpsu.md
        response[2] = message[2];
        response[3] = message[3];
        response[4] = message[4];
    } else {
        response[2] = message[2];
    }
    return response;
}

GenericSelect* TRequest::get_select() const {
    return dynamic_cast<GenericSelect*>(m_entity);
}

bool TRequest::isGetInProgress() const {
    uint32_t mil = millis();
    return m_last_get_timestamp > m_last_handle_timestamp && ((mil - m_last_get_timestamp) < 3*1000); // Consider 3 sek => package is lost
}

bool TRequest::isMatch(uint32_t can_id, TMessage const& responseData) const {
    const bool is_set = (responseData[0] & 0x0F) == 0x00;
    const bool is_response = (responseData[0] & 0x0F) == 0x02;

    const bool is_our_response = can_id == m_can_id;                                    // Listen for responses caused by our sendGet
    const bool is_rocon_panel_response = can_id == 0x10A && (is_response || is_set);    // Listen for responses and sets caused by RoCon control panel (0x10A)

    const bool is_valid = is_our_response || is_rocon_panel_response;

    if (is_valid) {
        for (uint32_t index = 0; index < responseData.size(); ++index) {
            if (m_expected_reponse[index] != DC && responseData[index] != m_expected_reponse[index]) {
                return false;
            }
        }
        return responseData.size() == 7;
    }
    return false;
}

bool TRequest::handle(uint32_t can_id, TMessage const& responseData, uint32_t timestamp) {
    if (isMatch(can_id, responseData)) {

        TRequest::TVariant variant;

        if (m_config.data_offset > 0 && (m_config.data_offset + m_config.data_size) <= 7) {
            if (m_config.data_size >= 1 && m_config.data_size <= 2) {
                const uint16_t value = m_config.handle_lambda_set ? m_config.handle_lambda(responseData) :
                    (
                        m_config.data_size == 2 ?
                        (((responseData[m_config.data_offset] << 8) + responseData[m_config.data_offset + 1])) :
                        (responseData[m_config.data_offset])
                    );
                variant = handleValue(value);
            } else {
                Utils::call_later([this](){
                    ESP_LOGE("validateConfig", "Invalid data size: %d", m_config.data_size);
                });
            }
        } else {
            Utils::call_later([this](){
                ESP_LOGE("validateConfig", "Invalid data_offset: %d", m_config.data_offset);
            });
        }

        m_post_handle_lambda(this);

        std::string value;
        if (std::holds_alternative<uint32_t>(variant)) {
            value = std::to_string(std::get<uint32_t>(variant));
        } else if (std::holds_alternative<uint8_t>(variant)) {
            value = std::to_string(std::get<uint8_t>(variant));
        } else if (std::holds_alternative<float>(variant)) {
            value = std::to_string(std::get<float>(variant));
        } else if (std::holds_alternative<bool>(variant)) {
            value = std::get<bool>(variant);
        } else if (std::holds_alternative<std::string>(variant)) {
            value = std::get<std::string>(variant);
        } else {
            value = "Unsupported value type!";
        }

        Utils::log("handle ", "%s<%s> can_id<%s> data<%s>",
            getName().c_str(), value.c_str(), Utils::to_hex(can_id).c_str(), Utils::to_hex(responseData).c_str());

        m_last_handle_timestamp = timestamp;
        return true;
    }
    return false;
}

bool TRequest::sendGet(esphome::esp32_can::ESP32Can* pCanBus) {
    if (pCanBus == nullptr) {
        ESP_LOGE("sendGet", "pCanbus is null!");
        return false;
    }

    const uint32_t can_id = 0x680;
    const bool use_extended_id = false;

    pCanBus->send_data(can_id, use_extended_id, { m_command.begin(), m_command.end() });

    Utils::log("sendGet", "%s can_id<%s> command<%s>",
        getName().c_str(), Utils::to_hex(can_id).c_str(), Utils::to_hex(m_command).c_str());

    m_last_get_timestamp = millis();
    return true;
}

bool TRequest::sendSet(esphome::esp32_can::ESP32Can* pCanBus, float value) {
    if (pCanBus == nullptr) {
        ESP_LOGE("sendSet", "pCanbus is null!");
        return false;
    }

    const uint32_t can_id = 0x680;
    const bool use_extended_id = false;

    TMessage command = TMessage(m_config.command);
    command[0] = 0x30;
    command[1] = 0x00;
    if (m_config.set_lambda_set) {
        m_config.set_lambda(command, value);
    } else {
        Utils::setBytes(command, value, m_config.data_offset, m_config.data_size);
        //const int16_t ivalue = static_cast<int16_t>(value); // converte negative values. E.g. -15 to 0xFF6A
        //const uint16_t uvalue = static_cast<int16_t>(ivalue);
        //Utils::setBytes(command, uvalue, m_config.data_offset, m_config.data_size);
    }

    pCanBus->send_data(can_id, use_extended_id, { command.begin(), command.end() });
    Utils::log("sendSet", "name<%s> value<%f> can_id<%s> command<%s>",
        getName().c_str(), value, Utils::to_hex(can_id).c_str(), Utils::to_hex(command).c_str());

    sendGet(pCanBus);

    return true;
}

}
}