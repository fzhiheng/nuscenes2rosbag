#include "nuscenes2bag/utils.hpp"
#include <nuscenes2bag/MetaDataReader.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>

using namespace std;
namespace json = nlohmann;

namespace nuscenes2bag {

    template<typename T>
    void
    throwKeyNotFound(const T &key, const char *msg) {
        std::string errorMsg = "MetaDataError: ";
        errorMsg += msg;
        errorMsg += " [" + key + "]";
        throw InvalidMetaDataException(errorMsg);
    }

    template<template<class, class, class...> class Container,
            class Key,
            class Value,
            class... TArgs>
    const Value &
    findOrThrow(const Container<Key, Value, TArgs...> &container,
                const Key &key,
                const char *msg) {
        auto it = container.find(key);
        if (it == container.end()) {
            throwKeyNotFound<Key>(key, msg);
        }
        return it->second;
    }

    void
    MetaDataReader::loadFromDirectory(const fs::path &directoryPath) {
        const fs::path sceneFile = directoryPath / "scene.json";
        const fs::path sampleFile = directoryPath / "sample.json";
        const fs::path sampleDataFile = directoryPath / "sample_data.json";
        const fs::path egoPoseFile = directoryPath / "ego_pose.json";
        const fs::path calibratedSensorFile =
                directoryPath / "calibrated_sensor.json";
        const fs::path sensorFile = directoryPath / "sensor.json";

        scenes = loadScenesFromFile(sceneFile);
        scene2Samples = loadSampleInfos(sampleFile);
        sample2SampleData = loadSampleDataInfos(sampleDataFile);
        calibratedSensorToken2CalibratedSensorInfo =
                loadCalibratedSensorInfo(calibratedSensorFile);
        sensorToken2CalibratedSensorName = loadCalibratedSensorNames(sensorFile);

        // build inverse (EgoPose.token -> Scene.token) map
        // and (scene.token -> calibratedSensor[]) map
        std::map<Token, Token> egoPoseToken2sceneToken;

        for (const auto &keyvalue: scene2Samples) {
            const Token &sceneToken = keyvalue.first;
            const std::vector<SampleInfo> &sampleInfos = keyvalue.second;

            for (const auto &sampleInfo: sampleInfos) {
                for (const auto &sampleData: sample2SampleData[sampleInfo.token]) {
                    // add egoPoseInfo
                    egoPoseToken2sceneToken.emplace(sampleData.egoPoseToken, sceneToken);

                    // add calibrated sensor info
                    auto &calibratedSensorInfoSet =
                            getExistingOrDefault(scene2CalibratedSensorInfo, sceneToken);
                    const auto &calibratedSensorInfo =
                            findOrThrow(calibratedSensorToken2CalibratedSensorInfo,
                                        sampleData.calibratedSensorToken,
                                        "unable to find calibrated sensor");
                    const auto &calibratedSensorName =
                            findOrThrow(sensorToken2CalibratedSensorName,
                                        calibratedSensorInfo.sensorToken,
                                        "unable to find sensor");
                    calibratedSensorInfoSet.insert(CalibratedSensorInfoAndName{
                            calibratedSensorInfo, calibratedSensorName});
                }
            }
        }

        scene2EgoPose = loadEgoPoseInfos(egoPoseFile, egoPoseToken2sceneToken);

        loadFromDirectoryCalled = true;
    }

    json::json
    MetaDataReader::slurpJsonFile(const fs::path &filePath) {
        std::ifstream file(filePath.string());
        if (!file.is_open()) {
            std::string errMsg = string("Unable to open ") + filePath.string();
            throw std::runtime_error(errMsg);
        }
        json::json newJson;
        file >> newJson;
        return newJson;
    }

    std::vector<SceneInfo>
    MetaDataReader::loadScenesFromFile(const fs::path &filePath) {
        auto sceneJsons = slurpJsonFile(filePath);
        std::vector<SceneInfo> sceneInfos;

        std::regex sceneIdRegex("scene-(\\d+)");

        for (const auto &sceneJson: sceneJsons) {
            std::string sceneIdStr = sceneJson["name"];
            std::smatch match;
            std::regex_search(sceneIdStr, match, sceneIdRegex);
            SceneId sceneId = std::stoi(match.str(1));
            sceneInfos.push_back(SceneInfo{
                    sceneJson["token"],
                    sceneJson["nbr_samples"],
                    sceneId,
                    sceneJson["name"],
                    sceneJson["description"],
                    sceneJson["first_sample_token"],
            });
        }

        return sceneInfos;
    }

