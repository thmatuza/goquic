#include "adaptor.h"

#include "go_quic_dispatcher.h"
#include "go_quic_connection_helper.h"
#include "go_quic_server_packet_writer.h"

#include "net/quic/quic_connection.h"
#include "net/quic/quic_clock.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/crypto/quic_random.h"
#include "net/base/net_util.h"
#include "net/base/ip_endpoint.h"

#include "base/command_line.h"
#include "base/at_exit.h"
#include "base/logging.h"

#include <iostream>
#include <vector>

#define EXPECT_TRUE(x) { if (!(x)) printf("ERROR"); }

using namespace net;
using namespace std;



class GoQuicPacketWriterFactory : public QuicConnection::PacketWriterFactory {
 public:
  explicit GoQuicPacketWriterFactory(QuicPacketWriter* writer) {
//    ON_CALL(*this, Create(_)).WillByDefault(Return(writer));
    writer_ = writer;
  }
  virtual ~GoQuicPacketWriterFactory() override {}

  virtual QuicPacketWriter *Create(QuicConnection* connection) const override {
    return writer_;
  }

 private:
  QuicPacketWriter *writer_;
};


static base::AtExitManager *exit_manager;
void initialize() {
  int argc = 1;
  char *argv[] = {"test", nullptr};
  base::CommandLine::Init(argc, argv);
  exit_manager = new base::AtExitManager;
}

void set_log_level(int level) {
  logging::SetMinLogLevel(level);
}

/*
void *create_quic_connection(int connection_id, IPEndPoint *ip_endpoint) {
  std::cout << "Hello world!" << std::endl;

  //TestConnectionHelper helper_ = new TestConnectionHelper(&clock_, &random_generator_);
  //GoQuicPacketWriter writer_ = new GoQuicPacketWriter(version(), &clock_);
  //NiceQuic<QuicPacketWriterFactory> factory_;

  //QuicConnection* conn = new QuicConnection(connection_id, IPEndPoint(), helper_.get(), factory_, true, true, true, version());

  //
  QuicClock clock_;
  QuicRandom* random_generator_ = QuicRandom::GetInstance();

  TestConnectionHelper *helper = new TestConnectionHelper(&clock_, random_generator_);
  GoQuicPacketWriter *writer = new GoQuicPacketWriter(&clock_);
  QuicVersionVector supported_versions;
  for (size_t i = 0; i < arraysize(kSupportedQuicVersions); ++i) {
    supported_versions.push_back(kSupportedQuicVersions[i]);
  }
  GoQuicPacketWriterFactory factory(writer);

  QuicConnection* conn = new QuicConnection(connection_id, *ip_endpoint, helper, factory, true, true, true, supported_versions); 
  return conn;
}

int quic_connection_version(QuicConnection *conn) {
  return conn->version();
}

void quic_connection_process_udp_packet(QuicConnection *conn, IPEndPoint *self_address, IPEndPoint *peer_address, QuicEncryptedPacket *packet) {
  conn->ProcessUdpPacket(*self_address, *peer_address, *packet);
}
*/

GoQuicDispatcher *create_quic_dispatcher(void *go_udp_conn) {
  QuicConfig* config = new QuicConfig();
  QuicCryptoServerConfig* crypto_config = new QuicCryptoServerConfig("secret", QuicRandom::GetInstance());
  QuicClock* clock = new QuicClock();
  QuicRandom* random_generator = QuicRandom::GetInstance();

  TestConnectionHelper *helper = new TestConnectionHelper(clock, random_generator);
  QuicVersionVector *versions = new QuicVersionVector(net::QuicSupportedVersions());

  /* Initialize Configs ------------------------------------------------*/

  // If an initial flow control window has not explicitly been set, then use a
  // sensible value for a server: 1 MB for session, 64 KB for each stream.
  const uint32 kInitialSessionFlowControlWindow = 1 * 1024 * 1024;  // 1 MB
  const uint32 kInitialStreamFlowControlWindow = 64 * 1024;         // 64 KB
  if (config->GetInitialStreamFlowControlWindowToSend() ==
      kMinimumFlowControlSendWindow) {
    config->SetInitialStreamFlowControlWindowToSend(
        kInitialStreamFlowControlWindow);
  }
  if (config->GetInitialSessionFlowControlWindowToSend() ==
      kMinimumFlowControlSendWindow) {
    config->SetInitialSessionFlowControlWindowToSend(
        kInitialSessionFlowControlWindow);
  }

  scoped_ptr<CryptoHandshakeMessage> scfg(
      crypto_config->AddDefaultConfig(
          helper->GetRandomGenerator(), helper->GetClock(),
          QuicCryptoServerConfig::ConfigOptions()));
  /* Initialize Configs Ends ----------------------------------------*/

  GoQuicDispatcher* dispatcher = new GoQuicDispatcher(*config,
      *crypto_config,
      *versions,
      new GoQuicDispatcher::DefaultPacketWriterFactory(),
      helper);

  GoQuicServerPacketWriter* writer = new GoQuicServerPacketWriter(go_udp_conn, dispatcher);

  dispatcher->Initialize(writer);

  return dispatcher;
}

