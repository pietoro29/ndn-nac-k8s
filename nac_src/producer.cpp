#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/security/validator-config.hpp>
#include <ndn-cxx/util/io.hpp>
#include <ndn-nac/encryptor.hpp>

#include "nac-utils.hpp"

#include <iostream>
#include <filesystem>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

namespace ndn::nac::examples {

class Producer
{
public:
  Producer()
    : m_face(nullptr, m_keyChain)
    , m_validator(m_face)
  {
    const char* prefixEnv = std::getenv("NDN_DATA_PREFIX");
    if (!prefixEnv){
        throw std::runtime_error("NDN_DATA_PREFIX environment variable not set");
    }
    m_dataPrefix = Name(prefixEnv);

    // バリデータ (テスト用: 全許可)
    m_validator.load(R"CONF(trust-anchor { type any })CONF", "fake-config");

    //KEKのロード
    std::string kekPath = findKeyFile("/data/nac-data", "kek_");
    if (kekPath.empty()) {
        throw std::runtime_error("No KEK file (kek_*.data) found in /data/nac-data/");
    }

    m_kekData = ndn::io::load<Data>(kekPath);
    if (!m_kekData) throw std::runtime_error("Failed to parse KEK data");

    std::cout << "Loaded KEK: " << m_kekData->getName() << std::endl;

    //KEK配信の登録
    m_kekHandle = m_face.setInterestFilter(
        m_kekData->getName().getPrefix(-1), // /ndn/AM/.../KEK で待ち受け
        [this](const InterestFilter&, const Interest& interest) {
            if (interest.matchesData(*m_kekData)) {
                    m_face.put(*m_kekData);
            }
        }
    );

    //Encryptor 初期化
    // KEKの名前からAccessPrefixを導出
    Name accessPrefix = m_kekData->getName().getPrefix(-2);
    auto myIdentity = m_keyChain.getPib().getDefaultIdentity();
    m_encryptor = std::make_unique<Encryptor>(
        accessPrefix,
        accessPrefix, //CK Prefix
        ndn::security::SigningInfo(myIdentity),
        [](const ErrorCode& code, const std::string& msg) {
            std::cerr << "NAC Encryption Error: " << msg << std::endl;
        },
        m_validator,
        m_keyChain,
        m_face
    );
  }

  void run()
  {
    std::cout << "=== Producer Ready  for" << m_dataPrefix << " ===" << std::endl;

    // データ要求に対するフィルタ登録
    m_face.setInterestFilter(
        InterestFilter(m_dataPrefix),
        std::bind(&Producer::onContentInterest, this, std::placeholders::_2),
        [](const Name& prefix) { std::cout << "Registered prefix: " << prefix << std::endl; },
        [](const Name&, const std::string& msg) { std::cerr << "Register failed: " << msg << std::endl; }
    );
    m_face.processEvents();
  }

private:
  void onContentInterest(const Interest& interest) {
    std::cout << "<< Interest: " << interest.getName() << std::endl;
    std::string content = "Secure Video Data at " + std::to_string(std::time(nullptr));

    try{
        //contentを暗号化
        auto encrypted = m_encryptor->encrypt({reinterpret_cast<const uint8_t*>(content.data()), content.size()});
        //Dataパケットを作る
        auto data = std::make_shared<Data>(interest.getName());
        data->setFreshnessPeriod(1_s);
        data->setContent(encrypted.wireEncode());
        //署名
        m_keyChain.sign(*data);
        m_face.put(*data);
        std::cout << ">> Sent Encrypted Data (" << content.size() << " bytes)" << std::endl;
    } catch (const std::exception& e){
        std::cerr << "Encryption/Signing Error: " << e.what() << std::endl;
    }
  }

  KeyChain m_keyChain;
  Face m_face;
  ValidatorConfig m_validator;
  std::shared_ptr<Data> m_kekData;
  ScopedInterestFilterHandle m_kekHandle;
  std::unique_ptr<Encryptor> m_encryptor;
  Name m_dataPrefix;
};

} // namespace

int main() {
  try {
    ndn::nac::examples::Producer producer;
    producer.run();
  } catch (const std::exception& e) {
    std::cerr << "Fatal: " << e.what() << std::endl;
    return 1;
  }
}
