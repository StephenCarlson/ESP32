/*********
  Rui Santos
  Complete project details at https://randomnerdtutorials.com  
*********/


// Steve's Notes:
// http://shawnhymel.com/1882/how-to-create-a-web-server-with-websockets-using-an-esp32-in-arduino/
// https://github.com/MGX3D/EspDRO/blob/master/EspDRO.ino
// https://techtutorialsx.com/2017/11/03/esp32-arduino-websocket-server/





#include <WiFi.h>
// #include <Servo.h>

// Servo myservo;  // create servo object to control a servo
// twelve servo objects can be created on most boards

// GPIO the servo is attached to
// static const int servoPin = 13;

#define LEDC_TIMER_RESOLUTION  8
#define LEDC_BASE_FREQ     50000

#define LEDC_EN_DRV_BOOST   0
#define LEDC_PWM_BOOST      1
#define LEDC_EN_DRV_BUCK    2
#define LEDC_PWM_BUCK       3

#define PIN_LED             5
#define PIN_EN_AMPS         16
#define PIN_VDIV_REF        18
#define PIN_ADC_VREF        35
#define PIN_ADC_I           34
#define PIN_EN_DRV_BOOST    23
#define PIN_PWM_BOOST       19
#define PIN_EN_DRV_BUCK     22
#define PIN_PWM_BUCK        21

#define INCVAL              1


unsigned long lastTime = 0;
uint8_t ledState = 0;
int16_t ampsSetpoint = 0;
bool enableVref = false;
int16_t filtered_vref = 0;
int16_t filtered_amps = 0;
int16_t integrator = 0;

// Replace with your network credentials
const char* ssid     = "Xfinityandbeyond";
const char* password = "waynesoraven";

// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;

// Decode HTTP GET value
String valueString = String(5);
int pos1 = 0;
int pos2 = 0;

void setup() {
  Serial.begin(115200);

//  myservo.attach(servoPin);  // attaches the servo on the servoPin to the servo object
  // ledcSetup(LEDC_EN_DRV_BOOST, LEDC_BASE_FREQ, LEDC_TIMER_RESOLUTION);
  // ledcSetup(LEDC_PWM_BOOST, LEDC_BASE_FREQ, LEDC_TIMER_RESOLUTION);
  // ledcSetup(LEDC_EN_DRV_BUCK, LEDC_BASE_FREQ, LEDC_TIMER_RESOLUTION);
  ledcSetup(LEDC_PWM_BUCK, LEDC_BASE_FREQ, LEDC_TIMER_RESOLUTION);
  // ledcAttachPin(PIN_EN_DRV_BOOST, LEDC_EN_DRV_BOOST);
  // ledcAttachPin(PIN_PWM_BOOST, LEDC_PWM_BOOST);
  // ledcAttachPin(PIN_EN_DRV_BUCK, LEDC_EN_DRV_BUCK);
  ledcAttachPin(PIN_PWM_BUCK, LEDC_PWM_BUCK);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);


  pinMode(PIN_EN_AMPS, OUTPUT);
  digitalWrite(PIN_EN_AMPS, HIGH);
  pinMode(PIN_VDIV_REF, OUTPUT);
  digitalWrite(PIN_VDIV_REF, LOW); // HIGH

  pinMode(PIN_EN_DRV_BOOST, OUTPUT);
  digitalWrite(PIN_EN_DRV_BOOST, LOW);
  pinMode(PIN_PWM_BOOST, OUTPUT);
  digitalWrite(PIN_PWM_BOOST, LOW);

  pinMode(PIN_EN_DRV_BUCK, OUTPUT);
  digitalWrite(PIN_EN_DRV_BUCK, HIGH);

  analogSetSamples(4);
  // analogSetCycles(128);
  // analogSetClockDiv(64);
  // analogSetAttenuation(ADC_11db); // ADC_0db ADC_2_5db ADC_6db ADC_11db
  pinMode(PIN_ADC_I, INPUT);
  pinMode(PIN_ADC_VREF, INPUT);



  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();
}

