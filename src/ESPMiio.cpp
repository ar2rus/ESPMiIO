#include "ESPMiio.h"

#include <Arduino.h>
#include "crypto_utils.h"
#include "byte_utils.h"

MiioToken::MiioToken(std::string token){
  if (token.empty()){
    memset(this->token, 0xFF, 16);
  }else{
    hex2bin(token.c_str(), this->token);
    //md5
    br_md5(this->token, 16, token_md5);
    //iv
    char arr[32];
    memcpy(&arr[0], token_md5, 16);  
    memcpy(&arr[16], this->token, 16);
    br_md5(arr, 32, token_iv);
  }
}

size_t MiioToken::encrypt(const char* msg, size_t msg_size, char* encoded_msg){
  if (msg && msg_size){
    //copy message
    memcpy(encoded_msg, msg, msg_size);
    //copy iv
    char iv[16];
    memcpy(iv, token_iv, 16);
    //padding
    int padSize = br_aes_ct_BLOCK_SIZE - (msg_size % br_aes_ct_BLOCK_SIZE);
    
    int len = msg_size + padSize;
    for (int i = msg_size; i < len; i++){
      encoded_msg[i] = padSize;
    }
    //encode
    br_aes_ct_cbcenc(token_md5, 16, iv, encoded_msg, len);
    return len;
  }
  return 0;
}

size_t MiioToken::decrypt(const char* msg, size_t msg_size, char* decoded_msg){
  if (msg && msg_size){
    //copy
    memcpy(decoded_msg, msg, msg_size);
    //copy iv
    char iv[16];
    memcpy(iv, token_iv, 16);
    //decode
    br_aes_ct_cbcdec(token_md5, 16, iv, decoded_msg, msg_size);
    //unpadding
    return msg_size - decoded_msg[msg_size-1];
  }
  return 0;
}

MiioMessage::MiioMessage(uint32_t unknownHeader, uint32_t deviceID, uint32_t timeStamp, uint16_t payloadID){
   this->unknownHeader = unknownHeader;
   this->deviceID = deviceID;
   this->timeStamp = timeStamp;
   this->payloadID = payloadID;
   this->valid = true;
}

MiioMessage::~MiioMessage(){
}

bool MiioMessage::testMessage(char* message, size_t message_len, MiioToken* token){
  if (!message){
    return false;
  }else{
    if (message_len < 0x20){
      Serial.println("Too short message");
      return false;
    } else {
      if (ntohs(*(uint16_t*)&message[0]) != HEADER){
        Serial.println("Header wrong");
        return false;
      }else{
        if (ntohs(*(uint16_t*)&message[2]) != message_len){
          Serial.println("Message len wrong");
          return false;
        }else{
          if (ntohl(*(uint32_t*)&message[4]) != HELLO_UNKNOWN && message_len != 0x20){
            char message_tmp[message_len];
            memcpy(message_tmp, message, message_len);
            if (token){
              memcpy(&message_tmp[16], token->getToken(), 16);
            }else{
              memset(&message_tmp[16], 0xFF, 16);
            }
            char md5[16];
            br_md5(message_tmp, message_len, md5);
            for (int i = 0; i < 16; i++){
              if (message[i + 16] != md5[i]){
                Serial.println("Wrong crc");
                return false;
              }
            }
          }
        }
      }
    }
  }
  return true;
}

MiioMessage::MiioMessage(char* message, size_t message_len, MiioToken* token){
  if (!testMessage(message, message_len, token)){
      Serial.println("Message testing failed");
      valid = false;
   }else{
      this->unknownHeader = ntohl(*((uint32_t*)&message[4]));
      this->deviceID = ntohl(*((uint32_t*)&message[8]));
      this->timeStamp = ntohl(*((uint32_t*)&message[12]));
      this->payloadID = 0;
      this->valid = true;
   }
}

