// check the structure from here http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html
void MQTT_CONNECT(String mqtt_clentID, uint8_t mqtt_alive_time, String mqtt_username, String mqtt_password) {
  MQTTConnectHeader[11] = mqtt_alive_time;
  MQTTConnectHeader[1] = 10 + 2 + mqtt_clentID.length() + 2 + mqtt_username.length() + 2 + mqtt_password.length();

  int i;
  for (i = 0; i < 12; i++) {
    pConnectRequest[i] = MQTTConnectHeader[i];
  }
  pConnectRequest[12] = 0;
  pConnectRequest[13] = mqtt_clentID.length();

  for (i = 0; i < mqtt_clentID.length(); i++) {
    pConnectRequest[i + 12 + 2] = (uint8_t)mqtt_clentID.charAt(i);
  }
  pConnectRequest[12 + 2 + mqtt_clentID.length()] = 0;
  pConnectRequest[12 + 2 + mqtt_clentID.length() + 1] = mqtt_username.length();
  for (i = 0; i < mqtt_username.length(); i++) {
    pConnectRequest[i + 12 + 2 + mqtt_clentID.length() + 2] = (uint8_t)mqtt_username.charAt(i);
  }
  pConnectRequest[12 + 2 + mqtt_clentID.length() + 2 + mqtt_username.length()] = 0;
  pConnectRequest[12 + 2 + mqtt_clentID.length() + 2 + mqtt_username.length() + 1] = mqtt_password.length();
  for (i = 0; i < mqtt_password.length(); i++) {
    pConnectRequest[i + 12 + 2 + mqtt_clentID.length() + 2 + mqtt_username.length() + 2] = (uint8_t)mqtt_password.charAt(i);
  }

  webSocket.sendBIN(pConnectRequest, MQTTConnectHeader[1] + 2);
}

byte stack_exe_used = 0;
#define STACK_EXE_DEPTH 6
String Stack_Execute[2][STACK_EXE_DEPTH];
//bool Event_Stack_Executed = false;
void Publish_Stach_Push(String str_topic, String str_payload) {
  if (WS_MQTT_status == MQTT_CONNECTED && str_topic.length() < 64 && str_payload.length() < 256) {
    for (byte i = STACK_EXE_DEPTH - 1; i >= 0; i--) {
      if (Stack_Execute[0][i].length() == 0 && Stack_Execute[1][i].length() == 0) {
        Stack_Execute[0][i] = str_topic;
        Stack_Execute[1][i] = str_payload;
        stack_exe_used++;
        break;
      }
    }
  }
}