void quic_dispatcher_process_packet(GoQuicDispatcher *dispatcher, IPEndPoint *self_address, IPEndPoint *peer_address, QuicEncryptedPacket *packet) {
  dispatcher->ProcessPacket(*self_address, *peer_address, *packet);
}

QuicEncryptedPacket *create_quic_encrypted_packet(char *buffer, size_t length) {
  return new QuicEncryptedPacket(buffer, length, false /* Do not own the buffer, so will not free buffer in the destructor */);
}

void delete_quic_encrypted_packet(QuicEncryptedPacket *packet) {
  delete packet;
}

IPAddressNumber *create_ip_address_number(unsigned char *ip_buf, size_t length) {
  IPAddressNumber *ip = new IPAddressNumber;
  for (int i = 0; i < length; i++) {
    ip->push_back(ip_buf[i]);
  }
  return ip;
}

void delete_ip_address_number(IPAddressNumber *ip) {
  delete ip;
}

IPEndPoint *create_ip_end_point(IPAddressNumber *ip, uint16_t port) {
  return new IPEndPoint(*ip, port);
}

size_t ip_endpoint_ip_address(IPEndPoint *ip_end_point, void *address_buf) {
  const IPAddressNumber &ip = ip_end_point->address();
  const size_t size = ip.size();
  for (int i = 0; i < size; i++) {
    // XXX(serialx): Performance and safety issues here. Ensure address_buf is large enough for ipv6
    ((char *)address_buf)[i] = ip[i];
  }
  return size;
}

uint16_t ip_endpoint_port(IPEndPoint *ip_end_point) {
  return ip_end_point->port();
}

void delete_ip_end_point(IPEndPoint *ip_end_point) {
  delete ip_end_point;
}

/*
void test_quic() {
  std::cout << "Hello world!" << std::endl;

  //TestConnectionHelper helper_ = new TestConnectionHelper(&clock_, &random_generator_);
  //GoQuicPacketWriter writer_ = new GoQuicPacketWriter(version(), &clock_);
  //NiceQuic<QuicPacketWriterFactory> factory_;

  //QuicConnection* conn = new QuicConnection(connection_id, IPEndPoint(), helper_.get(), factory_, true, true, true, version());

  //
  QuicConnectionId connection_id = 42;
  QuicClock clock_;
  QuicRandom* random_generator_ = QuicRandom::GetInstance();

  TestConnectionHelper *helper = new TestConnectionHelper(&clock_, random_generator_);
  GoQuicPacketWriter *writer = new GoQuicPacketWriter(&clock_);
  QuicVersionVector supported_versions;
  for (size_t i = 0; i < arraysize(kSupportedQuicVersions); ++i) {
    supported_versions.push_back(kSupportedQuicVersions[i]);
  }
  GoQuicPacketWriterFactory factory(writer);

  QuicConnection* conn = new QuicConnection(connection_id, IPEndPoint(), helper, factory, true, true, true, supported_versions); 

  QuicPublicResetPacket header;
  QuicFramer framer_(QuicSupportedVersions(), QuicTime::Zero(), true);
  header.public_header.connection_id = 42;
  header.public_header.reset_flag = true;
  header.public_header.version_flag = false;
  header.rejected_sequence_number = 10101;
  scoped_ptr<QuicEncryptedPacket> packet(
      framer_.BuildPublicResetPacket(header));
  conn->ProcessUdpPacket(IPEndPoint(), IPEndPoint(), *packet);

  std::cout << conn << std::endl;
}
*/