char* MiioMessage::create(std::string payload, MiioToken* token, size_t &size){
  if (!valid){
    return NULL;
  }

  size_t pl_size = 0;
  char pl[payload.size() + 16]; //16 for padding
  if (!payload.empty()){
    pl_size = token->encrypt(payload.c_str(), payload.size(), pl);
  }

  //Serial.printf("SND: devID=%u; Ts=%u; pID=%u; p=%s\n", deviceID, timeStamp, (uint32_t)payloadID, payload.c_str());
  
  size = 32 + pl_size;
  char* message = new char[size];
  
  uint16_t _header = htons(HEADER);
  memcpy(&message[0], &_header, 2);
  uint16_t _size = htons(size);
  memcpy(&message[2], &_size, 2);
  uint32_t _unknownHeader = htonl(unknownHeader);
  memcpy(&message[4], &_unknownHeader, 4);
  uint32_t _deviceID = htonl(deviceID);
  memcpy(&message[8], &_deviceID, 4);
  uint32_t _timeStamp = htonl(timeStamp);
  memcpy(&message[12], &_timeStamp, 4);
  memcpy(&message[16], token->getToken(), 16);
  memcpy(&message[0x20], pl, pl_size);

  if (!isHello()){
    char digest[16];
    br_md5(message, size, digest);
    memcpy(&message[16], digest, 16);
  }
  
  return message;
}

MiioCommand::MiioCommand(): MiioMessage(HELLO_UNKNOWN, HELLO_DEVICE_ID, HELLO_TIME_STAMP, 0){
  this->method = "";
}

MiioCommand::MiioCommand(uint32_t deviceID, uint32_t timeStamp, uint16_t payloadID, std::string method, std::string params) :
  MiioMessage(NORMAL_UNKNOWN, deviceID, timeStamp, payloadID){
  this->method = method;
  this->params = params;
}


MiioCommand::~MiioCommand(){
}

char* MiioCommand::create(MiioToken* token, size_t &size){
  if (isHello()){
    return MiioMessage::create("", token, size);
  }

  DynamicJsonDocument doc(150);
  doc["id"] = MiioMessage::getPayloadID();
  if (method.empty()) return NULL;
  doc["method"] = method.c_str();
  StaticJsonDocument<50> params_doc;
  deserializeJson(params_doc, params);
  if (params_doc.is<JsonArray>()) {
    doc["params"] = params_doc.as<JsonArray>();
  } else {
    doc.createNestedArray("params").add(params_doc.as<JsonVariant>());
  }
//  int param = atoi(params.c_str());
//  if (param == 0) {
//    params_doc.add(params.c_str());
//  } else {
//    params_doc.add(param);
//  }
//  }

  char json[150];
  size_t json_size = serializeJson(doc, json, 150);
//  Serial.println(json);
  
  return MiioMessage::create(std::string(json, json_size), token, size);
}

MiioResponse::MiioResponse(char* message, size_t message_len, MiioToken* token): MiioMessage(message, message_len, token){
  if (token && MiioMessage::isValid() && message_len > 0x20){
    size_t payload_len = message_len - 0x20;
    decrypted_payload = new char[payload_len];
    size_t len = token->decrypt(&message[0x20], payload_len, decrypted_payload);
    if (len){
      DynamicJsonDocument doc(512);
      DeserializationError error = deserializeJson(doc, decrypted_payload, len);
      if (!error){
        this->payloadID = doc["id"];
        //Serial.printf("RCV: devID=%u; Ts=%u; pID=%u\n", getDeviceID(), getTimeStamp(), payloadID);

        JsonVariant r = doc["result"];
        if (!r.isNull()){
          if (r.is<JsonArray>()){
            r = r[0];
          }
          this->result = r;
        }else{
          this->error = doc["error"].as<JsonObject>();
        }
      }else{
        Serial.printf("Payload json parsing error: %s\n", error.c_str());
      }
    }
  }
}

bool MiioResponse::isValid(){
  return MiioMessage::isValid() && getDeviceID()!= -1 && getTimeStamp() != -1;
}

MiioResponse::~MiioResponse(){
  if (decrypted_payload){
    free(decrypted_payload);
  }
}

MiioDevice::MiioDevice(IPAddress* ip, std::string token, uint16_t timeout){
  this->ip = ip;
  
  this->deviceID = -1;
  this->timeStamp = -1;
  
  if (timeout < 10){
    timeout = 10;
  }
  this->timeout = timeout;

  this->token = new MiioToken(token);
}

bool MiioDevice::isConnected(){
  return ip && deviceID != -1 && timeStamp != -1;
}

void MiioDevice::disconnect(){
  deviceID = -1;
}

MiioDevice::~MiioDevice(){
 socket.close();
 if (token) {
    free(token);
    token = NULL;
 }
}

void MiioDevice::udp_rcv(AsyncUDPPacket packet, AuPacketHandlerFunction cb){
   udp_reset();
   if (cb) {
      cb(packet);
   }
}

