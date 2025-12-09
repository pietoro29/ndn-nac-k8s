#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/security/validator-config.hpp>
#include <ndn-cxx/util/io.hpp>
#include <ndn-nac/decryptor.hpp>
#include <ndn-cxx/security/pib/identity.hpp>
#include "nac-utils.hpp"

#include <iostream>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

namespace ndn::nac::examples {

class Consumer
{
public:
    Consumer()
        : m_face(nullptr, m_keyChain)
        , m_validator(m_face)
    {
        const char* prefixEnv = std::getenv("NDN_DATA_PREFIX");
        if (!prefixEnv) {
            throw std::runtime_error("NDN_DATA_PREFIX environment variable not set");
        }
        m_dataPrefix = Name(prefixEnv);

        m_validator.load(R"CONF(trust-anchor { type any })CONF", "fake-config");
    }

    void run()
    {
        //KDKのロード
        std::string kdkPath = findKeyFile("/data/nac-data", "kdk_");
        if (!kdkPath.empty()) {
            std::cout << "[Consumer] Found local KDK cache: " << kdkPath << std::endl;
            auto kdkData = ndn::io::load<Data>(kdkPath);
            if (kdkData) {
                initializeDecryptor(kdkData);
                sendContentInterest();
                m_face.processEvents();
                return;
            }
        }

        //ローカルになければネットワークへ取りに行く
        std::cout << "[Consumer] No local KDK. Fetching from network..." << std::endl;
        fetchKdk();

        m_face.processEvents();
    }

private:
  // KDKをネットワークから取得する
    void fetchKdk() {
        // KDKの命名規則は /<ContentPrefix>/NAC/KDK/<KEK-ID>/...
        // KEK-IDが不明なため、/<ContentPrefix>/NAC/KDK までを指定して CanBePrefix で投げる
        Name kdkQuery = m_dataPrefix;
        kdkQuery.append("NAC").append("KDK");

        Interest interest(kdkQuery);
        interest.setCanBePrefix(true); // 具体的なIDが不明なためPrefixで検索
        interest.setMustBeFresh(true);

        std::cout << "=== Fetching KDK: " << interest.getName() << " ===" << std::endl;

        m_face.expressInterest(interest,
              std::bind(&Consumer::onKdkData, this, std::placeholders::_1, std::placeholders::_2),
            std::bind(&Consumer::onNack, this, std::placeholders::_1, std::placeholders::_2),
            std::bind(&Consumer::onTimeout, this, std::placeholders::_1)
        );
    }

  // KDK受信時のコールバック
    void onKdkData(const Interest&, const Data& data) {
        std::cout << "Received KDK Data: " << data.getName() << std::endl;
        auto kdkData = std::make_shared<Data>(data);

        try {
              initializeDecryptor(kdkData);
            //本来のコンテンツを取りに行く
            sendContentInterest();
        } catch (const std::exception& e) {
            std::cerr << "Failed to initialize Decryptor with fetched KDK: " << e.what() << std::endl;
            exit(1);
        }
    }

  // Decryptorの初期化
    void initializeDecryptor(std::shared_ptr<Data> kdkData) {
        m_kdkData = kdkData;

        ndn::security::pib::Identity myIdentity;
        try {
            myIdentity = m_keyChain.getPib().getDefaultIdentity();
        } catch (const std::exception& e) {
            std::cerr << "FATAL: No default identity found. " << e.what() << std::endl;
            exit(1);
        }

        std::cout << "Initializing Decryptor with Identity: " << myIdentity.getName() << std::endl;

        // Decryptor作成
        m_decryptor = std::make_unique<Decryptor>(
            myIdentity.getDefaultKey(), // KDKは自分のKeyで暗号化されている
            m_validator,
            m_keyChain,
            m_face
        );

        // KDK配信登録
        Name kdkName = m_kdkData->getName();
        // 名前の中に "ENCRYPTED-BY" が含まれているか確認して登録範囲を決める
        // 簡易的にData名のPrefix(-1)で登録
        m_kdkHandle = m_face.setInterestFilter(
            kdkName.getPrefix(-1),
            [this](const InterestFilter&, const Interest& interest) {
                if (interest.matchesData(*m_kdkData)) {
                    // std::cout << "Serving KDK to self" << std::endl;
                    m_face.put(*m_kdkData);
                }
            }
        );
    }


    // コンテンツ取得リクエスト
    void sendContentInterest() {
        Interest interest(m_dataPrefix);
        interest.setCanBePrefix(true);
        interest.setMustBeFresh(true);
        std::cout << "=== Consumer Sending Interest: " << interest << " ===" << std::endl;

        m_face.expressInterest(interest,
            std::bind(&Consumer::onData, this, std::placeholders::_1, std::placeholders::_2),
            std::bind(&Consumer::onNack, this, std::placeholders::_1, std::placeholders::_2),
            std::bind(&Consumer::onTimeout, this, std::placeholders::_1)
        );
    }

    void onData(const Interest&, const Data& data) {
        std::cout << "Received Content Data. Decrypting..." << std::endl;

        if (!m_decryptor) return;

        Block contentBlock = data.getContent();
        contentBlock.parse();

        try{
            Block encryptedContent = contentBlock.blockFromValue();
            m_decryptor->decrypt(encryptedContent,
                [](const ConstBufferPtr& c) {
                    std::cout << "\n*** SUCCESS! Decrypted: "
                              << std::string(reinterpret_cast<const char*>(c->data()), c->size())
                              << " ***\n" << std::endl;
                    exit(0);
                },
                [](const ErrorCode& c, const std::string& e) {
                    std::cerr << "Decryption Failed [" << static_cast<int>(c) << "]: " << e << std::endl;
                    exit(1);
                }
            );
        } catch (const std::exception& e) {
            std::cerr << "Parse Error: " << e.what() << std::endl;
        }
    }

    void onNack(const Interest& interest, const lp::Nack& nack) const {
        std::cerr << "Nack for " << interest.getName() << ": " << nack.getReason() << std::endl;
        exit(1);
    }

    void onTimeout(const Interest& interest) const {
        std::cerr << "Timeout for " << interest.getName() << std::endl;
        exit(1);
    }

    KeyChain m_keyChain;
    Face m_face;
    ValidatorConfig m_validator;
    std::unique_ptr<Decryptor> m_decryptor;
    std::shared_ptr<Data> m_kdkData;
    ScopedInterestFilterHandle m_kdkHandle;
    Name m_dataPrefix;
};

} // namespace

int main() {
    try {
        ndn::nac::examples::Consumer c;
        c.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}
