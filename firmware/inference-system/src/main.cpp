#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "DHT.h"
#include <EloquentTinyML.h>
#include "model.h" 
#include "wifi_secrets.h"

// Wi-Fi configuration
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

WebServer server(80);

// Sensor and model configuration
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define NUM_INPUTS 2
#define NUM_OUTPUTS 4 
#define TENSOR_ARENA_SIZE 4 * 1024 

Eloquent::TinyML::TfLite<NUM_INPUTS, NUM_OUTPUTS, TENSOR_ARENA_SIZE> ml;

#ifndef RGB_BUILTIN
#define RGB_BUILTIN 48 
#endif

// Keep normalization values aligned with training output.
const float TEMP_MIN = 20.0;   
const float TEMP_RANGE = 15.0; 
const float HUM_MIN = 40.0;    
const float HUM_RANGE = 45.0;  

String classNames[] = {
    "Temperature: High, Humidity: High", 
    "Temperature: Low, Humidity: Low", 
    "Temperature: Low, Humidity: High", 
    "Temperature: High, Humidity: Low"
}; 
// Global values served by the web endpoint.
float currentTemp = 0.0;
float currentHum = 0.0;
String currentPrediction = "Initializing...";
float currentConfidence = 0.0;

// Read sensor periodically without blocking the event loop.
unsigned long lastSensorRead = 0;
const unsigned long READ_INTERVAL = 2000;

// Web page stored in flash memory.
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>ESP32 TinyML Dashboard</title>
    <style>
        body { background-color: #121212; color: #ffffff; font-family: 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; display: flex; flex-direction: column; align-items: center; justify-content: center; height: 100vh; margin: 0; }
        .card { background-color: #1e1e1e; border-radius: 15px; padding: 30px; box-shadow: 0 10px 30px rgba(0,0,0,0.5); text-align: center; max-width: 400px; width: 90%; }
        h1 { font-size: 1.5rem; color: #bb86fc; margin-top: 0; margin-bottom: 25px; }
        .data-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; margin-bottom: 25px; }
        .data-box { background-color: #2d2d2d; padding: 20px 10px; border-radius: 10px; }
        .data-value { font-size: 2.2rem; font-weight: bold; color: #03dac6; margin-bottom: 5px; }
        .data-label { font-size: 0.85rem; color: #aaaaaa; text-transform: uppercase; letter-spacing: 1px; }
        .prediction-box { background-color: #3700b3; padding: 20px; border-radius: 10px; margin-bottom: 10px; }
        .prediction-text { font-size: 1.1rem; font-weight: bold; margin-bottom: 8px; line-height: 1.4; }
        .confidence { font-size: 0.9rem; color: #cfb8f8; }
    </style>
</head>
<body>
    <div class="card">
        <h1>Environment Monitor</h1>
        
        <div class="data-grid">
            <div class="data-box">
                <div class="data-value" id="tempValue">--</div>
                <div class="data-label">Temp &deg;C</div>
            </div>
            <div class="data-box">
                <div class="data-value" id="humValue">--</div>
                <div class="data-label">Humidity %</div>
            </div>
        </div>

        <div class="prediction-box">
            <div class="prediction-text" id="predValue">Waiting for sensor data...</div>
            <div class="confidence" id="confValue">Confidence: --%</div>
        </div>
    </div>

    <script>
        // Fetch new data from the ESP32 every 2 seconds without reloading the page
        setInterval(() => {
            fetch('/data')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('tempValue').innerText = data.temp.toFixed(1);
                    document.getElementById('humValue').innerText = data.hum.toFixed(1);
                    document.getElementById('predValue').innerText = data.prediction;
                    document.getElementById('confValue').innerText = "Confidence: " + data.confidence.toFixed(1) + "%";
                })
                .catch(err => console.error("Error fetching data: ", err));
        }, 2000);
    </script>
</body>
</html>
)rawliteral";


// Function prototypes
bool readSensorData(float &temperature, float &humidity);
void preprocessData(float temp, float hum, float* inputArray);
void runInference(float* inputArray, float* outputArray);
int getWinningClass(float* outputArray, float &highestConfidence);
void updateLED(int classIndex);
void handleRoot();
void handleData();


void setup() {
    Serial.begin(115200);
    dht.begin();
    neopixelWrite(RGB_BUILTIN, 0, 0, 0);

    // 1. Connect to Wi-Fi
    Serial.print("Connecting to Wi-Fi");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected!");
    Serial.print("Open this IP in your browser: http://");
    Serial.println(WiFi.localIP());

    // 2. Start the model
    ml.begin((unsigned char*)model_tflite);
    Serial.println("Model started successfully.");

    // 3. Setup Web Server Routes
    server.on("/", handleRoot); // Serves the HTML page
    server.on("/data", handleData); // Serves the live JSON data
    server.begin();
}

void loop() {
    // Keep listening for web browsers requesting the page/data
    server.handleClient();

    unsigned long currentMillis = millis();
    
    // Only read the sensor every 2 seconds, don't block the rest of the code!
    if (currentMillis - lastSensorRead >= READ_INTERVAL) {
        lastSensorRead = currentMillis;

        float temperature, humidity;
        
        if (!readSensorData(temperature, humidity)) {
            Serial.println("Failed to read from DHT sensor!");
            return; 
        }

        float mlInputs[NUM_INPUTS];
        preprocessData(temperature, humidity, mlInputs);

        float mlOutputs[NUM_OUTPUTS];
        runInference(mlInputs, mlOutputs);

        float confidence;
        int winningClass = getWinningClass(mlOutputs, confidence);

        // Update the global variables so the web server can serve the latest info
        currentTemp = temperature;
        currentHum = humidity;
        currentPrediction = classNames[winningClass];
        currentConfidence = confidence;

        updateLED(winningClass);
    }
}


bool readSensorData(float &temperature, float &humidity) {
    humidity = dht.readHumidity();
    temperature = dht.readTemperature();
    return !(isnan(humidity) || isnan(temperature));
}

void preprocessData(float temp, float hum, float* inputArray) {
    inputArray[0] = (temp - TEMP_MIN) / TEMP_RANGE;
    inputArray[1] = (hum - HUM_MIN) / HUM_RANGE;
}

void runInference(float* inputArray, float* outputArray) {
    ml.predict(inputArray, outputArray);
}

int getWinningClass(float* outputArray, float &highestConfidence) {
    int winningIndex = 0;
    highestConfidence = 0.0;
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        if (outputArray[i] > highestConfidence) {
            highestConfidence = outputArray[i];
            winningIndex = i;
        }
    }
    return winningIndex;
}

void updateLED(int classIndex) {
    switch(classIndex) {
        case 0: neopixelWrite(RGB_BUILTIN, 64, 0, 0); break;
        case 1: neopixelWrite(RGB_BUILTIN, 0, 0, 64); break;
        case 2: neopixelWrite(RGB_BUILTIN, 64, 64, 0); break;
        case 3: neopixelWrite(RGB_BUILTIN, 0, 64, 0); break;
        default: neopixelWrite(RGB_BUILTIN, 0, 0, 0); break;
    }
}

void handleRoot() {
    server.send(200, "text/html", index_html);
}

void handleData() {
    String json = "{";
    json += "\"temp\":" + String(currentTemp) + ",";
    json += "\"hum\":" + String(currentHum) + ",";
    json += "\"prediction\":\"" + currentPrediction + "\",";
    json += "\"confidence\":" + String(currentConfidence * 100.0);
    json += "}";
    
    server.send(200, "application/json", json);
}