bool MiioDevice::udp_send(char* message, size_t size, IPAddress *ip, AuPacketHandlerFunction cb, MiioErrorHandlerFunction error){
  udp_reset();

  bool sent;
  if (ip){
      sent = socket.writeTo((const uint8_t*)message, size, *ip, PORT);
  }else{
      sent = socket.broadcastTo((uint8_t*)message, size, PORT);
  }

  if (sent){
    ticker.once_ms_scheduled(timeout, std::bind(&MiioDevice::udp_timeout, this, error));
    socket.onPacket(std::bind(&MiioDevice::udp_rcv, this, std::placeholders::_1, cb));
  }
  return sent;
}

void MiioDevice::udp_reset(){
  socket.onPacket((AuPacketHandlerFunction)NULL);
  ticker.detach();
}

void MiioDevice::udp_timeout(MiioErrorHandlerFunction error){
  udp_reset();
  if (error){ error(MIIO_TIMEOUT); }
}

void MiioDevice::hello_rcv(AsyncUDPPacket packet, MiioErrorHandlerFunction error){
  if (packet.length() > 2){
    //copy to heap        
    char* data = new char[packet.length()];
    memcpy(data, packet.data(), packet.length());
  
    size_t length = ntohs(*(uint16_t*)&data[2]);
    MiioResponse* response = new MiioResponse(data, length, token);
    if (response->isValid()){
      deviceID = response->getDeviceID();
      timeStamp = response->getTimeStamp();
      payloadID = timeStamp & 0b1111111111111;
      //Serial.printf("HEL_RCV: deviceId = %u; timeStamp = %u; payloadID = %u\n", deviceID, timeStamp, (uint32_t)payloadID);

      if(!this->ip){
        this->ip = new IPAddress(packet.remoteIP());
      }
    }else{
      if (error){ error(MIIO_INVALID_RESPONSE); }
    }
    free(response);
    free(data);
  }else{
    if (error){ error(MIIO_INVALID_RESPONSE); }
  }
}

bool MiioDevice::connect(MiioErrorHandlerFunction error){
  if (isBusy()){
    if (error){ error(MIIO_BUSY); }
    return false;
  }
  
  MiioCommand hello;
  size_t size;
  char* msg = hello.create(token, size);
  if (msg){
    bool sent = udp_send(msg, size, ip, std::bind(&MiioDevice::hello_rcv, this, std::placeholders::_1, error), error);
    free(msg);
    return sent;
  } else {
    if (error){ error(MIIO_MESSAGE_CREATING); }
  }
  return false;
}

void MiioDevice::send_rcv(AsyncUDPPacket packet, MiioResponseHandlerFunction callback, MiioErrorHandlerFunction error){
  if (packet.length() > 2){
    //copy to heap        
    char* data = new char[packet.length()];
    memcpy(data, packet.data(), packet.length());
  
    size_t length = ntohs(*(uint16_t*)&data[2]);
    MiioResponse* response = new MiioResponse(data, length, token);

    if (response->isValid() && response->getPayloadID() == payloadID){
        timeStamp = response->getTimeStamp();
        uint16_t newPayloadID = timeStamp & 0b1111111111111;
        if (newPayloadID > payloadID){
          payloadID = newPayloadID;
        }
        if (callback){ 
          callback(*response);
        }
    }else{
      if (error){ error(MIIO_INVALID_RESPONSE); }
    }

    free(response);
    free(data);
  }else{
    if (error){ error(MIIO_INVALID_RESPONSE); }
  }
}

bool MiioDevice::send(std::string method, std::string params, MiioResponseHandlerFunction callback, MiioErrorHandlerFunction error){
  if (!isConnected()){
    if (error){ error(MIIO_NOT_CONNECTED); }
    return false;
  }

  if (isBusy()){
    if (error){ error(MIIO_BUSY); }
    return false;
  }
  
  if (payloadID >= 10000){
    payloadID = 0;
  }
  
  MiioCommand command(deviceID, ++timeStamp, ++payloadID, method, params);
  size_t size;
  char* msg = command.create(token, size);
  if (msg){
    bool sent = udp_send(msg, size, ip, std::bind(&MiioDevice::send_rcv, this, std::placeholders::_1, callback, error), error);
    free(msg);
    return sent;
  }else{
    if (error){ error(MIIO_MESSAGE_CREATING); }
  }
  return false;
}
