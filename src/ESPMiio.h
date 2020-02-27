#ifndef ESPMiio_h
#define ESPMiio_h

#include <Ticker.h>
#include <ArduinoJson.h>
#include "IPAddress.h"
#include "ESPAsyncUDP.h"
#include <string>

#define PORT 54321

#define UNSIGNED_FFFFFFFF 0xFFFFFFFFu

#define HEADER 0x2131
#define HELLO_UNKNOWN UNSIGNED_FFFFFFFF
#define HELLO_DEVICE_ID UNSIGNED_FFFFFFFF
#define HELLO_TIME_STAMP UNSIGNED_FFFFFFFF
#define NORMAL_UNKNOWN 0

enum MiioError{
  MIIO_NOT_CONNECTED,
  MIIO_TIMEOUT,
  MIIO_BUSY,
  MIIO_INVALID_RESPONSE,
  MIIO_MESSAGE_CREATING,
};

class MiioToken{
  private:
    char token[16];
    char token_md5[16];
    char token_iv[16];
  public:
    MiioToken(std::string token);
    MiioToken(): MiioToken(""){};

    char* getToken(){ return token; };
    
    size_t encrypt(const char* msg, size_t msg_size, char* encoded_msg);
    size_t decrypt(const char* msg, size_t msg_size, char* decoded_msg);
};

class MiioMessage{
  private:
    uint32_t unknownHeader;
    uint32_t deviceID;
    uint32_t timeStamp;
    bool valid;
 protected:
    uint16_t payloadID;
 public:
    MiioMessage(uint32_t unknownHeader, uint32_t deviceID, uint32_t timeStamp, uint16_t payloadID);
    MiioMessage(char* message, size_t message_len, MiioToken* token);
    virtual ~MiioMessage();

    char* create(std::string payload, MiioToken* token, size_t &size);

    bool isHello(){ return unknownHeader == HELLO_UNKNOWN; };
    uint16_t getPayloadID(){ return payloadID; };
    uint32_t getDeviceID(){ return deviceID; };
    uint32_t getTimeStamp(){ return timeStamp; };
    bool isValid(){ return valid; };
    static bool testMessage(char* message, size_t message_len, MiioToken* token);
};

class MiioCommand: public MiioMessage{
  private:
    std::string method;
    //params
  public:
    MiioCommand();
    MiioCommand(uint32_t deviceID, uint32_t timeStamp, uint16_t payloadID, std::string method/*params*/);
    virtual ~MiioCommand();

    char* create(MiioToken* token, size_t &size);
};

class MiioResponse: public MiioMessage{
  private:
    char* decrypted_payload;
    JsonObject result;
    JsonObject error;
  public:
    MiioResponse(char* message, size_t message_len, MiioToken* token);
    virtual ~MiioResponse();

    bool isValid();
    JsonObject getResult(){ return result; };
    JsonObject getError(){ return error; };
};

typedef std::function<void(MiioResponse& reponse)> MiioResponseHandlerFunction;
typedef std::function<void(MiioError e)> MiioErrorHandlerFunction;

class MiioDevice {
  private:
    AsyncUDP socket;

    IPAddress* ip;
    MiioToken* token;

    uint16_t timeout;

    int32_t deviceID;
    int32_t timeStamp;
    uint16_t payloadID;

    Ticker ticker;

    bool udp_send(char* message, size_t size, IPAddress *ip, AuPacketHandlerFunction cb, MiioErrorHandlerFunction error = NULL);
    void udp_reset();
    void udp_timeout(MiioErrorHandlerFunction cb);
    
    void udp_rcv(AsyncUDPPacket packet, AuPacketHandlerFunction cb);
    void hello_rcv(AsyncUDPPacket packet, MiioErrorHandlerFunction error);
    void send_rcv(AsyncUDPPacket packet, MiioResponseHandlerFunction callback, MiioErrorHandlerFunction error);
  public:
    MiioDevice(IPAddress* ip, std::string token, uint16_t timeout = 1000);
    virtual ~MiioDevice();

    bool connect(MiioErrorHandlerFunction error = NULL);  //hello
    bool send(std::string method/*, params*/, MiioResponseHandlerFunction callback, MiioErrorHandlerFunction error = NULL);
    void disconnect();

    bool isConnected();
    bool isBusy(){ return ticker.active(); };
};


#endif
