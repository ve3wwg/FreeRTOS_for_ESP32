// eventgr.ino

// Update with your WIFI credentials
#define WIFI_SSID     "MySSID"
#define WIFI_PASSWD   "MyPassword"

// LED GPIOs:
#define GPIO_LED1     18
#define GPIO_LED2     19
#define GPIO_LED3     21

#define N_LED         3

#include <WiFi.h>

#define WIFI_RDY      0b0001
#define LED_CHG       0b0010

static EventGroupHandle_t hevt;
static WiFiServer http(80);
static WiFiUDP udp;

static int leds[N_LED] =
  { GPIO_LED1, GPIO_LED2, GPIO_LED3 };

static char const
  *ssid = WIFI_SSID,
  *passwd = WIFI_PASSWD;

static bool getline(String& s,WiFiClient client) {
  char ch;
  bool flag = false;

  s.clear();
  while ( client.connected() ) {
    if ( client.available() ) {
      ch = client.read();
      flag = true;

      if ( ch == '\r' )
        continue;         // Ignore CR
      if ( ch == '\n' )
        break;
      s += ch;
    } else {
      taskYIELD();
    }
  }
  return client.connected() && flag;
}

static void http_server(void *arg) {

  xEventGroupWaitBits(
    hevt,           // Event group
    WIFI_RDY,       // bits to wait for
    pdFALSE,        // no clear
    pdFALSE,        // wait for all bits
    portMAX_DELAY); // timeout

  auto subnet = WiFi.subnetMask();
  printf("Server ready: %s port 80\n",
    WiFi.localIP().toString().c_str());

  for (;;) {
    WiFiClient client = http.available();

    if ( client ) {
      // A client has connected:
      String line, header;
      bool gothdrf = false;

      printf("New client connect from %s\n",
        client.remoteIP().toString().c_str());

      while ( client.connected() ) {
        if ( getline(header,client) ) {
          while ( getline(line,client) && line.length() > 0 )
            ;
        }
        if ( !client.connected() )
          break;

        client.println("HTTP/1.1 200 OK");
        client.println("Content-type:text/html");
        client.println("Connection: close");
        client.println();
            
        if ( !strncmp(header.c_str(),"GET /led",8) ) {
          const char *cp = header.c_str() + 8;
          bool changedf = false;

          if ( cp[0] >= '0' && cp[0] <= ('0'+N_LED) ) {
            int ledx = uint8_t(cp[0]) - uint8_t('0');

            if ( cp[1] == '/'
              && ( cp[2] == '0' || cp[2] == '1' ) ) {
                bool onoff = !!(cp[2] & 1);
              printf("LED%d = %d\n",ledx,onoff);
              if ( onoff != !!digitalRead(leds[ledx]) ) {
                digitalWrite(leds[ledx],onoff?HIGH:LOW);
                changedf = true;
              }
            }
          }
          if ( changedf )
            xEventGroupSetBits(hevt,LED_CHG);
        }

        client.println("<!DOCTYPE html><html>");
        client.println("<head><meta name=\"viewport\" "
          "content=\"width=device-width, initial-scale=1\">");
        client.println("<link rel=\"icon\" href=\"data:,\">");
        client.println("<style>html { font-family: Helvetica; "
          "display: inline-block; margin: 0px auto; "
          "text-align: center;}");
        client.println(".button { background-color: "
          "#4CAF50; border: none; color: white; "
          "padding: 16px 40px;");
        client.println("text-decoration: none; "
          "font-size: 30px; margin: 2px; cursor: pointer;}");
        client.println(".button2 {background-color: #555555;}"
          "</style></head>");
        client.println("<body><h1>ESP32 Event Groups "
          "(eventgr.ino)</h1>");
            
        for ( int x=0; x<N_LED; ++x ) {
          bool state = !!digitalRead(leds[x]);
          char temp[32];

          snprintf(temp,sizeof temp,"<p>LED%d - State ",x);
          client.println(temp);
          client.println(String(state ? "on" : "off") + "</p>");
          client.println("<p><a href=\"");
          snprintf(temp,sizeof temp,"/led%d/%d",x,!state);
          client.println(temp);
          client.println("\"><button class=\"button\">");
          client.println(state?"OFF":"ON");
          client.println("</button></a></p>");
        }
               
        client.println("</body></html>");
        client.println();
            break;
      }
      client.stop();
      header = "";
      Serial.println("Client disconnected.");
      Serial.println("");
    }
  }
}