void loop(){
  unsigned long localTimestamp = micros();
  if((localTimestamp-5000)>lastTime){
    lastTime = localTimestamp;
    ledState = (ledState)? 0 : 1;
    digitalWrite(PIN_LED, ledState);

    // Enable SyncFET (Disables Diode, hazardous)
    // digitalWrite(PIN_EN_DRV_BOOST, HIGH);
    // digitalWrite(PIN_PWM_BOOST, LOW);
    // delayMicroseconds(20);
    // digitalWrite(PIN_EN_DRV_BOOST, HIGH);
    // digitalWrite(PIN_PWM_BOOST, HIGH);

    uint16_t measured_vref = analogRead(PIN_ADC_VREF);
    uint16_t measured_amps = analogRead(PIN_ADC_I);

    // e(t) = r(t) - y(t) , u(t) = K_p*e(t)
    // uint16_t setpoint = (measured_vref > 2000)? 0 : 200;
    // int16_t error = setpoint - measured_amps;
    // int16_t control = (error<0)? 0 : 

    filtered_vref = filtered_vref + measured_vref/8 - filtered_vref/8;
    filtered_amps = filtered_amps + measured_amps/8 - filtered_amps/8;

    int16_t measured = (filtered_amps - filtered_vref); // 4096 worst case
    int16_t error    = ampsSetpoint - measured; // Setpoint is 2550 worst case
    integrator = integrator + error/16; 
    integrator = (integrator<-500)? -500 : (integrator>500)? 500 : integrator;
    int16_t control  = error*2 + integrator;
    uint8_t buckPwm  = (control<0)? 0 : (control>230)? 230 : control;

    // buckPwm = (ampsMeasured >= (ampsSetpoint*10))? \ 
    //   ( (buckPwm>(0  +INCVAL))? (buckPwm-INCVAL) : 0   ): \
    //   ( (buckPwm<(230-INCVAL))? (buckPwm+INCVAL) : 230 );



    ledcWrite(LEDC_PWM_BUCK, buckPwm);

    // if(ampsMeasured < 10) {
    //   digitalWrite(PIN_EN_DRV_BOOST, LOW);
    //   digitalWrite(PIN_PWM_BOOST, LOW);
    // }


    digitalWrite(PIN_VDIV_REF, ((enableVref)? HIGH : LOW) );


    Serial.print("vref,amps,meas,set,intg,buck:");
    Serial.print(measured_vref);
    Serial.print(",");
    Serial.print(measured_amps);
    Serial.print(",");
    Serial.print(measured);
    Serial.print(",");
    Serial.print(ampsSetpoint);
    Serial.print(",");
    Serial.print(integrator);
    Serial.print(",");
    Serial.println(buckPwm);
  }
  
  
  WiFiClient client = server.available();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons 
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>body { text-align: center; font-family: \"Trebuchet MS\", Arial; margin-left:auto; margin-right:auto;}");
            client.println(".slider { width: 300px; }</style>");
            client.println("<script src=\"https://ajax.googleapis.com/ajax/libs/jquery/3.3.1/jquery.min.js\"></script>");
                     
            // Web Page
            client.println("</head><body><h1>ESP32 PWM Slider</h1>");
            client.println("<p>PWM1 Value: <span id=\"servoPos\"></span></p>");          
            client.println("<input type=\"range\" min=\"0\" max=\"255\" class=\"slider\" id=\"servoSlider\" onchange=\"servo(this.value)\" value=\""+valueString+"\"/>");
            client.println("<p>PWM2 Value: <span id=\"servo2Pos\"></span></p>");          
            client.println("<input type=\"range\" min=\"0\" max=\"255\" class=\"slider\" id=\"servo2Slider\" onchange=\"servo2(this.value)\" value=\""+valueString+"\"/>");
            
            client.println("<script>var slider = document.getElementById(\"servoSlider\");");
            client.println("var slider2 = document.getElementById(\"servo2Slider\");");
            client.println("var servoP = document.getElementById(\"servoPos\"); servoP.innerHTML = slider.value;");
            client.println("var servo2P = document.getElementById(\"servo2Pos\"); servo2P.innerHTML = slider2.value;");
            client.println("slider.oninput = function() { slider.value = this.value; servoP.innerHTML = this.value; }");
            client.println("slider2.oninput = function() { slider2.value = this.value; servo2P.innerHTML = this.value; }");
            client.println("$.ajaxSetup({timeout:1000});");
            client.println(" function servo(pos) { $.get(\"/?value=\" + pos + \"&\"); {Connection: close};}");
            client.println(" function servo2(pos) { $.get(\"/?value2=\" + pos + \"&\"); {Connection: close};}</script>");
           
            client.println("</body></html>");     
            
            //GET /?value=180& HTTP/1.1
            if(header.indexOf("GET /?value=")>=0) {
              pos1 = header.indexOf('=');
              pos2 = header.indexOf('&');
              valueString = header.substring(pos1+1, pos2);
              
              //Rotate the servo
//              myservo.write(valueString.toInt());
              // ledcWrite(LEDC_PWM_BOOST, valueString.toInt());
              ampsSetpoint = (10 * valueString.toInt());
              Serial.println(valueString); 
            }         

            //GET /?value=180& HTTP/1.1
            if(header.indexOf("GET /?value2=")>=0) {
              pos1 = header.indexOf('=');
              pos2 = header.indexOf('&');
              valueString = header.substring(pos1+1, pos2);
              
              //Rotate the servo
//              myservo.write(valueString.toInt());
              // ledcWrite(LEDC_EN_DRV_BOOST, valueString.toInt());
              enableVref = (valueString.toInt() >= 127);
              Serial.println(valueString); 
            }   
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}