void Publish_Stack_Pop() {

  if ((Stack_Execute[0][STACK_EXE_DEPTH - 1].length() > 0 && Stack_Execute[1][STACK_EXE_DEPTH - 1].length() > 0)) {
    SERIAL_DEBUG("stack topic is: %s, stack payload is : %s\n", Stack_Execute[0][STACK_EXE_DEPTH - 1].c_str(), Stack_Execute[1][STACK_EXE_DEPTH - 1].c_str());

    /*
       subscribe topic length should < 64
    */
    if (Stack_Execute[1][STACK_EXE_DEPTH - 1] == "SUBSCRIBE") {
      
      pSubscribeRequest[0] = 130;
      pSubscribeRequest[1] = 5 + Stack_Execute[0][STACK_EXE_DEPTH - 1].length();
      pSubscribeRequest[2] = 0;
      pSubscribeRequest[3] = MQTT_package_id;
      pSubscribeRequest[4] = 0;
      //subscribe topic.length < 64
      pSubscribeRequest[5] = Stack_Execute[0][STACK_EXE_DEPTH - 1].length();

      for (int i = 0; i < Stack_Execute[0][STACK_EXE_DEPTH - 1].length(); i++) {
        pSubscribeRequest[i + 6] = (uint8_t)Stack_Execute[0][STACK_EXE_DEPTH - 1].charAt(i);
      }

      pSubscribeRequest[Stack_Execute[0][STACK_EXE_DEPTH - 1].length() + 6] = 0;//QOS=0
      SERIAL_DEBUG("sendBIN PREPARED:\n");
      webSocket.sendBIN(pSubscribeRequest, Stack_Execute[0][STACK_EXE_DEPTH - 1].length() + 6 + 1);

      //MQTT_package_id++;
    }

    /*
       unsubscribe topic length should < 64
    */
    else if (Stack_Execute[1][STACK_EXE_DEPTH - 1] == "UNSUBSCRIBE") {

      pUnSubscribeRequest[0] = 162;
      pUnSubscribeRequest[1] = 4 + Stack_Execute[0][STACK_EXE_DEPTH - 1].length();
      pUnSubscribeRequest[2] = 0;
      pUnSubscribeRequest[3] = MQTT_package_id;
      pUnSubscribeRequest[4] = 0;
      //unsubscribe topic.length < 64
      pUnSubscribeRequest[5] = Stack_Execute[0][STACK_EXE_DEPTH - 1].length();

      for (int i = 0; i < Stack_Execute[0][STACK_EXE_DEPTH - 1].length(); i++) {
        pUnSubscribeRequest[i + 6] = (uint8_t)Stack_Execute[0][STACK_EXE_DEPTH - 1].charAt(i);
      }

      SERIAL_DEBUG("sendBIN PREPARED\n");
      webSocket.sendBIN(pUnSubscribeRequest, Stack_Execute[0][STACK_EXE_DEPTH - 1].length() + 6);

      MQTT_package_id++;
    }
    /*
       publish
    */
    else {

      if (Stack_Execute[0][STACK_EXE_DEPTH - 1].length() + Stack_Execute[1][STACK_EXE_DEPTH - 1].length() + 2 > 127) {
        
        pPublishRequest[0] = 48;
        pPublishRequest[1] = (uint8_t)(Stack_Execute[0][STACK_EXE_DEPTH - 1].length() + Stack_Execute[1][STACK_EXE_DEPTH - 1].length() + 2) - ((Stack_Execute[0][STACK_EXE_DEPTH - 1].length() + Stack_Execute[1][STACK_EXE_DEPTH - 1].length() + 2) / 128 - 1) * 128;
        pPublishRequest[2] = (uint8_t)(Stack_Execute[0][STACK_EXE_DEPTH - 1].length() + Stack_Execute[1][STACK_EXE_DEPTH - 1].length() + 2) / 128;
        pPublishRequest[3] = 0;
        pPublishRequest[4] = (uint8_t)Stack_Execute[0][STACK_EXE_DEPTH - 1].length();
        for (int i = 0; i < Stack_Execute[0][STACK_EXE_DEPTH - 1].length(); i++) {
          pPublishRequest[i + 5] = (uint8_t)Stack_Execute[0][STACK_EXE_DEPTH - 1].charAt(i);
        }
        for (int i = 0; i < Stack_Execute[1][STACK_EXE_DEPTH - 1].length(); i++) {
          pPublishRequest[i + 5 + Stack_Execute[0][STACK_EXE_DEPTH - 1].length()] = (uint8_t)Stack_Execute[1][STACK_EXE_DEPTH - 1].charAt(i);
        }
        SERIAL_DEBUG("sendBIN PREPARED\n");
        webSocket.sendBIN(pPublishRequest, Stack_Execute[0][STACK_EXE_DEPTH - 1].length() + Stack_Execute[1][STACK_EXE_DEPTH - 1].length() + 5);
      }
      else {
        
        pPublishRequest[0] = 48;
        pPublishRequest[1] = (uint8_t)Stack_Execute[0][STACK_EXE_DEPTH - 1].length() + Stack_Execute[1][STACK_EXE_DEPTH - 1].length() + 2;
        pPublishRequest[2] = 0;
        pPublishRequest[3] = (uint8_t)Stack_Execute[0][STACK_EXE_DEPTH - 1].length();
        for (int i = 0; i < Stack_Execute[0][STACK_EXE_DEPTH - 1].length(); i++) {
          pPublishRequest[i + 4] = (uint8_t)Stack_Execute[0][STACK_EXE_DEPTH - 1].charAt(i);
        }
        for (int i = 0; i < Stack_Execute[1][STACK_EXE_DEPTH - 1].length(); i++) {
          pPublishRequest[i + 4 + Stack_Execute[0][STACK_EXE_DEPTH - 1].length()] = (uint8_t)Stack_Execute[1][STACK_EXE_DEPTH - 1].charAt(i);
        }
        SERIAL_DEBUG("sendBIN PREPARED\n");
        webSocket.sendBIN(pPublishRequest, Stack_Execute[0][STACK_EXE_DEPTH - 1].length() + Stack_Execute[1][STACK_EXE_DEPTH - 1].length() + 4);
      }
    }
    for (byte i = STACK_EXE_DEPTH - 1; i > 0; i--) {
      Stack_Execute[0][i] = Stack_Execute[0][i - 1];
      Stack_Execute[1][i] = Stack_Execute[1][i - 1];
    }
    Stack_Execute[0][0] = "";
    Stack_Execute[1][0] = "";
    stack_exe_used--;
  }
}