    std::map<Token, std::vector<SampleInfo>>
    MetaDataReader::loadSampleInfos(const fs::path &filePath) {
        auto sampleInfos = slurpJsonFile(filePath);
        std::map<Token, std::vector<SampleInfo>> token2Samples;

        for (const auto &sampleInfo: sampleInfos) {
            Token sampleToken = sampleInfo["token"];
            Token sceneToken = sampleInfo["scene_token"];
            std::vector<SampleInfo> &samples =
                    getExistingOrDefault(token2Samples, sceneToken);
            samples.push_back(
                    SampleInfo{sceneToken, sampleToken, sampleInfo["timestamp"]});
        }

        return token2Samples;
    }

    std::map<Token, std::vector<SampleDataInfo>>
    MetaDataReader::loadSampleDataInfos(const fs::path &filePath) {
        auto sampleDataJsons = slurpJsonFile(filePath);
        std::map<Token, std::vector<SampleDataInfo>> sample2SampleData;

        for (const auto &sampleDataJson: sampleDataJsons) {
            Token sampleToken = sampleDataJson["sample_token"];
            Token sampleDataToken = sampleDataJson["token"];
            std::vector<SampleDataInfo> &sampleDatas =
                    getExistingOrDefault(sample2SampleData, sampleToken);
            sampleDatas.push_back(SampleDataInfo{
                    sampleDataToken,
                    sampleDataJson["timestamp"],
                    sampleDataJson["ego_pose_token"],
                    sampleDataJson["calibrated_sensor_token"],
                    sampleDataJson["fileformat"],
                    sampleDataJson["is_key_frame"],
                    sampleDataJson["filename"],
            });
        }

        return sample2SampleData;
    }

    std::vector<ImuData>
    MetaDataReader::loadImuData(const fs::path &filePath) {
        std::cout << "Loading IMU data from json file" << std::endl;
        auto imuDataJsons = slurpJsonFile(filePath);
        std::vector<ImuData> imuDatas;

        std::cout << "Unerializing IMU data" << std::endl;
        for (const auto &imuDataJson: imuDataJsons) {
            auto linear_accel = imuDataJson["linear_accel"];
            auto q = imuDataJson["q"];
            auto rotation_rate = imuDataJson["rotation_rate"];
            imuDatas.push_back(ImuData{
                    {linear_accel[0], linear_accel[1], linear_accel[2]},
                    {q[0], q[1], q[2], q[3]},
                    {rotation_rate[0], rotation_rate[1], rotation_rate[2]},
                    imuDataJson["utime"]
            });
        }

        return imuDatas;
    }

    std::vector<WheelData>
    MetaDataReader::loadWheelData(const fs::path &filePath) {
        std::cout << "Loading WHEEL data from json file" << std::endl;
        auto wheelDataJsons = slurpJsonFile(filePath);
        std::vector<WheelData> wheelDatas;

        std::cout << "Unerializing WHEEL data" << std::endl;
        for (const auto &wheelDataJson: wheelDataJsons) {
            auto FL_wheel_speed = wheelDataJson["FL_wheel_speed"];
            auto FR_wheel_speed = wheelDataJson["FR_wheel_speed"];
            auto RL_wheel_speed = wheelDataJson["RL_wheel_speed"];
            auto RR_wheel_speed = wheelDataJson["RR_wheel_speed"];
            auto left_solar = wheelDataJson["left_solar"];
            auto longitudinal_accel = wheelDataJson["longitudinal_accel"];
            auto meanEffTorque = wheelDataJson["meanEffTorque"];
            auto odom = wheelDataJson["odom"];
            auto odom_speed = wheelDataJson["odom_speed"];
            auto pedal_cc = wheelDataJson["pedal_cc"];
            auto regen = wheelDataJson["regen"];
            auto requestedTorqueAfterProc = wheelDataJson["requestedTorqueAfterProc"];
            auto right_solar = wheelDataJson["right_solar"];
            auto steer_corrected = wheelDataJson["steer_corrected"];
            auto steer_offset_can = wheelDataJson["steer_offset_can"];
            auto steer_raw = wheelDataJson["steer_raw"];
            auto transversal_accel = wheelDataJson["transversal_accel"];
            auto utime = wheelDataJson["utime"];

            wheelDatas.push_back(WheelData{FL_wheel_speed, FR_wheel_speed, RL_wheel_speed, RR_wheel_speed, left_solar,
                                           longitudinal_accel, meanEffTorque, odom, odom_speed, pedal_cc, regen,
                                           requestedTorqueAfterProc, right_solar, steer_corrected, steer_offset_can,
                                           steer_raw, transversal_accel,
                                           utime});
        }

        return wheelDatas;
    }


