// Copyright (C) 2018 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <grpcpp/grpcpp.h>
#include <iostream>
#include <memory>
#include <string>

#include "android/base/Log.h"
#include "android/console.h"
#include "android/emulation/control/EmulatorService.h"
#include "android/emulation/control/emulator_controller.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

namespace android {
namespace emulation {
namespace control {

class EmulatorControllerServiceImpl : public EmulatorControllerService {
public:
    void stop() override { mServer->Shutdown(); }

    EmulatorControllerServiceImpl(EmulatorController::Service* service,
                                  grpc::Server* server)
        : mService(service), mServer(server) {}

private:
    std::unique_ptr<EmulatorController::Service> mService;
    std::unique_ptr<grpc::Server> mServer;
};

// Logic and data behind the server's behavior.
class EmulatorControllerImpl final : public EmulatorController::Service {
public:
    EmulatorControllerImpl(const AndroidConsoleAgents* agents)
        : mAgents(agents) {}

    Status setRotation(ServerContext* context,
                       const Rotation* request,
                       Rotation* reply) override {
        LOG(VERBOSE) << request->DebugString().c_str();;
        mAgents->emu->rotate((SkinRotation)request->rotation());
        return getRotation(context, nullptr, reply);
        return Status::OK;
    }

    Status getRotation(ServerContext* context,
                       const ::google::protobuf::Empty* request,
                       Rotation* reply) override {
        reply->set_rotation((Rotation_SkinRotation)mAgents->emu->getRotation());
        LOG(VERBOSE) << reply->DebugString().c_str();;
        return Status::OK;
    }

    Status setBattery(ServerContext* context,
                       const BatteryState* request,
                       BatteryState* reply) override {
        LOG(VERBOSE) << reply->DebugString().c_str();
        auto battery = mAgents->battery;
        battery->setHasBattery(request->hasbattery());
        battery->setIsBatteryPresent(request->ispresent());
        battery->setIsCharging(request->status() == BatteryState_BatteryStatus_BATTERY_STATUS_CHARGING);
        battery->setCharger((BatteryCharger) request->charger());
        battery->setChargeLevel(request->chargelevel());
        battery->setHealth((BatteryHealth) request->health());
        battery->setStatus((BatteryStatus) request->status());
        return getBattery(context, nullptr, reply);
    }

    Status getBattery(ServerContext* context,
                       const ::google::protobuf::Empty* request,
                       BatteryState* reply) override {
        auto battery = mAgents->battery;
        reply->set_hasbattery(battery->hasBattery());
        reply->set_ispresent(battery->present());
        reply->set_charger((BatteryState_BatteryCharger) battery->charger());
        reply->set_chargelevel(battery->chargeLevel());
        reply->set_health((BatteryState_BatteryHealth) battery->health());
        reply->set_status((BatteryState_BatteryStatus) battery->status());
        LOG(VERBOSE) << reply->DebugString().c_str();;
        return Status::OK;
    }

    Status setGps(ServerContext* context,
                       const GpsState* request,
                       GpsState* reply) override {
        auto location = mAgents->location;
        struct  timeval tVal;
        memset(&tVal, 0, sizeof(tVal));
        gettimeofday(&tVal, NULL);

        location->gpsSetPassiveUpdate(request->passiveupdate());
        location->gpsSendLoc(request->latitude(), request->longitude(),
                             request->elevation(),
                             request->speed(),
                             request->heading(),
                             request->satellites(), &tVal);

        LOG(VERBOSE) << request->DebugString().c_str();;
        return getGps(context, nullptr, reply);
    }

    Status getGps(ServerContext* context,
                       const ::google::protobuf::Empty* request,
                       GpsState* reply) override {

        auto location = mAgents->location;
        double lat, lon, speed, heading, elevation;
        int32_t count;

        // TODO(jansene):Implement in underlying agent.
        reply->set_passiveupdate(location->gpsGetPassiveUpdate());
        location->gpsGetLoc(&lat, &lon, &elevation, &speed, &heading, &count);

        reply->set_latitude(lat);
        reply->set_longitude(lon);
        reply->set_speed(speed);
        reply->set_heading(heading);
        reply->set_elevation(elevation);
        reply->set_satellites(count);

        LOG(VERBOSE) << reply->DebugString().c_str();;
        return Status::OK;
    }

    Status sendTouch(ServerContext* context,
                     const TouchEvent* request,
                     ::google::protobuf::Empty* reply) override {
        LOG(VERBOSE) << request->DebugString().c_str();
        mAgents->finger->setTouch(request->istouching(), request->touchid());
        return Status::OK;
    }

    Status sendKey(ServerContext* context,
                     const KeyEvent* request,
                     ::google::protobuf::Empty* reply) override {
        LOG(VERBOSE) << request->DebugString().c_str();;
        mAgents->user_event->sendKeyCode(request->key());
        return Status::OK;
    }

    Status sendMouse(ServerContext* context,
                     const MouseEvent* request,
                     ::google::protobuf::Empty* reply) override {
        LOG(VERBOSE) << request->DebugString().c_str();;
        mAgents->user_event->sendMouseEvent(request->x(), request->y(),
                                            request->z(), request->buttons());
        return Status::OK;
    }

    Status sendRotary(ServerContext* context,
                     const RotaryEvent* request,
                     ::google::protobuf::Empty* reply) override {
        mAgents->user_event->sendRotaryEvent(request->delta());
        LOG(VERBOSE) << request->DebugString().c_str();;
        return Status::OK;
    }

    Status getVmConfiguration(ServerContext* context,
                              const ::google::protobuf::Empty* request,
                              VmConfiguration* reply) override {

        ::VmConfiguration config;
        mAgents->vm->getVmConfiguration(&config);
        reply->set_hypervisortype( (VmConfiguration_VmHypervisorType) (config.hypervisorType));
        reply->set_numberofcpucores(config.numberOfCpuCores);
        reply->set_ramsizebytes(config.ramSizeBytes);
        LOG(VERBOSE) << reply->DebugString().c_str();;
        return Status::OK;
    }

    private:
        const AndroidConsoleAgents* mAgents;
    };

using Builder = EmulatorControllerService::Builder;

Builder::Builder() : mCredentials{grpc::InsecureServerCredentials()} {}

Builder& Builder::withConsoleAgents(
        const AndroidConsoleAgents* const consoleAgents) {
    mAgents = consoleAgents;
    return *this;
}
Builder& Builder::withCredentials(
        std::shared_ptr<grpc::ServerCredentials> credentials) {
    mCredentials = credentials;
    return *this;
}

Builder& Builder::withPort(int port) {
    mPort = port;
    return *this;
}

std::unique_ptr<EmulatorControllerService> Builder::build() {
    if (mAgents == nullptr) {
        // Excuse me?
        return nullptr;
    }

    std::string server_address = "0.0.0.0:" + std::to_string(mPort);
    std::unique_ptr<EmulatorController::Service> controller(
            new EmulatorControllerImpl(mAgents));

    ServerBuilder builder;
    builder.AddListeningPort(server_address, mCredentials);
    builder.RegisterService(controller.release());
    // enable tls
    // builder.useTransportSecurity(certChainFile, privateKeyFile)

    auto service = builder.BuildAndStart();
    if (!service)
        return nullptr;

    return std::unique_ptr<EmulatorControllerService>(
            new EmulatorControllerServiceImpl(controller.release(),
                                              service.release()));
}

}  // namespace control
}  // namespace emulation
}  // namespace android