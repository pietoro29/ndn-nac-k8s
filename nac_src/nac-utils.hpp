#ifndef NAC_UTILS_HPP
#define NAC_UTILS_HPP
#include <ndn-cxx/util/io.hpp>
#include <ndn-cxx/face.hpp>
#include <filesystem>
#include <vector>
#include <string>
#include <iostream>

namespace fs = std::filesystem;

namespace ndn::nac::examples {

// 指定ディレクトリからkek_ or kdk_で始まるDataファイルを探す
std::string findKeyFile(const std::string& directory, const std::string& filePrefix) {
    for (const auto& entry : fs::directory_iterator(directory)) {
        std::string filename = entry.path().filename().string();
        if (filename.rfind(filePrefix, 0) == 0 && filename.substr(filename.length() - 5) == ".data") {
            return entry.path().string();
        }
    }
    return "";
}

// ローカルファイルからロードしたDataをFace経由で取得できるように配信
void serveLocalData(Face& face, std::shared_ptr<Data> data) {
    if (!data) return;

    face.setInterestFilter(
        data->getName().getPrefix(-1),
        [data](const InterestFilter&, const Interest& interest){
            if (interest.matchesData(*data)) {

            }
        },
        nullptr
    );
}
} // namespace ndn::nac::examples
#endif