static void udp_broadcast(void *arg) {

  xEventGroupWaitBits(
    hevt,           // Event Group
    WIFI_RDY,       // bits to wait for
    pdFALSE,        // no clear
    pdFALSE,        // wait for all bits
    portMAX_DELAY); // timeout

  // Determine IPv4 broadcast address:

  auto localip = WiFi.localIP();
  auto subnet = WiFi.subnetMask();
  auto broadcast = localip;

  for ( short x=0; x<4; ++x ) {
    broadcast[x] = 0xFF & ~(subnet[x]);
    broadcast[x] |= localip[x] & subnet[x];
  }

  printf("UDP ready: netmask %s broadcast %s\n",
    subnet.toString().c_str(),
    broadcast.toString().c_str()
  );

  // Send "Ready:\n"
  udp.beginPacket(broadcast,9000);
  udp.write((uint8_t const*)"Ready:\n",7);
  udp.endPacket();

  for (;;) {
    xEventGroupWaitBits(
      hevt,           // handle
      LED_CHG,        // bits to wait for
      pdTRUE,         // clear bits
      pdFALSE,        // wait for all bits
      portMAX_DELAY); // timeout
    char temp[16];
    
    // Send UDP packet:
    udp.beginPacket(broadcast,9000);
    for ( short x=0; x<N_LED; ++x ) {
      bool state = !!digitalRead(leds[x]);
      snprintf(temp,sizeof temp,
        "LED%d=%d\n",
        x,state);
      udp.write((uint8_t const*)temp,strlen(temp));
    }
    udp.write((uint8_t const*)"--\n",3);
    udp.endPacket();
  }
}

static void init_http() {
  unsigned count = 0;

  printf("WiFi connecting to SSID: %s\n",ssid);
  WiFi.begin(ssid,passwd);

  while ( WiFi.status() != WL_CONNECTED ) {
    delay(250);
    if ( ++count < 80 )
      printf(".");
    else {
      printf("\n");
      count = 0;
    }
  }

  printf("\nWiFi connected as %s\n",
    WiFi.localIP().toString().c_str());
  http.begin();

  xEventGroupSetBits(hevt,WIFI_RDY);
}

void setup() {
  int app_cpu = xPortGetCoreID();
  BaseType_t rc;

  // Configure LED GPIOs
  for ( int x=0; x<N_LED; ++x ) {
    pinMode(leds[x],OUTPUT);
    digitalWrite(leds[x],LOW);
  }

  // Create Event Group
  hevt = xEventGroupCreate();
  assert(hevt);

  // Allow USB to connect
  delay(2000);
  printf("\neventgr.ino:\n");

  // HTTP Server Task
  rc = xTaskCreatePinnedToCore(
    http_server, // function
    "http",   // Name
    2100,     // Stack size
    nullptr,  // Parameters
    1,        // Priority
    nullptr,  // handle
    app_cpu   // CPU
  );
  assert(rc == pdPASS);

  // UDP Broadcast Task
  rc = xTaskCreatePinnedToCore(
    udp_broadcast, // function
    "udp",    // Name
    2100,     // Stack size
    nullptr,  // Parameters
    1,        // Priority
    nullptr,  // handle
    app_cpu   // CPU
  );
  assert(rc == pdPASS);

  // Start WiFi
  init_http();
}

void loop() {
  // Not used:
  vTaskDelete(nullptr);
}