    EgoPoseInfo
    egoPoseJson2EgoPoseInfo(const json::json &egoPoseJson) {
        EgoPoseInfo egoPoseInfo;

        egoPoseInfo.translation[0] = egoPoseJson["translation"][0];
        egoPoseInfo.translation[1] = egoPoseJson["translation"][1];
        egoPoseInfo.translation[2] = egoPoseJson["translation"][2];

        egoPoseInfo.rotation[0] = egoPoseJson["rotation"][0];
        egoPoseInfo.rotation[1] = egoPoseJson["rotation"][1];
        egoPoseInfo.rotation[2] = egoPoseJson["rotation"][2];
        egoPoseInfo.rotation[3] = egoPoseJson["rotation"][3];

        egoPoseInfo.timeStamp = egoPoseJson["timestamp"];

        return egoPoseInfo;
    }

    std::map<Token, std::vector<EgoPoseInfo>>
    MetaDataReader::loadEgoPoseInfos(
            const fs::path &filePath,
            std::map<Token, Token> sampleDataToken2SceneToken) {

        auto egoPoseJsons = slurpJsonFile(filePath);
        std::map<Token, std::vector<EgoPoseInfo>> sceneToken2EgoPoseInfos;

        for (const auto &egoPoseJson: egoPoseJsons) {
            Token sampleDataToken = egoPoseJson["token"];
            const auto &sceneToken = findOrThrow(sampleDataToken2SceneToken,
                                                 sampleDataToken,
                                                 " Unable to find sample token");
            std::vector<EgoPoseInfo> &egoPoses =
                    getExistingOrDefault(sceneToken2EgoPoseInfos, sceneToken);

            EgoPoseInfo egoPoseInfo = egoPoseJson2EgoPoseInfo(egoPoseJson);
            egoPoses.push_back(egoPoseInfo);
        }

        return sceneToken2EgoPoseInfos;
    }

    std::map<Token, CalibratedSensorInfo>
    MetaDataReader::loadCalibratedSensorInfo(const fs::path &filePath) {
        auto calibratedSensorJsons = slurpJsonFile(filePath);
        std::map<Token, CalibratedSensorInfo>
                calibratedSensorToken2CalibratedSensorInfo;

        for (const auto &calibratedSensorJson: calibratedSensorJsons) {
            Token token = calibratedSensorJson["token"];
            auto translation = calibratedSensorJson["translation"];
            auto rotation = calibratedSensorJson["rotation"];
            CalibratedSensorInfo calibratedSensorInfo{
                    token,
                    calibratedSensorJson["sensor_token"],
                    {translation[0], translation[1], translation[2]},
                    {rotation[0], rotation[1], rotation[2], rotation[3]},
                    boost::none
            };

            boost::optional<json::json> sensor_intrinsics =
                    calibratedSensorJson["rotation"];

            calibratedSensorToken2CalibratedSensorInfo.emplace(token,
                                                               calibratedSensorInfo);
        }

        return calibratedSensorToken2CalibratedSensorInfo;
    }

