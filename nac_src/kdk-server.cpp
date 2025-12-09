#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/util/io.hpp>
#include <iostream>
#include <filesystem>
#include <map>

namespace fs = std::filesystem;

namespace ndn::nac::server {

class KdkServer
{
public:
  KdkServer()
    : m_face(nullptr, m_keyChain)
  {
    loadDataFiles("/data/nac-data");
  }

  void run()
  {
    if (m_store.empty()) {
      std::cerr << "WARN: No data loaded. Server is idle." << std::endl;
    } else {
      std::cout << "KDK Server running. Serving " << m_store.size() << " data packets." << std::endl;
    }
    m_face.processEvents();
  }

private:
  void loadDataFiles(const std::string& pathStr)
  {
    fs::path dir(pathStr);
    if (!fs::exists(dir) || !fs::is_directory(dir)) {
      std::cerr << "Error: Directory not found: " << pathStr << std::endl;
      return;
    }

    for (const auto& entry : fs::directory_iterator(dir)) {
      if (entry.path().extension() == ".data") {
        try {
          auto data = ndn::io::load<Data>(entry.path().string());
          if (data) {
            storeData(data);
          }
        }
        catch (const std::exception& e) {
          std::cerr << "Failed to load " << entry.path() << ": " << e.what() << std::endl;
        }
      }
    }
  }

  void storeData(std::shared_ptr<Data> data)
  {
    Name name = data->getName();
    m_store[name] = data;

    std::cout << "Loaded: " << name << std::endl;

    Name prefixToServe = name;
    //名前から"KDK"を探してそこまでの長さで切る
    for (ssize_t i = 0; i < static_cast<ssize_t>(name.size()); ++i) {
      if (name[i].toUri() == "KDK") {
        prefixToServe = name.getPrefix(i + 1); // .../NAC/KDK までを含む
        break;
      }
    }

    std::cout << "[KDK Server] Registering filter: " << prefixToServe << std::endl;

    m_face.setInterestFilter(prefixToServe,
      [this, data](const InterestFilter&, const Interest& interest) {
        // InterestがData名の一部(Prefix)であるか、あるいはDataがInterestにマッチするか
        if (data->getName().isPrefixOf(interest.getName()) || interest.matchesData(*data)){
          std::cout << "[KDK Server] Serving: " << data->getName() << std::endl;
          m_face.put(*data);
        }
      },
      [](const Name& prefix, const std::string& msg) {
        std::cerr << "Register failed for " << prefix << ": " << msg << std::endl;
      }
    );
  }

private:
  KeyChain m_keyChain;
  Face m_face;
  std::map<Name, std::shared_ptr<Data>> m_store;
};

} // namespace

int main(int argc, char** argv)
{
  try {
    ndn::nac::server::KdkServer server;
    server.run();
  }
  catch (const std::exception& e) {
    std::cerr << "FATAL: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
