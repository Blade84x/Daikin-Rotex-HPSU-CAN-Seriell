#include "esphome/components/daikin_rotex_can/requests.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace daikin_rotex_can {

TRequests::TRequests()
: m_requests()
, m_pCanbus(nullptr)
{
}

void TRequests::add(esphome::daikin_rotex_can::TRequest const& request) {
    m_requests.push_back(request);
}

void TRequests::removeInvalidRequests() {
    m_requests.erase(
        std::remove_if(
            m_requests.begin(),
            m_requests.end(),
            [](TRequest const& request) { return !request.isGetSupported(); }
        ),
        m_requests.end()
    );
}

bool TRequests::sendNextPendingGet() {
    TRequest* pRequest = getNextRequestToSend();
    if (pRequest != nullptr) {
        pRequest->sendGet(m_pCanbus);
        return true;
    }
    return false;
}

void TRequests::sendGet(std::string const& request_name) {
    const auto it = std::find_if(
        m_requests.begin(),
        m_requests.end(),
        [& request_name](auto& request) {
            return request.getName() == request_name && request.isGetSupported();
        }
    );

    if (it != m_requests.end()) {
        it->sendGet(m_pCanbus);
    } else {
        ESP_LOGE("sendGet", "Unknown request: %s", request_name.c_str());
    }
}

void TRequests::sendSet(std::string const& request_name, float value) {
    const auto it = std::find_if(m_requests.begin(), m_requests.end(),
        [&request_name](auto& request) { return request.isSetter() && request.getName() == request_name; }
    );
    if (it != m_requests.end()) {
        it->sendSet(m_pCanbus, value);
    } else {
        ESP_LOGE("sendSet", "Unknown request: %s", request_name.c_str());
    }
}

void TRequests::handle(uint32_t can_id, std::vector<uint8_t> const& responseData) {
    bool bHandled = false;
    const uint32_t timestamp = millis();
    for (auto& request : m_requests) {
        if (request.isMatch(can_id, responseData)) {
            request.handle(can_id, responseData, timestamp);
            bHandled = true;
        }
    }
    if (!bHandled) {
        Utils::log("unhandled", "can_id<%s> data<%s>", Utils::to_hex(can_id).c_str(), Utils::to_hex(responseData).c_str());
    }
}

TRequest* TRequests::getNextRequestToSend() {
    const uint32_t timestamp = millis();
    const uint32_t interval = static_cast<uint32_t>(10/*id(update_interval).state*/) * 1000;

    for (auto& request : m_requests) {
        if (request.hasSendGet()) {
            if ((timestamp > (request.getLastUpdate() + interval)) && !request.inProgress()) {
                return &request;
            }
        }
    }
    return nullptr;
}

}
}