    std::map<Token, CalibratedSensorName>
    MetaDataReader::loadCalibratedSensorNames(const fs::path &filePath) {
        auto calibratedSensorNameJsons = slurpJsonFile(filePath);
        std::map<Token, CalibratedSensorName> sensorToken2CalibratedSensorName;

        for (const auto &calibratedSensorNameJson: calibratedSensorNameJsons) {
            sensorToken2CalibratedSensorName.emplace(
                    calibratedSensorNameJson["token"],
                    CalibratedSensorName{calibratedSensorNameJson["token"],
                                         calibratedSensorNameJson["channel"],
                                         calibratedSensorNameJson["modality"]});
        };

        return sensorToken2CalibratedSensorName;
    }

    std::vector<Token>
    MetaDataReader::getAllSceneTokens() const {
        assert(loadFromDirectoryCalled);
        std::vector<Token> tokens;
        std::transform(scenes.begin(),
                       scenes.end(),
                       std::back_inserter(tokens),
                       [](const SceneInfo &sceneInfo) { return sceneInfo.token; });
        return tokens;
    }

    boost::optional<SceneInfo>
    MetaDataReader::getSceneInfo(const Token &sceneToken) const {
        assert(loadFromDirectoryCalled);
        auto it = std::find_if(
                scenes.begin(), scenes.end(), [&sceneToken](const SceneInfo &sceneInfo) {
                    return sceneInfo.token == sceneToken;
                });
        if (it == scenes.end()) {

            return boost::none;
        }

        return boost::optional<SceneInfo>(*it);
    }

    std::vector<SampleDataInfo>
    MetaDataReader::getSceneSampleData(const Token &sceneToken) const {
        std::vector<SampleDataInfo> sampleDataInfos;

        const auto &sceneSamples =
                findOrThrow(scene2Samples, sceneToken, " sample for scene token");
        for (const auto &sceneSample: sceneSamples) {
            const Token &sceneSampleToken = sceneSample.token;
            const auto &sceneSampleDatas = findOrThrow(
                    sample2SampleData, sceneSampleToken, " sample data for sample token");

            for (const SampleDataInfo &sampleData: sceneSampleDatas) {
                sampleDataInfos.push_back(sampleData);
            }
        }

        return sampleDataInfos;
    }

    std::vector<ImuData>
    MetaDataReader::getImuData(const fs::path &inPath) const {
        return loadImuData(inPath);
    }

    std::vector<WheelData>
    MetaDataReader::getWheelData(const fs::path &inPath)const {
        return loadWheelData(inPath);
    }

    std::vector<EgoPoseInfo>
    MetaDataReader::getEgoPoseInfo(const Token &sceneToken) const {
        return findOrThrow(scene2EgoPose, sceneToken, "ego pose by scene token");
    }

    CalibratedSensorInfo
    MetaDataReader::getCalibratedSensorInfo(
            const Token &calibratedSensorToken) const {
        return findOrThrow(calibratedSensorToken2CalibratedSensorInfo,
                           calibratedSensorToken,
                           "calibrated sensor info by sensor token");
    }

    CalibratedSensorName
    MetaDataReader::getSensorName(const Token &sensorToken) const {
        return findOrThrow(sensorToken2CalibratedSensorName,
                           sensorToken,
                           "sensor name by sensor token");
    }

    std::vector<CalibratedSensorInfoAndName>
    MetaDataReader::getSceneCalibratedSensorInfo(const Token &sceneToken) const {
        std::vector<CalibratedSensorInfoAndName> sceneCalibratedSensorInfo;
        const auto &sceneCalibratedSensorInfoSet =
                findOrThrow(scene2CalibratedSensorInfo,
                            sceneToken,
                            "calibrated sensor info by scene token");
        std::copy(sceneCalibratedSensorInfoSet.begin(),
                  sceneCalibratedSensorInfoSet.end(),
                  std::back_inserter(sceneCalibratedSensorInfo));
        return sceneCalibratedSensorInfo;
    }

    boost::optional<SceneInfo>
    MetaDataReader::getSceneInfoByNumber(const uint32_t sceneNumber) const {
        boost::optional<SceneInfo> sceneInfoOpt;
        for (const auto &scene: scenes) {
            if (scene.sceneId == sceneNumber) {
                sceneInfoOpt = scene;
            }
        }
        return sceneInfoOpt;
    }

}